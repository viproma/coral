#include "dsb/domain/locator.hpp"

#include "zmq.hpp"

#include "dsb/comm/messaging.hpp"
#include "dsb/comm/util.hpp"
#include "dsb/compat_helpers.hpp"

namespace dsb
{
namespace domain
{


Locator::Locator(
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


const std::string& Locator::ReportMasterEndpoint() const { return m_reportMasterEndpoint; }


const std::string& Locator::ReportSlavePEndpoint() const { return m_reportSlavePEndpoint; }


const std::string& Locator::InfoMasterEndpoint() const { return m_infoMasterEndpoint; }


const std::string& Locator::InfoSlavePEndpoint() const { return m_infoSlavePEndpoint; }


const std::string& Locator::ExecReqEndpoint() const { return m_execReqEndpoint; }


Locator GetDomainEndpoints(const std::string& domainBrokerAddress)
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
    std::deque<zmq::message_t> msg;
    msg.push_back(dsb::comm::ToFrame("GET_PROXY_PORTS"));
    dsb::comm::Send(sck, msg);
    dsb::comm::Receive(sck, msg);
    if (msg.size() < 5 || dsb::comm::ToString(msg[0]) != "PROXY_PORTS") {
        throw std::runtime_error("Invalid reply from domain broker");
    }
    return Locator(
        baseAddress + dsb::comm::ToString(msg[1]),
        baseAddress + dsb::comm::ToString(msg[2]),
        baseAddress + dsb::comm::ToString(msg[3]),
        baseAddress + dsb::comm::ToString(msg[4]),
        fullAddress);
}


}}
