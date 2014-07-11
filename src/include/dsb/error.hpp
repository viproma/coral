/**
\file
\brief  Main header file for dsb::error.
*/
#ifndef DSB_ERROR_HPP
#define DSB_ERROR_HPP

#include <sstream>
#include <stdexcept>


namespace dsb
{

/// Exception types and error handling facilities.
namespace error
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


/**
\def    DSB_INPUT_CHECK(test)
\brief  Checks the value of one or more function input parameters, and
        throws an `std::invalid_argument` if they do not fulfill the
        given requirements.

Example:

    void Foo(int x)
    {
        SFH_INPUT_CHECK(x > 0);
        ...
    }

If the above fails, i.e. if `x <= 0`, an exception will be thrown with
the following error message:

    Foo: Input requirement not satisfied: x > 0

This obviates the need to type redundant and tedious stuff like

    if (x <= 0) throw std::invalid_argument("x must be greater than zero");

To ensure consistent, clear and understandable exceptions, the following
guidelines should be observed when using this macro:

  - The test expression should only include input parameters of the
    function/method in question, as well as literals and user-accessible
    symbols.  (For example, a requirement that `x > m_foo` is rather
    difficult for the user to comply with if `m_foo` is a private member
    variable.)
  - Since `std::invalid_argument` is a subclass of `std::logic_error`,
    this macro should only be used to catch logic errors, i.e. errors that
    are avoidable by design.  (For example, `!fileName.empty()` is probably
    OK, but `exists(fileName)` is not, since the  latter can only be
    verified at runtime.)
  - Use descriptive parameter names (e.g. `name` instead of `n`).
  - Use the same parameter names in the header and in the implementation.
  - Keep test expressions simple. Complicated expressions can often be
    written as separate tests.

If, for some reason, any of the above is not possible, consider writing
your own specialised `if`/`throw` clause.

In general, it is important to keep in mind who is the target audience for
the exception and its accompanying error message: namely, other developers
who will be using your function, and who will be using the exception to
debug their code.

\param[in] test An expression which can be implicitly converted to `bool`.
*/
#define DSB_INPUT_CHECK(test)                                                  \
    do {                                                                       \
        if (!(test)) {                                                         \
            dsb::error::InternalThrow<std::invalid_argument >                  \
                (__FUNCTION__, -1, "Input requirement not satisfied", #test);  \
        }                                                                      \
    } while(false)


namespace
{
    // Internal helper function.
    // This function is only designed for use by the macros in this header,
    // and it is subject to change without warning at any time.
    template<class ExceptionT>
    inline void InternalThrow(const char* location,
                              int lineNo,
                              const char* msg,
                              const char* detail)
    {
        std::stringstream s;
        s << location;
        if (lineNo >= 0) s << '(' << lineNo << ')';
        s << ": " << msg;
        if (detail) s << ": " << detail;
        throw ExceptionT(s.str());
    }
}


}}      // namespace
#endif  // header guard
