/**
\file
\brief  Main header file for coral::error.
\copyright
    Copyright 2013-2017, SINTEF Ocean and the Coral contributors.
    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
#ifndef CORAL_ERROR_HPP
#define CORAL_ERROR_HPP

#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include "coral/config.h"


namespace coral
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


/// Exception thrown on an attempt to use an unsupported protocol.
class ProtocolNotSupported : public std::runtime_error
{
public:
    explicit ProtocolNotSupported(const std::string& whatArg)
        : std::runtime_error(whatArg) { }

    explicit ProtocolNotSupported(const char* whatArg)
        : std::runtime_error(whatArg) { }
};


/**
\def    CORAL_INPUT_CHECK(test)
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
#define CORAL_INPUT_CHECK(test)                                                  \
    do {                                                                       \
        if (!(test)) {                                                         \
            coral::error::detail::Throw<std::invalid_argument>                   \
                (__FUNCTION__, -1, "Input requirement not satisfied", #test);  \
        }                                                                      \
    } while(false)


/**
\brief  An exception which is used to signal that one or more of a function's
        preconditions were not met.

Note that `std::invalid_argument` should be used to signal problems with
function arguments, even though such could also, strictly speaking, be
classified as precondition violations.

\see #CORAL_PRECONDITION_CHECK
*/
class PreconditionViolation : public std::logic_error
{
public:
    explicit PreconditionViolation(const std::string& whatArg)
        : std::logic_error(whatArg) { }
};


/**
\def    CORAL_PRECONDITION_CHECK(test)
\brief  Throws a coral::error::PreconditionViolation if the given boolean
        expression evaluates to `false`.

This macro may be used to verify that a function's preconditions hold.
For example, say that you have a class `File` that looks like this:
~~~{.cpp}
class File
{
public:
    void Open(const std::string& filename);
    void Close();
    bool IsOpen() const;
};
~~~
A reasonable implementation of the `Open` method could then start like this:
~~~{.cpp}
void File::Open()
{
    CORAL_PRECONDITION_CHECK(!IsOpen());
    CORAL_INPUT_CHECK(!filename.empty());
    ...
}
~~~
If the first test fails (i.e., if a file has already been opened), an exception
of type coral::error::PreconditionViolation will be thrown, with an error message
similar to the following:

    File::Open: Precondition not satisfied: !IsOpen()

To ensure consistent, clear and understandable exceptions, the test expression
should be formulated so that it is possible for a user, who does not know
the internals of your class or function, to understand what is going on and
how to fix it.

\param[in]  test    An expression which can be implicitly converted to `bool`.
*/
#define CORAL_PRECONDITION_CHECK(test)                                        \
    do {                                                                    \
        if (!(test)) {                                                      \
            coral::error::detail::Throw<coral::error::PreconditionViolation>    \
                (__FUNCTION__, -1, "Precondition not satisfied", #test);    \
        }                                                                   \
    } while(false)


namespace detail
{
    // Internal helper function.
    // This function is only designed for use by the macros in this header,
    // and it is subject to change without warning at any time.
    template<class ExceptionT>
    inline void Throw(
        const char* location, int lineNo, const char* msg, const char* detail)
    {
        std::stringstream s;
        s << location;
        if (lineNo >= 0) s << '(' << lineNo << ')';
        s << ": " << msg;
        if (detail) s << ": " << detail;
        throw ExceptionT(s.str());
    }
}


/**
\brief  Constructs an error message by combining a user-defined message and
        a standard system error message.

The system error message is obtained by calling `std::strerror(errnoValue)`.
If `errnoValue` is zero, the function only returns `msg`.  Otherwise, if
`msg` is empty, only the system message is returned.  Otherwise, the format
of the returned message is:

    user message (system message)
*/
std::string ErrnoMessage(const std::string& msg, int errnoValue) CORAL_NOEXCEPT;


/**
\brief  Generic errors

These are for conditions that are not covered by std::errc, but which are not
specific to simulation as such.
*/
enum class generic_error
{
    /// An ongoing operation was aborted
    aborted = 1,

    /// A pending operation was canceled before it was started
    canceled,

    /// An ongoing operation failed
    operation_failed,

    /// An irrecoverable error happened
    fatal,
};

/// Error category for generic errors
const std::error_category& generic_category() CORAL_NOEXCEPT;

/// Simulation errors.
enum class sim_error
{
    /// Slave is unable to perform a time step
    cannot_perform_timestep = 1,

    /// Communications timeout between slaves
    data_timeout,
};

/// Error category for simulation errors.
const std::error_category& sim_category() CORAL_NOEXCEPT;


// Standard functions to make std::error_code and std::error_condition from
// generic_error and sim_error.
std::error_code make_error_code(generic_error e) CORAL_NOEXCEPT;
std::error_code make_error_code(sim_error e) CORAL_NOEXCEPT;
std::error_condition make_error_condition(generic_error e) CORAL_NOEXCEPT;
std::error_condition make_error_condition(sim_error e) CORAL_NOEXCEPT;


}} // namespace


namespace std
{
    // Register sim_error for implicit conversion to std::error_code
    // TODO: Should this be error_condition?
    template<>
    struct is_error_code_enum<coral::error::sim_error>
        : public true_type
    { };
}
#endif  // header guard
