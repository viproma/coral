#include "model.hpp"

#include <cassert>
#include <iostream> // Only for Model::DebugDump(); remove later!
#include <stdexcept>
#include <utility>
#include "boost/foreach.hpp"



Slave::Slave(const std::string& name, const std::string& type)
    : m_name(name), m_type(type)
{
}


const std::string& Slave::Name() const { return m_name; }

const std::string& Slave::Type() const { return m_type; }

Slave::ParamMap& Slave::Params() { return m_params; }

const Slave::ParamMap& Slave::Params() const { return m_params; }


// =============================================================================


Variable::Variable(const std::string& qualifiedName)
{
    const auto dotPos = qualifiedName.find_first_of('.');
    if (dotPos >= qualifiedName.size() - 1) {
        throw std::runtime_error("Invalid variable reference: " + qualifiedName);
    }
    slaveName = qualifiedName.substr(0, dotPos);
    varName   = qualifiedName.substr(dotPos + 1);
}


std::string Variable::QualifiedName() const
{
    return slaveName + "." + varName;
}


// =============================================================================


Slave* Model::AddSlave(const std::string& name, const std::string& type)
{
    auto ins = m_slaves.insert(std::make_pair(name, Slave(name, type)));
    if (!ins.second) {
        throw std::runtime_error("Duplicate slave name: " + name);
    }
    return &(ins.first->second);
}


void Model::Connect(const Variable& input, const Variable& output)
{
    if (m_slaves.count(input.slaveName) == 0) {
        throw std::runtime_error(
            "Attempted to connect nonexistent slave: " + input.slaveName);
    }
    if (m_slaves.count(output.slaveName) == 0) {
        throw std::runtime_error(
            "Attempted to connect nonexistent slave: " + output.slaveName);
    }
    auto ins = m_connections[input.slaveName].insert(
        std::make_pair(input.varName, output));
    if (!ins.second) {
        throw std::runtime_error(
            "Variable connected multiple times: " + input.QualifiedName());
    }
}


void Model::DebugDump() const
{
    std::cout << "Slaves:\n";
    BOOST_FOREACH(const auto& s, m_slaves) {
        assert(s.first == s.second.Name());
        std::cout << "  " << s.first << " (" << s.second.Type() << ")\n";
        BOOST_FOREACH(const auto& p, s.second.Params()) {
            std::cout << "    " << p.first << " = " << p.second << "\n";
        }
    }
    std::cout << "Connections:\n";
    BOOST_FOREACH(const auto& is, m_connections) {
        BOOST_FOREACH(const auto& iv, is.second) {
            std::cout << "  " << iv.second.QualifiedName() << " -> "
                      << is.first << "." << iv.first << "\n";
        }
    }
}
