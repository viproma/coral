#include "config_parser.hpp"

#include <algorithm>
#include <limits>
#include <map>
#include <set>
#include <utility>
#include <vector>

#include "boost/foreach.hpp"
#include "boost/lexical_cast.hpp"
#include "boost/numeric/conversion/cast.hpp"
#include "boost/property_tree/info_parser.hpp"
#include "boost/property_tree/ptree.hpp"

#include "coral/log.hpp"


SimulationEvent::SimulationEvent(
    coral::model::TimePoint t,
    coral::model::SlaveID s,
    coral::model::VariableID v,
    const coral::model::ScalarValue& n)
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
        if (varSpec.empty()) {
            throw std::runtime_error(
                "Missing or empty variable identifier (should be on the format \"slave.var\")");
        }
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


    typedef std::multimap<std::string, coral::master::ProviderCluster::SlaveType>
        SlaveTypeMap;

    // Obtains the list of available slave types on the network and returns it
    // in the form of a map where the keys are slave type names and the values
    // are slave type descriptions.
    SlaveTypeMap SlaveTypesByName(coral::master::ProviderCluster& providers)
    {
        SlaveTypeMap types;
        BOOST_FOREACH (const auto& st, providers.GetSlaveTypes(std::chrono::seconds(1))) {
            types.insert(std::make_pair(st.description.Name(), st));
        }
        return types;
    }


    coral::model::ScalarValue ParseVariableValue(
        const coral::model::VariableDescription& variableDefinition,
        const boost::property_tree::ptree& valueNode)
    {
        try {
            switch (variableDefinition.DataType()) {
                case coral::model::REAL_DATATYPE:
                    return valueNode.get_value<double>();
                case coral::model::INTEGER_DATATYPE:
                    return valueNode.get_value<int>();
                case coral::model::BOOLEAN_DATATYPE:
                    return valueNode.get_value<bool>();
                case coral::model::STRING_DATATYPE:
                    return valueNode.get_value<std::string>();
                default:
                    assert (false);
                    return coral::model::ScalarValue();
            }
        } catch (const std::runtime_error& e) {
            throw std::runtime_error(
                "Invalid value for variable '" + variableDefinition.Name()
                + "': " + valueNode.get_value<std::string>()
                + " (" + e.what() + ")");
        }
    }

    const coral::master::ProviderCluster::SlaveType* GetSlaveType(
        const std::map<std::string, const coral::master::ProviderCluster::SlaveType*>& slaves,
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
        coral::model::VariableID id;
        coral::model::ScalarValue value;
    };

    struct VariableConnection
    {
        coral::model::VariableID inputId;
        std::string otherSlaveName;
        coral::model::VariableID otherOutputId;
    };

    // Variable name lookup could take a long time for slave types with a
    // large number of variables, because coral::master::ProviderCluster::SlaveType
    // stores the variable descriptions in a vector.  Therefore, we cache the
    // ones we use in a hash map.
    typedef std::map<std::string, const coral::model::VariableDescription*>
        VarDescriptionCacheEntry;
    typedef std::map<const coral::master::ProviderCluster::SlaveType*, VarDescriptionCacheEntry>
        VarDescriptionCache;

    // Given a slave type description and a variable name, this function will
    // first look for the slave type in the cache and, if it is found, do a
    // fast lookup of the variable description.  If the slave type is not found
    // in the cache, it will be added.
    const coral::model::VariableDescription* GetCachedVarDescription(
        const coral::master::ProviderCluster::SlaveType* slaveType,
        const std::string& variableName,
        VarDescriptionCache& cache)
    {
        // Look for the slave type in the cache first
        auto cachedSlaveTypeIt = cache.find(slaveType);
        if (cachedSlaveTypeIt == cache.end()) {
            // Slave type not found in cache, so add it.
            VarDescriptionCacheEntry vars;
            BOOST_FOREACH(const auto& v, slaveType->description.Variables()) {
                vars[v.Name()] = &v;
            }
            cachedSlaveTypeIt = cache.insert(
                std::make_pair(slaveType, std::move(vars))).first;
        }
        const auto& cachedSlaveType = cachedSlaveTypeIt->second;

        auto cachedVarIt = cachedSlaveType.find(variableName);
        if (cachedVarIt == cachedSlaveType.end()) {
            throw std::runtime_error(
                "Slave type '" + slaveType->description.Name()
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
        const std::multimap<std::string, coral::master::ProviderCluster::SlaveType>& slaveTypes,
        std::map<std::string, const coral::master::ProviderCluster::SlaveType*>& slaves,
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
                    "Slave type not found: " + slaveTypeName);
            } else if (typeCount > 1) {
                throw std::runtime_error(
                    "Found two or more slave types with the same name: " + slaveTypeName);
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
        const std::map<std::string, const coral::master::ProviderCluster::SlaveType*>& slaves,
        std::ostream* warningLog,
        std::map<std::string, std::vector<VariableConnection>>& connections,
        VarDescriptionCache& varDescriptionCache)
    {
        assert(connections.empty());
        std::map<std::string, std::set<std::string>> connectedVars; // just for making warnings
        const auto connTree = ptree.get_child("connections", boost::property_tree::ptree());
        try {
            for (const auto& connNode : connTree) {
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
                    if (inputVarDesc->Causality() != coral::model::INPUT_CAUSALITY) {
                        throw std::runtime_error("Not an input variable: " + inputVarDesc->Name());
                    }
                    if (outputVarDesc->Causality() != coral::model::OUTPUT_CAUSALITY) {
                        throw std::runtime_error("Not an output variable: " + outputVarDesc->Name());
                    }
                    VariableConnection vc;
                    vc.inputId = inputVarDesc->ID();
                    vc.otherSlaveName = outputSpec.first;
                    vc.otherOutputId = outputVarDesc->ID();
                    connections[inputSpec.first].push_back(vc);

                    if (warningLog) {
                        connectedVars[inputSpec.first].insert(inputSpec.second);
                        connectedVars[outputSpec.first].insert(outputSpec.second);
                    }
                } catch (const std::runtime_error& e) {
                    throw std::runtime_error("In connection between "
                        + connNode.first + " and " + connNode.second.data() + ": "
                        + e.what());
                }
            }
        } catch (const std::runtime_error& e) {
            throw std::runtime_error(
                std::string("In \"connections\" section: ") + e.what());
        }

        // If warnings are enabled, we list all unconnected input/output variables.
        if (warningLog) {
            for (const auto& slave : slaves) {
                const auto sit = connectedVars.find(slave.first);
                for (const auto& var : slave.second->description.Variables()) {
                    if (var.Causality() == coral::model::INPUT_CAUSALITY
                        || var.Causality() == coral::model::OUTPUT_CAUSALITY)
                    {
                        if (sit == connectedVars.end()
                            || sit->second.find(var.Name()) == sit->second.end())
                        {
                            *warningLog << "Warning: " << slave.first << '.'
                                << var.Name() << " is not connected" << std::endl;
                        }
                    }
                }
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
        const std::map<std::string, const coral::master::ProviderCluster::SlaveType*>& slaves,
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
                const auto timePoint = boost::lexical_cast<coral::model::TimePoint>(scenNode.first);
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
                            if (varDesc->Causality() == coral::model::INPUT_CAUSALITY) {
                                *warningLog << "Warning: " << varChangeNode.first
                                    << " is an input variable.  If it is connected to"
                                       " an output, the scenario event may not have the"
                                       " intended effect." << std::endl;
                            } else if (varDesc->Causality() != coral::model::PARAMETER_CAUSALITY) {
                                *warningLog << "Warning: " << varChangeNode.first
                                    << " is not a parameter, and should therefore"
                                       " normally not be changed manually." << std::endl;
                            }
                            if (varDesc->Variability() == coral::model::CONSTANT_VARIABILITY
                                || varDesc->Variability() == coral::model::FIXED_VARIABILITY) {
                                *warningLog << "Warning: " << varChangeNode.first
                                    << " is not a modifiable variable." << std::endl;
                            }
                        }
                        scenario.push_back(SimulationEvent(
                            timePoint,
                            coral::model::INVALID_SLAVE_ID,
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
    coral::master::ProviderCluster& providers,
    coral::master::Execution& execution,
    std::vector<SimulationEvent>& scenarioOut,
    std::chrono::milliseconds commTimeout,
    std::chrono::milliseconds instantiationTimeout,
    std::ostream* warningLog)
{
    const auto ptree = ReadPtreeInfoFile(path);
    const auto slaveTypes = SlaveTypesByName(providers);

    std::map<std::string, const coral::master::ProviderCluster::SlaveType*> slaves;
    std::map<std::string, std::vector<VariableValue>> variables;
    VarDescriptionCache varDescriptionCache;
    ParseSlavesNode(ptree, slaveTypes, slaves, variables, varDescriptionCache);

    std::map<std::string, std::vector<VariableConnection>> connections;
    ParseConnectionsNode(ptree, slaves, warningLog, connections, varDescriptionCache);

    std::vector<SimulationEvent> scenario;
    std::vector<std::string> scenarioEventSlaveName; // We don't know IDs yet, so we keep a parallel list of names
    ParseScenarioNode(ptree, slaves, warningLog, scenario, scenarioEventSlaveName, varDescriptionCache);

    // Add all the slaves to the execution and map their names to their
    // numeric IDs.
    std::vector<coral::master::AddedSlave> slavesToAdd;
    BOOST_FOREACH (const auto& slave, slaves) {
        slavesToAdd.emplace_back();
        slavesToAdd.back().locator = providers.InstantiateSlave(
            slave.second->providers.front(),
            slave.second->description.UUID(),
            instantiationTimeout);
        slavesToAdd.back().name = slave.first;
    }
    try {
        execution.Reconstitute(slavesToAdd, commTimeout);
    } catch (const std::runtime_error&) {
        for (const auto& addedSlave : slavesToAdd) {
            if (addedSlave.error) {
                coral::log::Log(coral::log::error,
                    boost::format("Error adding slave '%s': %s")
                        % addedSlave.name
                        % addedSlave.error.message());
            }
        }
        throw;
    }
    std::map<std::string, coral::model::SlaveID> slaveIDs;
    std::map<coral::model::SlaveID, std::string> slaveNames;
    for (const auto& addedSlave : slavesToAdd) {
        slaveIDs[addedSlave.name] = addedSlave.id;
        slaveNames[addedSlave.id] = addedSlave.name;
    }

    // Using the name-ID mapping, build lists of variable settings from the
    // lists of initial values and connections, and execute "set variables"
    // commands for each slave.
    std::vector<coral::master::SlaveConfig> slaveConfigs;
    std::map<std::string, std::size_t> slaveConfigsIndexes;
    for (const auto& slaveVars : variables) {
        const auto& slaveName = slaveVars.first;
        auto index = slaveConfigsIndexes.find(slaveName);
        if (index == end(slaveConfigsIndexes)) {
            index = slaveConfigsIndexes.insert(std::make_pair(
                    slaveName,
                    slaveConfigs.size()
                )).first;
            slaveConfigs.emplace_back();
            slaveConfigs.back().slaveID = slaveIDs.at(slaveName);
        }
        auto& sc = slaveConfigs[index->second];
        for (const auto& varValue : slaveVars.second) {
            sc.variableSettings.emplace_back(varValue.id, varValue.value);
        }
    }
    for (const auto& slaveConns : connections) {
        const auto& slaveName = slaveConns.first;
        auto index = slaveConfigsIndexes.find(slaveName);
        if (index == end(slaveConfigsIndexes)) {
            index = slaveConfigsIndexes.insert(std::make_pair(
                    slaveName,
                    slaveConfigs.size()
                )).first;
            slaveConfigs.emplace_back();
            slaveConfigs.back().slaveID = slaveIDs.at(slaveName);
        }
        auto& sc = slaveConfigs[index->second];
        for (const auto& conn : slaveConns.second) {
            sc.variableSettings.emplace_back(
                conn.inputId,
                coral::model::Variable(
                    slaveIDs.at(conn.otherSlaveName),
                    conn.otherOutputId));
        }
    }
    try {
        execution.Reconfigure(slaveConfigs, commTimeout);
    } catch (const std::exception&) {
        for (const auto& sc : slaveConfigs) {
            if (sc.error) {
                coral::log::Log(coral::log::error,
                    boost::format("Error configuring variables of slave '%s': %s")
                    % slaveNames.at(sc.slaveID)
                    % sc.error.message());
            }
        }
        throw;
    }

    // Update the scenario with the correct numeric slave IDs.
    for (size_t i = 0; i < scenario.size(); ++i) {
        scenario[i].slave = slaveIDs[scenarioEventSlaveName[i]];
    }
    scenarioOut.swap(scenario);
}


SetVariablesException::SetVariablesException()
    : std::runtime_error("Error setting variable(s)"),
      m_msg("Error setting variable(s) for the following slave(s):")
{
}


const char* SetVariablesException::what() const CORAL_NOEXCEPT
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
        if (ec.stepTimeoutMultiplier * ec.stepSize * 1000 < 1.0) {
            Error("step_timeout_multiplier is too small");
        }
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
