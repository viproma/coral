#ifndef DSB_PROTOCOL_ERROR_HPP
#define DSB_PROTOCOL_ERROR_HPP

#include <stdexcept>


namespace dsb { namespace protocol
{


/// Exception thrown when communication fails due to a protocol violation.
class ProtocolViolationException : public std::runtime_error
{
public:
    explicit ProtocolViolationException(const std::string& whatArg)
        : std::runtime_error(whatArg) { }

    explicit ProtocolViolationException(const char* whatArg)
        : std::runtime_error(whatArg) { }
};


}}      // namespace
#endif  // header guard
