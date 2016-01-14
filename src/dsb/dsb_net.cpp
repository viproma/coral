#include "dsb/net.hpp"

#include <stdexcept>
#include "zmq.hpp"

#include "dsb/comm/messaging.hpp"
#include "dsb/comm/util.hpp"
#include "dsb/error.hpp"


namespace dsb
{
namespace net
{


// =============================================================================
// DomainLocator
// =============================================================================

DomainLocator::DomainLocator(
    const std::string& reportMasterEndpoint,
    const std::string& reportSlavePEndpoint,
    const std::string& infoMasterEndpoint,
    const std::string& infoSlavePEndpoint,
    const std::string& execReqEndpoint)
    : m_reportMasterEndpoint(reportMasterEndpoint),
      m_reportSlavePEndpoint(reportSlavePEndpoint),
      m_infoMasterEndpoint(infoMasterEndpoint),
      m_infoSlavePEndpoint(infoSlavePEndpoint),
      m_execReqEndpoint(execReqEndpoint)
{
}


const std::string& DomainLocator::ReportMasterEndpoint() const { return m_reportMasterEndpoint; }


const std::string& DomainLocator::ReportSlavePEndpoint() const { return m_reportSlavePEndpoint; }


const std::string& DomainLocator::InfoMasterEndpoint() const { return m_infoMasterEndpoint; }


const std::string& DomainLocator::InfoSlavePEndpoint() const { return m_infoSlavePEndpoint; }


const std::string& DomainLocator::ExecReqEndpoint() const { return m_execReqEndpoint; }


DomainLocator GetDomainEndpoints(const std::string& domainBrokerAddress)
{
    if (domainBrokerAddress.substr(0, 6) != "tcp://") {
        throw std::runtime_error(
            "Invalid broker address: " + domainBrokerAddress
            + " (only TCP communication supported)");
    }
    const auto colonPos = domainBrokerAddress.rfind(':');
    std::string baseAddress, fullAddress;
    if (colonPos == 3) {
        baseAddress = domainBrokerAddress + ':';
        fullAddress = baseAddress + std::to_string(DEFAULT_DOMAIN_BROKER_PORT);
    } else {
        baseAddress = domainBrokerAddress.substr(0, colonPos+1);
        fullAddress = domainBrokerAddress;
    }

    auto sck = zmq::socket_t(dsb::comm::GlobalContext(), ZMQ_REQ);
    sck.connect(fullAddress.c_str());
    std::vector<zmq::message_t> msg;
    msg.push_back(dsb::comm::ToFrame("GET_PROXY_PORTS"));
    dsb::comm::Send(sck, msg);
    dsb::comm::Receive(sck, msg);
    if (msg.size() < 5 || dsb::comm::ToString(msg[0]) != "PROXY_PORTS") {
        throw std::runtime_error("Invalid reply from domain broker");
    }
    return DomainLocator(
        baseAddress + dsb::comm::ToString(msg[1]),
        baseAddress + dsb::comm::ToString(msg[2]),
        baseAddress + dsb::comm::ToString(msg[3]),
        baseAddress + dsb::comm::ToString(msg[4]),
        fullAddress);
}


// =============================================================================
// ExecutionLocator
// =============================================================================

ExecutionLocator::ExecutionLocator(
    const std::string& masterEndpoint,
    const std::string& slaveEndpoint,
    const std::string& variablePubEndpoint,
    const std::string& variableSubEndpoint,
    const std::string& execTerminationEndpoint,
    const std::string& execID,
    std::chrono::seconds commTimeout)
    : m_masterEndpoint(masterEndpoint),
      m_slaveEndpoint(slaveEndpoint),
      m_variablePubEndpoint(variablePubEndpoint),
      m_variableSubEndpoint(variableSubEndpoint),
      m_execTerminationEndpoint(execTerminationEndpoint),
      m_execName(execID),
      m_commTimeout(commTimeout)
{
}

const std::string& ExecutionLocator::MasterEndpoint() const
{
    return m_masterEndpoint;
}

const std::string& ExecutionLocator::SlaveEndpoint() const
{
    return m_slaveEndpoint;
}

const std::string& ExecutionLocator::VariablePubEndpoint() const
{
    return m_variablePubEndpoint;
}

const std::string& ExecutionLocator::VariableSubEndpoint() const
{
    return m_variableSubEndpoint;
}

const std::string& ExecutionLocator::ExecTerminationEndpoint() const
{
    return m_execTerminationEndpoint;
}

const std::string& ExecutionLocator::ExecName() const
{
    return m_execName;
}

std::chrono::seconds ExecutionLocator::CommTimeout() const
{
    return m_commTimeout;
}


// =============================================================================
// SlaveLocator
// =============================================================================

SlaveLocator::SlaveLocator(
    const std::string& endpoint,
    const std::string& identity)
    : m_endpoint(endpoint),
      m_identity(identity)
{
}

const std::string& SlaveLocator::Endpoint() const
{
    return m_endpoint;
}

const std::string& SlaveLocator::Identity() const
{
    return m_identity;
}

bool SlaveLocator::Empty() const
{
    return m_endpoint.empty();
}

bool SlaveLocator::HasIdentity() const
{
    return !m_identity.empty();
}


}} // namespace
