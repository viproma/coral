/**
\file
\brief Functionality that helps with compiler compatibility.

This file should only be included in .cpp files, never in .hpp files.
*/
#ifndef DSB_COMPAT_HELPERS_HPP
#define DSB_COMPAT_HELPERS_HPP

#include "dsb/config.h"

// std::make_unique is a C++14 feature, first included in VS2013 and GCC 4.9
#if (__cplusplus <= 201103L) \
    && !(defined(__GNUC__) && (DSB_GNUC_VERSION >= 40900)) \
    && !(defined(_MSC_VER) && (_MSC_VER >= DSB_MSC12_VER))
#    define DSB_COMPAT_MAKE_UNIQUE_MISSING
#    include <memory>
#    include <utility>
#endif

#if defined(__cplusplus) && defined(_MSC_VER) && (_MSC_VER < 1700)
#   define DSB_COMPAT_TO_STRING_OVERLOADS_MISSING
#   include <string>
#endif


// std::make_unique() is introduced in C++14, so for non-compliant compilers,
// we have to define it ourselves.  Why do we want this so badly, you ask?
// Check out section 3 here for an excellent answer:
// http://herbsutter.com/2013/05/29/gotw-89-solution-smart-pointers/
// (TL;DR: Exception safety, mainly.)
#ifdef DSB_COMPAT_MAKE_UNIQUE_MISSING
    namespace std
    {
        template<typename T>
        unique_ptr<T> make_unique() { return unique_ptr<T>(new T()); }

        template<typename T, typename A1>
        unique_ptr<T> make_unique(A1&& arg1) { return unique_ptr<T>(new T(std::forward<A1>(arg1))); }

        template<typename T, typename A1, typename A2>
        unique_ptr<T> make_unique(A1&& arg1, A2&& arg2) { return unique_ptr<T>(new T(std::forward<A1>(arg1), std::forward<A2>(arg2))); }

        template<typename T, typename A1, typename A2, typename A3>
        unique_ptr<T> make_unique(A1&& arg1, A2&& arg2, A3&& arg3) { return unique_ptr<T>(new T(std::forward<A1>(arg1), std::forward<A2>(arg2), std::forward<A3>(arg3))); }

        template<typename T, typename A1, typename A2, typename A3, typename A4>
        unique_ptr<T> make_unique(A1&& arg1, A2&& arg2, A3&& arg3, A4&& arg4) { return unique_ptr<T>(new T(std::forward<A1>(arg1), std::forward<A2>(arg2), std::forward<A3>(arg3), std::forward<A4>(arg4))); }

        template<typename T, typename A1, typename A2, typename A3, typename A4, typename A5>
        unique_ptr<T> make_unique(A1&& arg1, A2&& arg2, A3&& arg3, A4&& arg4, A5&& arg5) { return unique_ptr<T>(new T(std::forward<A1>(arg1), std::forward<A2>(arg2), std::forward<A3>(arg3), std::forward<A4>(arg4), std::forward<A5>(arg5))); }

        // ...continue adding more as necessary
    }
#endif


// Visual Studio 2010 only defines std::to_string() for the widest signed and
// unsigned integer types, which causes ambiguity warnings when narrower types
// are used.  Here we define the missing ones.
#ifdef DSB_COMPAT_TO_STRING_OVERLOADS_MISSING
    namespace std
    {
        inline std::string to_string(int  i) { return std::to_string((long long) i); }
        inline std::string to_string(long i) { return std::to_string((long long) i); }
        inline std::string to_string(unsigned int  i) { return std::to_string((unsigned long long) i); }
        inline std::string to_string(unsigned long i) { return std::to_string((unsigned long long) i); }
    }
#endif


#endif // header guard
