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


/// Decodes a 16-bit unsigned integer using little-endian byte order.
uint16_t DecodeUint16(const char source[2]);


/// Returns a string that contains a random UUID.
std::string RandomUUID();


/**
\brief  Moves a value, replacing it with another one.

This function works just like `std::move`, except that it only works on lvalues
and assigns an explicit value to the variable which is being moved from.  This
is inefficient for types that provide move semantics (`std::move` should be used
for those), but useful for e.g. built-in types.
*/
template<typename T>
T SwapOut(T& variable, const T& replacement = T())
{
    auto tmp = std::move(variable);
    variable = replacement;
    return std::move(tmp);
}


template<typename Action>
class ScopeGuard : boost::noncopyable
{
public:
    ScopeGuard(Action action) : m_action(action) { }
    ~ScopeGuard() { m_action(); }
private:
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
