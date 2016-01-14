/**
\file
\brief  Main module header for dsb::net
*/
#ifndef DSB_NET_HPP
#define DSB_NET_HPP

#include <chrono>
#include <cstdint>
#include <string>


namespace dsb
{
/// Network addressing
namespace net
{


/// Class which represents the network location(s) of a simulation domain.
class DomainLocator
{
public:
    DomainLocator() { }

    DomainLocator(
        const std::string& reportMasterEndpoint,
        const std::string& reportSlavePEndpoint,
        const std::string& infoMasterEndpoint,
        const std::string& infoSlavePEndpoint,
        const std::string& execReqEndpoint);

    const std::string& ReportMasterEndpoint() const;
    const std::string& ReportSlavePEndpoint() const;
    const std::string& InfoMasterEndpoint() const;
    const std::string& InfoSlavePEndpoint() const;
    const std::string& ExecReqEndpoint() const;

private:
    std::string m_reportMasterEndpoint;
    std::string m_reportSlavePEndpoint;
    std::string m_infoMasterEndpoint;
    std::string m_infoSlavePEndpoint;
    std::string m_execReqEndpoint;
};


//TODO: This stuff is temporary and should be moved or removed.
const std::uint16_t DEFAULT_DOMAIN_BROKER_PORT = 10242;
DomainLocator GetDomainEndpoints(const std::string& domainBrokerAddress);


/// Class which represents the network location(s) of an execution.
class ExecutionLocator
{
public:
    ExecutionLocator() { }

    ExecutionLocator(
        const std::string& masterEndpoint,
        const std::string& slaveEndpoint,
        const std::string& variablePubEndpoint,
        const std::string& variableSubEndpoint,
        const std::string& execTerminationEndpoint,
        const std::string& execID,
        std::chrono::seconds commTimeout);

    const std::string& MasterEndpoint() const;
    const std::string& SlaveEndpoint() const;
    const std::string& VariablePubEndpoint() const;
    const std::string& VariableSubEndpoint() const;
    const std::string& ExecTerminationEndpoint() const;
    const std::string& ExecName() const;
    std::chrono::seconds CommTimeout() const;

private:
    std::string m_masterEndpoint;
    std::string m_slaveEndpoint;
    std::string m_variablePubEndpoint;
    std::string m_variableSubEndpoint;
    std::string m_execTerminationEndpoint;
    std::string m_execName;

    // The communications timeout for the whole execution.  If no communication
    // happens between participants for this amount of time, everyone is
    // responsible for shutting down themselves.
    // TODO: Re-evaluate whether this should be in a Locator, as it doesn't
    //       strictly have anything to do with location.
    std::chrono::seconds m_commTimeout;
};


/// Class which represents the network location(s) of a slave.
class SlaveLocator
{
public:
    explicit SlaveLocator(
        const std::string& endpoint = std::string(),
        const std::string& identity = std::string());
    const std::string& Endpoint() const;
    const std::string& Identity() const;
    bool Empty() const;
    bool HasIdentity() const;

private:
    std::string m_endpoint;
    std::string m_identity;
};


}}      // namespace
#endif  // header guard
