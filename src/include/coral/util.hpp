/**
\file
\brief Main header file for coral::util.
\copyright
    Copyright 2013-present, SINTEF Ocean.
    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
#ifndef CORAL_UTIL_HPP
#define CORAL_UTIL_HPP

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include <coral/config.h>
#include <boost/filesystem/path.hpp>
#include <boost/noncopyable.hpp>


namespace coral
{

/// Misc. utilities (i.e., stuff that didn't really fit anywhere else).
namespace util
{


/// Encodes a 16-bit unsigned integer using little-endian byte order.
void EncodeUint16(std::uint16_t source, char target[2]);


/// Encodes a 32-bit unsigned integer using little-endian byte order.
void EncodeUint32(std::uint32_t source, char target[4]);


/// Encodes a 64-bit unsigned integer using little-endian byte order.
void EncodeUint64(std::uint64_t source, char target[8]);


/// Decodes a 16-bit unsigned integer using little-endian byte order.
std::uint16_t DecodeUint16(const char source[2]);


/// Decodes a 32-bit unsigned integer using little-endian byte order.
std::uint32_t DecodeUint32(const char source[4]);


/// Decodes a 64-bit unsigned integer using little-endian byte order.
std::uint64_t DecodeUint64(const char source[8]);


/**
\brief  Given a character array and its length, compares it lexicographically
        to a zero-terminated string.

This is equivalent to std::strcmp, except that it only requires one of the
arrays (`stringz`) to be null-terminated.
*/
int ArrayStringCmp(const char* array, size_t length, const char* stringz);


/// Returns a string that contains a random UUID.
std::string RandomUUID();


/**
\brief  Creates a random string.

This creates a string of the given size by randomly selecting characters from
`charSet`, which must be a null-terminated string.

\throws std::invalid_argument
    If `charSet` is null or empty.
*/
std::string RandomString(size_t size, const char* charSet);

/**
\brief  Returns the current UTC time in the ISO 8601 "basic" format.

This returns a string on the form `yyyymmddThhmmssZ` (where the lower-case
letters represent the date/time numbers).
*/
std::string Timestamp();


/**
\brief  Moves a value, replacing it with another one.

This function works just like `std::move`, except that it only works on lvalues
and assigns an explicit value to the variable which is being moved from.  This
is inefficient for types that provide move semantics (`std::move` should be used
for those), but useful for e.g. built-in types.
*/
template<typename T1, typename T2>
T1 MoveAndReplace(T1& variable, const T2& replacement)
{
    auto tmp = std::move(variable);
    variable = replacement;
    return std::move(tmp);
}

/**
\brief  Overload of MoveAndReplace() that replaces `variable` with a
        default-constructed value.
*/
template<typename T>
T MoveAndReplace(T& variable)
{
    return MoveAndReplace(variable, T());
}


/**
\brief  Calls the given function(-like object), but only after swapping it with
        a default-constructed one.

This is useful for function objects that may only be called once, such as
one-shot callbacks.

`LastCall(f, x)` is equivalent to the following:
~~~{.cpp}
F tmp;
swap(f, tmp);
tmp(x);
~~~
Thus, `f` will be left in its default-constructed state even if it throws.
(However, if the default constructor throws in the first line, `f` will
never be called at all.)
*/
template<typename F>
void LastCall(F& f)
{
    F tmp;
    swap(f, tmp);
    tmp();
}

template<typename F, typename... Args>
void LastCall(F& f, Args&&... args)
{
    F tmp;
    swap(f, tmp);
    tmp(std::forward<Args>(args)...);
}


template<typename Action>
class ScopeGuard : boost::noncopyable
{
public:
    explicit ScopeGuard(Action action) : m_active(true), m_action(action) { }

    ScopeGuard(ScopeGuard&& other)
        : m_active(MoveAndReplace(other.m_active, false)),
          m_action(std::move(other.m_action))
    { }

    ScopeGuard& operator=(ScopeGuard&& other)
    {
        m_active = MoveAndReplace(other.m_active, false);
        m_action = std::move(other.m_action);
    }

    ~ScopeGuard() { if (m_active) m_action(); }

private:
    bool m_active;
    Action m_action;
};

/**
\brief  Scope guard.

This function creates a generic RAII object that will execute a user-defined
action on scope exit.
~~~{.cpp}
void Foo()
{
    // DoSomething() will always be called before Foo() returns.
    auto cleanup = OnScopeExit([]() { DoSomething(); });
    // ...
}
~~~
*/
template<typename Action>
ScopeGuard<Action> OnScopeExit(Action action) { return ScopeGuard<Action>(action); }


/// Options that control how new processes are created.
enum class ProcessOptions
{
    none = 0,

    /// Create a new console window for the process (Windows only)
    createNewConsole = 1
};
CORAL_DEFINE_BITWISE_ENUM_OPERATORS(ProcessOptions)


/**
\brief  Starts a new process.

Windows warning: This function only supports a very limited form of argument
quoting.  The elements of args may contain spaces, but no quotation marks or
other characters that are considered "special" in a Windows command line.
*/
void SpawnProcess(
    const std::string& program,
    const std::vector<std::string>& args,
    ProcessOptions options = ProcessOptions::none);


/**
\brief  Returns the path of the current executable.
\throws std::runtime_error if the path could for some reason not be determined.
*/
boost::filesystem::path ThisExePath();


}}      // namespace
#endif  // header guard
