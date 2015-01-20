/**
\file
\brief Functionality that helps with compiler compatibility.

This file should only be included in .cpp files, never in .hpp files.
*/
#ifndef DSB_COMPAT_HELPERS_HPP
#define DSB_COMPAT_HELPERS_HPP

#include <memory>
#include <utility>


// std::make_unique() is introduced in C++14, so for non-compliant compilers,
// we have to define it ourselves.  Why do we want this so badly, you ask?
// Check out section 3 here for an excellent answer:
// http://herbsutter.com/2013/05/29/gotw-89-solution-smart-pointers/
// (TL;DR: Exception safety, mainly.)
#if defined(__cplusplus) && (__cplusplus <= 201103L)
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

        // ...continue adding more as necessary
    }
#endif

#endif // header guard
