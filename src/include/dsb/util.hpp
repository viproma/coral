/**
\file
\brief Main header file for dsb::util.
*/
#ifndef DSB_UTIL_HPP
#define DSB_UTIL_HPP

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "boost/filesystem/path.hpp"
#include "boost/noncopyable.hpp"


namespace dsb
{

/// Misc. utilities (i.e., stuff that didn't really fit anywhere else).
namespace util
{


/// Encodes a 16-bit unsigned integer using little-endian byte order.
void EncodeUint16(uint16_t source, char target[2]);


/// Encodes a 32-bit unsigned integer using little-endian byte order.
void EncodeUint32(uint32_t source, char target[4]);


/// Decodes a 16-bit unsigned integer using little-endian byte order.
uint16_t DecodeUint16(const char source[2]);


/// Decodes a 32-bit unsigned integer using little-endian byte order.
uint32_t DecodeUint32(const char source[4]);


/// Returns a string that contains a random UUID.
std::string RandomUUID();


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
\brief  Calls the given function(-like object), but only after moving it from
        its original location.

`MoveAndCall(f, x)` is equivalent to the following:
~~~{.cpp}
auto tmp = std::move(f);
tmp(x);
~~~
This is useful for functions that may only be called once (e.g. one-shot
callbacks).
*/
template<typename F>
void MoveAndCall(F& f)
{
    auto tmp = std::move(f);
    tmp();
}

template<typename F, typename A0>
void MoveAndCall(F& f, A0&& a0)
{
    auto tmp = std::move(f);
    tmp(std::forward<A0>(a0));
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


/**
\brief  An RAII object that creates a unique directory on construction,
        and recursively deletes it again on destruction.
*/
class TempDir
{
public:
    TempDir();
    ~TempDir();
    const boost::filesystem::path& Path() const;
private:
    boost::filesystem::path m_path;
};


/**
\brief  Starts a new process.

Windows warning: This function only supports a very limited form of argument
quoting.  The elements of args may contain spaces, but no quotation marks or
other characters that are considered "special" in a Windows command line.
*/
void SpawnProcess(
    const std::string& program,
    const std::vector<std::string>& args);


/**
\brief  Returns the path of the current executable.
\throws std::runtime_error if the path could for some reason not be determined.
*/
boost::filesystem::path ThisExePath();


}}      // namespace
#endif  // header guard
