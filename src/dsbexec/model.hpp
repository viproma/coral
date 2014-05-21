#ifndef DSB_MODEL_HPP
#define DSB_MODEL_HPP

#include <map>
#include <string>


namespace dsb
{
namespace model
{


class Slave
{
public:
    typedef std::map<std::string, std::string> ParamMap;

    Slave(const std::string& name, const std::string& type);

    const std::string& Name() const;

    const std::string& Type() const;

    ParamMap& Params();

    const ParamMap& Params() const;

private:
    const std::string m_name;
    const std::string m_type;
    std::map<std::string, std::string> m_params;
};


struct Variable
{
    Variable(const std::string& qualifiedName);

    std::string QualifiedName() const;

    std::string slaveName;
    std::string varName;
};


class Model
{
public:
    Slave* AddSlave(const std::string& name, const std::string& type);

    void Connect(const Variable& input, const Variable& output);

    void DebugDump() const;

private:
    std::map<std::string, Slave> m_slaves;
    std::map<std::string, std::map<std::string, Variable>> m_connections;
};


}}      // namespace
#endif  // header guard
