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


SimulationEvent::SimulationEvent(
    dsb::model::TimePoint t,
    dsb::model::SlaveID s,
    dsb::model::VariableID v,
    const dsb::model::ScalarValue& n)
    : timePoint(t), slave(s), variable(v), newValue(n)
{
}


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


    typedef std::multimap<std::string, dsb::domain::Controller::SlaveType>
        SlaveTypeMap;

    // Obtains the list of available slave types on the domain and returns it
    // in the form of a map where the keys are slave type names and the values
    // are slave type descriptions.
    SlaveTypeMap SlaveTypesByName(dsb::domain::Controller& domain)
    {
        SlaveTypeMap types;
        BOOST_FOREACH (const auto& st, domain.GetSlaveTypes()) {
            types.insert(std::make_pair(st.name, st));
        }
        return types;
    }


    dsb::model::ScalarValue ParseVariableValue(
        const dsb::model::VariableDescription& variableDefinition,
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

    const dsb::domain::Controller::SlaveType* GetSlaveType(
        const std::map<std::string, const dsb::domain::Controller::SlaveType*>& slaves,
        const std::string& slaveName)
    {
        const auto slaveIt = slaves.find(slaveName);
        if (slaveIt == slaves.end()) {
            throw std::runtime_error("Unknown slave: " + slaveName);
        }
        return slaveIt->second;
    }

    struct VariableValue
    {
        dsb::model::VariableID id;
        dsb::model::ScalarValue value;
    };

    struct VariableConnection
    {
        dsb::model::VariableID inputId;
        std::string otherSlaveName;
        dsb::model::VariableID otherOutputId;
    };

    // Variable name lookup could take a long time for slave types with a
    // large number of variables, because dsb::domain::Controller::SlaveType
    // stores the variable descriptions in a vector.  Therefore, we cache the
    // ones we use in a hash map.
    typedef std::map<std::string, const dsb::model::VariableDescription*>
        VarDescriptionCacheEntry;
    typedef std::map<const dsb::domain::Controller::SlaveType*, VarDescriptionCacheEntry>
        VarDescriptionCache;

    // Given a slave type description and a variable name, this function will
    // first look for the slave type in the cache and, if it is found, do a
    // fast lookup of the variable description.  If the slave type is not found
    // in the cache, it will be added.
    const dsb::model::VariableDescription* GetCachedVarDescription(
        const dsb::domain::Controller::SlaveType* slaveType,
        const std::string& variableName,
        VarDescriptionCache& cache)
    {
        // Look for the slave type in the cache first
        auto cachedSlaveTypeIt = cache.find(slaveType);
        if (cachedSlaveTypeIt == cache.end()) {
            // Slave type not found in cache, so add it.
            VarDescriptionCacheEntry vars;
            BOOST_FOREACH(const auto& v, slaveType->variables) {
                vars[v.Name()] = &v;
            }
            cachedSlaveTypeIt = cache.insert(
                std::make_pair(slaveType, std::move(vars))).first;
        }
        const auto& cachedSlaveType = cachedSlaveTypeIt->second;

        auto cachedVarIt = cachedSlaveType.find(variableName);
        if (cachedVarIt == cachedSlaveType.end()) {
            throw std::runtime_error("Slave type '" + slaveType->name
                + "' has no variable named '" + variableName + "'");
        }
        return cachedVarIt->second;
    }
}


// Helper functions for ParseSystemConfig
namespace
{
    // Parses the "slave" node in 'ptree', building two maps:
    //   slaves   : maps slave names to slave types
    //   variables: maps slave names to lists of variable values
    void ParseSlavesNode(
        const boost::property_tree::ptree& ptree,
        const std::multimap<std::string, dsb::domain::Controller::SlaveType>& slaveTypes,
        std::map<std::string, const dsb::domain::Controller::SlaveType*>& slaves,
        std::map<std::string, std::vector<VariableValue>>& variables,
        VarDescriptionCache& varDescriptionCache)
    {
        assert(slaves.empty());
        assert(variables.empty());
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
            slaves[slaveName] = &slaveType;

            const auto initTree = slaveData.get_child("init", boost::property_tree::ptree());
            BOOST_FOREACH (const auto& initNode, initTree) {
                const auto varName = initNode.first;
                const auto varDesc = GetCachedVarDescription(&slaveType, varName, varDescriptionCache);
                VariableValue v;
                v.id = varDesc->ID();
                v.value = ParseVariableValue(*varDesc, initNode.second);
                variables[slaveName].push_back(v);
            }
        }
    }

    // Parses the "connections" node in 'ptree', building a mapping
    // ('connections') from slave names to lists of variable connections.
    void ParseConnectionsNode(
        const boost::property_tree::ptree& ptree,
        const std::map<std::string, const dsb::domain::Controller::SlaveType*>& slaves,
        std::map<std::string, std::vector<VariableConnection>>& connections,
        VarDescriptionCache& varDescriptionCache)
    {
        assert(connections.empty());
        const auto connTree = ptree.get_child("connections", boost::property_tree::ptree());
        BOOST_FOREACH (const auto& connNode, connTree) {
            const auto inputSpec = SplitVarSpec(connNode.first);
            const auto outputSpec = SplitVarSpec(connNode.second.data());
            try {
                const auto inputSlaveType = GetSlaveType(slaves, inputSpec.first);
                const auto outputSlaveType = GetSlaveType(slaves, outputSpec.first);
                const auto inputVarDesc = GetCachedVarDescription(
                    inputSlaveType, inputSpec.second, varDescriptionCache);
                const auto outputVarDesc = GetCachedVarDescription(
                    outputSlaveType, outputSpec.second, varDescriptionCache);
                if (inputVarDesc->DataType() != outputVarDesc->DataType()) {
                    throw std::runtime_error("Incompatible data types");
                }
                if (inputVarDesc->Causality() != dsb::model::INPUT_CAUSALITY) {
                    throw std::runtime_error("Not an input variable: " + inputVarDesc->Name());
                }
                if (outputVarDesc->Causality() != dsb::model::OUTPUT_CAUSALITY) {
                    throw std::runtime_error("Not an output variable: " + outputVarDesc->Name());
                }
                VariableConnection vc;
                vc.inputId = inputVarDesc->ID();
                vc.otherSlaveName = outputSpec.first;
                vc.otherOutputId = outputVarDesc->ID();
                connections[inputSpec.first].push_back(vc);
            } catch (const std::runtime_error& e) {
                throw std::runtime_error("In connection between "
                    + connNode.first + " and " + connNode.second.data() + ": "
                    + e.what());
            }
        }
    }

    // Parses the "scenario" node in 'ptree', producing two lists:
    //      scenario:  a list of scenario events, where the slave IDs are left
    //          undetermined since we don't know them yet.
    //      scenarioEventsSlaveName:  a list of the same length as 'scenario',
    //          which contains the names of the slaves that should receive
    //          the corresponding events in 'scenario'
    void ParseScenarioNode(
        const boost::property_tree::ptree& ptree,
        const std::map<std::string, const dsb::domain::Controller::SlaveType*>& slaves,
        std::ostream* warningLog,
        std::vector<SimulationEvent>& scenario,
        std::vector<std::string>& scenarioEventSlaveName,
        VarDescriptionCache& varDescriptionCache)
    {
        assert(scenario.empty());
        assert(scenarioEventSlaveName.empty());
        const auto scenTree = ptree.get_child("scenario", boost::property_tree::ptree());
        BOOST_FOREACH (const auto& scenNode, scenTree) {
            // Iterate elements of the form: timePoint { ... }
            try {
                const auto timePoint = boost::lexical_cast<dsb::model::TimePoint>(scenNode.first);
                BOOST_FOREACH (const auto& varChangeNode, scenNode.second) {
                    // Iterate elements of the form: varName varValue
                    const auto varSpec = SplitVarSpec(varChangeNode.first);
                    const auto& affectedSlaveName = varSpec.first;
                    const auto& affectedVarName = varSpec.second;
                    try {
                        const auto varDesc = GetCachedVarDescription(
                            slaves.at(affectedSlaveName),
                            affectedVarName,
                            varDescriptionCache);
                        if (warningLog) {
                            if (varDesc->Causality() == dsb::model::INPUT_CAUSALITY) {
                                *warningLog << "Warning: " << varChangeNode.first
                                    << " is an input variable.  If it is connected to"
                                       " an output, the scenario event may not have the"
                                       " intended effect." << std::endl;
                            } else if (varDesc->Causality() != dsb::model::PARAMETER_CAUSALITY) {
                                *warningLog << "Warning: " << varChangeNode.first
                                    << " is not a parameter, and should therefore"
                                       " normally not be changed manually." << std::endl;
                            }
                            if (varDesc->Variability() == dsb::model::CONSTANT_VARIABILITY
                                || varDesc->Variability() == dsb::model::FIXED_VARIABILITY) {
                                *warningLog << "Warning: " << varChangeNode.first
                                    << " is not a modifiable variable." << std::endl;
                            }
                        }
                        scenario.push_back(SimulationEvent(
                            timePoint,
                            dsb::model::INVALID_SLAVE_ID,
                            varDesc->ID(),
                            ParseVariableValue(*varDesc, varChangeNode.second)));
                        scenarioEventSlaveName.push_back(affectedSlaveName);
                    } catch (const std::exception& e) {
                        throw std::runtime_error("For variable " + varChangeNode.first
                            + ": " + e.what());
                    }
                }
            } catch (const std::exception& e) {
                throw std::runtime_error("In scenario event at t=" + scenNode.first
                    + ": " + e.what());
            }
        }
    }
}


void ParseSystemConfig(
    const std::string& path,
    dsb::domain::Controller& domain,
    dsb::execution::Controller& execution,
    const dsb::net::ExecutionLocator& executionLocator,
    std::vector<SimulationEvent>& scenarioOut,
    std::chrono::milliseconds commTimeout,
    std::chrono::milliseconds instantiationTimeout,
    std::ostream* warningLog)
{
    const auto ptree = ReadPtreeInfoFile(path);
    const auto slaveTypes = SlaveTypesByName(domain);

    std::map<std::string, const dsb::domain::Controller::SlaveType*> slaves;
    std::map<std::string, std::vector<VariableValue>> variables;
    VarDescriptionCache varDescriptionCache;
    ParseSlavesNode(ptree, slaveTypes, slaves, variables, varDescriptionCache);

    std::map<std::string, std::vector<VariableConnection>> connections;
    ParseConnectionsNode(ptree, slaves, connections, varDescriptionCache);

    std::vector<SimulationEvent> scenario;
    std::vector<std::string> scenarioEventSlaveName; // We don't know IDs yet, so we keep a parallel list of names
    ParseScenarioNode(ptree, slaves, warningLog, scenario, scenarioEventSlaveName, varDescriptionCache);

    // Add all the slaves to the execution and map their names to their
    // numeric IDs.
    std::map<std::string, std::shared_future<dsb::model::SlaveID>> slaveIDs;
    BOOST_FOREACH (const auto& slave, slaves) {
        auto slaveLoc = domain.InstantiateSlave(slave.second->uuid, instantiationTimeout);
        slaveIDs[slave.first] = execution.AddSlave(slaveLoc, commTimeout).share();
    }

    // Using the name-ID mapping, build lists of variable settings from the
    // lists of initial values and connections, and execute "set variables"
    // commands for each slave.
    std::map<std::string, std::vector<dsb::model::VariableSetting>>
        varSettings;
    BOOST_FOREACH (const auto& slaveVars, variables) {
        const auto& slaveName = slaveVars.first;
        BOOST_FOREACH (const auto& varValue, slaveVars.second) {
            varSettings[slaveName].push_back(dsb::model::VariableSetting(
                varValue.id,
                varValue.value));
        }
    }
    BOOST_FOREACH (const auto& slaveConns, connections) {
        const auto& slaveName = slaveConns.first;
        BOOST_FOREACH (const auto& conn, slaveConns.second) {
            varSettings[slaveName].push_back(dsb::model::VariableSetting(
                conn.inputId,
                dsb::model::Variable(slaveIDs.at(conn.otherSlaveName).get(), conn.otherOutputId)));
        }
    }
    std::vector<std::pair<std::string, std::future<void>>> futureResults;
    BOOST_FOREACH (const auto& varSetting, varSettings) {
        const auto& slaveName = varSetting.first;
        const auto slaveID = slaveIDs.at(slaveName).get();
        futureResults.push_back(std::make_pair(
            slaveName,
            execution.SetVariables(slaveID, varSetting.second, commTimeout)));
    }

    // Check that all operations succeeded, build a list of errors for those
    // that didn't.
    bool anyErrors = false;
    SetVariablesException svEx;
    BOOST_FOREACH (auto& result, futureResults)
    {
        try { result.second.get(); }
        catch (const std::exception& e) {
            anyErrors = true;
            svEx.AddSlaveError(result.first, e.what());
        }
    }
    if (anyErrors) throw svEx;

    // Update the scenario with the correct numeric slave IDs.
    for (size_t i = 0; i < scenario.size(); ++i) {
        scenario[i].slave = slaveIDs[scenarioEventSlaveName[i]].get();
    }
    scenarioOut.swap(scenario);
}


SetVariablesException::SetVariablesException()
    : std::runtime_error("Error setting variable(s)"),
      m_msg("Error setting variable(s) for the following slave(s):")
{
}


const char* SetVariablesException::what() const DSB_NOEXCEPT
{
    return m_msg.c_str();
}


void SetVariablesException::AddSlaveError(
    const std::string& slaveName,
    const std::string& errMsg)
{
    m_slaveErrors.push_back(std::make_pair(slaveName, errMsg));
    m_msg += ' ' + slaveName + " (" + errMsg + ");";
}


ExecutionConfig::ExecutionConfig()
    : startTime(0.0),
      stopTime(std::numeric_limits<double>::infinity()),
      stepSize(1.0),
      commTimeout(std::chrono::seconds(1)),
      stepTimeoutMultiplier(100.0),
      slaveTimeout(std::chrono::hours(1)),
      instantiationTimeout(std::chrono::seconds(30))
{
}


ExecutionConfig ParseExecutionConfig(const std::string& path)
{
    auto Error = [path](const std::string msg) {
        throw std::runtime_error("Error in configuration file (" + path + "): " + msg);
    };
    const auto ptree = ReadPtreeInfoFile(path);
    ExecutionConfig ec;

    if (auto node = ptree.get_child_optional("start")) {
        ec.startTime = node->get_value<double>();
    }

    if (auto node = ptree.get_child_optional("stop")) {
        ec.stopTime = node->get_value<double>();
    }
    if (ec.stopTime < ec.startTime) Error("Stop time less than start time");

    ec.stepSize = ptree.get_child("step_size").get_value<double>();
    if (ec.stepSize <= 0) Error("Nonpositive step size");

    if (auto commTimeoutNode = ptree.get_child_optional("comm_timeout_ms")) {
        ec.commTimeout = std::chrono::milliseconds(
            commTimeoutNode->get_value<typename std::chrono::milliseconds::rep>());
        if (ec.commTimeout <= std::chrono::milliseconds(0)) Error("Nonpositive comm_timeout_ms");
    }

    if (auto stepTimeoutMultiplierNode = ptree.get_child_optional("step_timeout_multiplier")) {
        ec.stepTimeoutMultiplier = stepTimeoutMultiplierNode->get_value<double>();
        if (ec.stepTimeoutMultiplier <= 0.0) Error("Nonpositive slave_timeout_multiplier");
    }

    if (auto slaveTimeoutNode = ptree.get_child_optional("slave_timeout_s")) {
        ec.slaveTimeout = std::chrono::seconds(
            slaveTimeoutNode->get_value<typename std::chrono::seconds::rep>());
        if (ec.slaveTimeout <= std::chrono::seconds(0)) Error("Nonpositive slave_timeout_s");
    }

    if (auto instTimeoutNode = ptree.get_child_optional("instantiation_timeout_ms")) {
        ec.instantiationTimeout = std::chrono::milliseconds(
            instTimeoutNode->get_value<typename std::chrono::milliseconds::rep>());
        if (ec.commTimeout <= std::chrono::milliseconds(0)) Error("Nonpositive instantiation_timeout_ms");
    }
    return ec;
}
