#include "config_parser.hpp"

#include <algorithm>
#include <limits>
#include <map>
#include <utility>
#include <vector>

#include "boost/foreach.hpp"
#include "boost/lexical_cast.hpp"
#include "boost/numeric/conversion/cast.hpp"
#include "boost/property_tree/info_parser.hpp"
#include "boost/property_tree/ptree.hpp"

#include "dsb/sequence.hpp"


namespace
{
    // Parses an INFO file as a property tree.
    boost::property_tree::ptree ReadPtreeInfoFile(const std::string& path)
    {
        boost::property_tree::ptree pt;
        boost::property_tree::read_info(path, pt);
        return pt;
    }
}


namespace
{
    std::pair<std::string, std::string> SplitVarSpec(const std::string& varSpec)
    {
        const auto dotPos = varSpec.find_first_of('.');
        if (dotPos >= varSpec.size() - 1) {
            throw std::runtime_error(
                "Invalid variable identifier (should be on the format \"slave.var\"): "
                + varSpec);
        }
        return std::make_pair(
            varSpec.substr(0, dotPos),
            varSpec.substr(dotPos + 1));
    }


    std::multimap<std::string, dsb::domain::Controller::SlaveType> SlaveTypesByName(
        dsb::domain::Controller& domain)
    {
        std::multimap<std::string, dsb::domain::Controller::SlaveType> types;
        BOOST_FOREACH (const auto& st, domain.GetSlaveTypes()) {
            types.insert(std::make_pair(st.name, st));
        }
        return types;
    }


    dsb::model::ScalarValue ParseVariableValue(
        const dsb::model::Variable& variableDefinition,
        const boost::property_tree::ptree& valueNode)
    {
        try {
            switch (variableDefinition.DataType()) {
                case dsb::model::REAL_DATATYPE:
                    return valueNode.get_value<double>();
                case dsb::model::INTEGER_DATATYPE:
                    return valueNode.get_value<int>();
                case dsb::model::BOOLEAN_DATATYPE:
                    return valueNode.get_value<bool>();
                case dsb::model::STRING_DATATYPE:
                    return valueNode.get_value<std::string>();
                default:
                    assert (false);
                    return dsb::model::ScalarValue();
            }
        } catch (const std::runtime_error& e) {
            throw std::runtime_error(
                "Invalid value for variable '" + variableDefinition.Name()
                + "': " + valueNode.get_value<std::string>()
                + " (" + e.what() + ")");
        }
    }

    struct SlaveInstance
    {
        dsb::model::SlaveID id;
        const dsb::domain::Controller::SlaveType* slaveType;
    };

    const SlaveInstance& FindSlaveInstance(
        const std::map<std::string, SlaveInstance>& slaves,
        const std::string& slaveName)
    {
        const auto slaveIt = slaves.find(slaveName);
        if (slaveIt == slaves.end()) {
            throw std::runtime_error("Unknown slave: " + slaveName);
        }
        return slaveIt->second;
    }


    const dsb::model::Variable& FindVariable(
        const SlaveInstance& slaveInstance,
        const std::string& varName)
    {
        const auto& variables = slaveInstance.slaveType->variables;
        const auto varIt = std::find_if(
            variables.begin(),
            variables.end(),
            [&](const dsb::model::Variable& varDef) { return varDef.Name() == varName; });
        if (varIt == variables.end()) {
            throw std::runtime_error(
                "Variable not found: " + varName);
        }
        return *varIt;
    }
}


void ParseSystemConfig(
    const std::string& path,
    dsb::domain::Controller& domain,
    dsb::execution::Controller& execution,
    const dsb::execution::Locator& executionLocator)
{
    const auto ptree = ReadPtreeInfoFile(path);

    const auto slaveTypes = SlaveTypesByName(domain);

    std::map<std::string, SlaveInstance> slaves;
    std::map<dsb::model::SlaveID, std::vector<dsb::model::VariableValue>> variables;

    // NOTE: For some reason, if we put the get_child() call inside the
    // BOOST_FOREACH macro invocation, we get a segfault if the child node
    // does not exist.
    const auto slaveTree = ptree.get_child("slaves", boost::property_tree::ptree());
    BOOST_FOREACH (const auto& slaveNode, slaveTree) {
        const auto slaveName = slaveNode.first;
        if (slaves.count(slaveName)) {
            throw std::runtime_error(
                "Configuration file contains two slaves with the name '"
                + slaveName + "'");
        }
        const auto slaveData = slaveNode.second;
        const auto slaveTypeName = slaveData.get<std::string>("type");
        const auto typeCount = slaveTypes.count(slaveTypeName);
        if (typeCount == 0) {
            throw std::runtime_error(
                "Slave type not found in domain: " + slaveTypeName);
        } else if (typeCount > 1) {
            throw std::runtime_error(
                "Two or more slave types with the same name found in domain: " + slaveTypeName);
        }
        const auto& slaveType = slaveTypes.find(slaveTypeName)->second;
        
        // Assign a slave ID
        const auto slaveID = boost::numeric_cast<uint16_t>(slaves.size() + 1);
        SlaveInstance slaveInstance = { slaveID, &slaveType };
        slaves[slaveName] = slaveInstance;

        const auto initTree = slaveData.get_child("init", boost::property_tree::ptree());
        BOOST_FOREACH (const auto& initNode, initTree) {
            const auto varName = initNode.first;
            const auto varDefIt = std::find_if(
                slaveType.variables.begin(),
                slaveType.variables.end(),
                [&](const dsb::model::Variable& varDef) { return varDef.Name() == varName; });
            if (varDefIt == slaveType.variables.end()) {
                throw std::runtime_error(
                    "Variable '" + varName + "' not found in slave type '"
                    + slaveTypeName + "'");
            }
            dsb::model::VariableValue v;
            v.id = varDefIt->ID();
            v.value = ParseVariableValue(*varDefIt, initNode.second);
            variables[slaveID].push_back(v);
        }
    }

    std::map<dsb::model::SlaveID, std::vector<dsb::model::VariableConnection>> connections;
    const auto connTree = ptree.get_child("connections", boost::property_tree::ptree());
    BOOST_FOREACH (const auto& connNode, connTree) {
        const auto inputSpec = SplitVarSpec(connNode.first);
        const auto outputSpec = SplitVarSpec(connNode.second.data());
        try {
            const auto& inputSlave = FindSlaveInstance(slaves, inputSpec.first);
            const auto& outputSlave = FindSlaveInstance(slaves, outputSpec.first);
            const auto& inputVar = FindVariable(inputSlave, inputSpec.second);
            const auto& outputVar = FindVariable(outputSlave, outputSpec.second);
            if (inputVar.DataType() != outputVar.DataType()) {
                throw std::runtime_error("Incompatible data types");
            }
            dsb::model::VariableConnection vc;
            vc.inputId = inputVar.ID();
            vc.otherSlaveId = outputSlave.id;
            vc.otherOutputId = outputVar.ID();
            connections[inputSlave.id].push_back(vc);
        } catch (const std::runtime_error& e) {
            throw std::runtime_error("In connection between "
                + connNode.first + " and " + connNode.second.data() + ": "
                + e.what());
        }
    }

    BOOST_FOREACH (const auto& slave, slaves) {
        execution.AddSlave(slave.second.id);
    }
    BOOST_FOREACH (auto& slaveVars, variables) {
        execution.SetVariables(
            slaveVars.first,
            dsb::sequence::ElementsOf(slaveVars.second));
    }
    BOOST_FOREACH (auto& conn, connections) {
        execution.ConnectVariables(
            conn.first,
            dsb::sequence::ElementsOf(conn.second));
    }
    BOOST_FOREACH (const auto& slave, slaves) {
        domain.InstantiateSlave(
            slave.second.slaveType->uuid,
            executionLocator,
            slave.second.id);
    }
}


ExecutionConfig::ExecutionConfig()
    : startTime(0.0),
      stopTime(std::numeric_limits<double>::infinity()),
      stepSize(1.0)
{
}


ExecutionConfig ParseExecutionConfig(const std::string& path)
{
    const auto ptree = ReadPtreeInfoFile(path);
    ExecutionConfig ec;
    ec.startTime = ptree.get_child("start").get_value<double>();
    auto stopTimeNode = ptree.get_child_optional("stop");
    if (stopTimeNode) {
        ec.stopTime = stopTimeNode->get_value<double>();
    }
    ec.stepSize = ptree.get_child("step_size").get_value<double>();
    return ec;
}
