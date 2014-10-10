#include "config_parser.hpp"

#include <map>
#include <limits>
#include <vector>
#include <utility>

#include "boost/foreach.hpp"
#include "boost/lexical_cast.hpp"
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
    std::pair<std::string, unsigned> SplitVarSpec(const std::string& varSpec)
    {
        const auto dotPos = varSpec.find_first_of('.');
        if (dotPos >= varSpec.size() - 1) {
            throw std::runtime_error(
                "Invalid variable identifier (should be on the format \"slave.var\"): "
                + varSpec);
        }
        return std::make_pair(
            varSpec.substr(0, dotPos),
            boost::lexical_cast<unsigned>(varSpec.substr(dotPos + 1)));
    }
}


void ParseSystemConfig(
    const std::string& path,
    dsb::execution::Controller& controller)
{
    const auto ptree = ReadPtreeInfoFile(path);

    std::map<std::string, unsigned> slaves;
    std::map<unsigned, std::vector<dsb::types::Variable>> variables;

    // NOTE: For some reason, if we put the get_child() call inside the
    // BOOST_FOREACH macro invocation, we get a segfault if the child node
    // does not exist.
    const auto slaveTree = ptree.get_child("slaves", boost::property_tree::ptree());
    BOOST_FOREACH (const auto& slaveNode, slaveTree) {
        const auto slaveName = slaveNode.first;
        const auto slaveData = slaveNode.second;
        const auto slaveId = slaveData.get<unsigned>("id");
        if (slaves.count(slaveName)) {
            throw std::runtime_error(
                "Configuration file contains two slaves with the name '"
                + slaveName + "'");
        }
        slaves[slaveName] = slaveId;

        const auto initTree = slaveData.get_child("init", boost::property_tree::ptree());
        BOOST_FOREACH (const auto& initNode, initTree) {
            // TODO: support all variable types, not just double
            dsb::types::Variable v;
            v.id = boost::lexical_cast<unsigned>(initNode.first);
            v.value = initNode.second.get_value<double>();
            variables[slaveId].push_back(v);
        }
    }

    std::map<unsigned, std::vector<dsb::types::VariableConnection>> connections;
    const auto connTree = ptree.get_child("connections", boost::property_tree::ptree());
    BOOST_FOREACH (const auto& connNode, connTree) {
        const auto inputSpec = SplitVarSpec(connNode.first);
        const auto outputSpec = SplitVarSpec(connNode.second.data());
        dsb::types::VariableConnection vc;
        vc.inputId = inputSpec.second;
        vc.otherSlaveId = slaves.at(outputSpec.first);
        vc.otherOutputId = outputSpec.second;
        connections[slaves.at(inputSpec.first)].push_back(vc);
    }

    BOOST_FOREACH (const auto& slave, slaves) {
        controller.AddSlave(slave.second);
    }
    BOOST_FOREACH (auto& slaveVars, variables) {
        controller.SetVariables(
            slaveVars.first,
            dsb::sequence::ElementsOf(slaveVars.second));
    }
    BOOST_FOREACH (auto& conn, connections) {
        controller.ConnectVariables(
            conn.first,
            dsb::sequence::ElementsOf(conn.second));
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
