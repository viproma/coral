#ifndef DSB_DOMAIN_LOCATOR
#define DSB_DOMAIN_LOCATOR

#include <string>


namespace dsb
{
namespace domain
{


class Locator
{
public:
    Locator() { }

    Locator(
        const std::string& reportEndpoint,
        const std::string& infoEndpoint,
        const std::string& execReqEndpoint);

    const std::string& ReportEndpoint() const;
    const std::string& InfoEndpoint() const;
    const std::string& ExecReqEndpoint() const;

private:
    std::string m_reportEndpoint;
    std::string m_infoEndpoint;
    std::string m_execReqEndpoint;
};


}}      // namespace
#endif  // header guard
