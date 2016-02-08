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

#include "dsb/config.h"
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


/**
\brief  An RAII object that creates a unique directory on construction
        and recursively deletes it again on destruction.
*/
class TempDir
{
public:
    /**
    \brief  Creates a new temporary directory.

    The name of the new directory will be randomly generated, and there are
    three options of where it will be created, depending on the value of
    `parent`.  In the following, `temp` refers to a directory suitable for
    temporary files under the conventions of the operating system (e.g. `/tmp`
    under UNIX-like systems), and `name` refers to the randomly generated
    name mentioned above.

      - If `parent` is empty: `temp/name`
      - If `parent` is relative: `temp/parent/name`
      - If `parent` is absolute: `parent/name`
    */
    explicit TempDir(
        const boost::filesystem::path& parent = boost::filesystem::path());

    TempDir(const TempDir&) = delete;
    TempDir& operator=(const TempDir&) = delete;

    /**
    \brief  Move constructor.

    Ownership of the directory is transferred from `other` to `this`.
    Afterwards, `other` no longer refers to any directory, meaning that
    `other.Path()` will return an empty path, and its destructor will not
    perform any filesystem operations.
    */
    TempDir(TempDir&& other) DSB_NOEXCEPT;

    /// Move assignment operator. See TempDir(TempDir&&) for semantics.
    TempDir& operator=(TempDir&&) DSB_NOEXCEPT;

    /// Destructor.  Recursively deletes the directory.
    ~TempDir() DSB_NOEXCEPT;

    /// Returns the path to the directory.
    const boost::filesystem::path& Path() const;

private:
    void DeleteNoexcept() DSB_NOEXCEPT;

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
