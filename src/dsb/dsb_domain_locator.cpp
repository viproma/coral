#include "dsb/domain/locator.hpp"


namespace dsb
{
namespace domain
{


Locator::Locator(
    const std::string& reportEndpoint,
    const std::string& infoEndpoint,
    const std::string& execReqEndpoint)
    : m_reportEndpoint(reportEndpoint),
      m_infoEndpoint(infoEndpoint),
      m_execReqEndpoint(execReqEndpoint)
{
}


const std::string& Locator::ReportEndpoint() const { return m_reportEndpoint; }


const std::string& Locator::InfoEndpoint() const { return m_infoEndpoint; }


const std::string& Locator::ExecReqEndpoint() const { return m_execReqEndpoint; }


}}
