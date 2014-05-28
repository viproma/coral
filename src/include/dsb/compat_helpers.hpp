// This file contains functionality that helps with compiler compatibility.
// It should only be included in .cpp files, never in .hpp files.
#ifndef DSB_COMPAT_HELPERS_HPP
#define DSB_COMPAT_HELPERS_HPP

#include <memory>

// VS2010 does not have std::make_unique(), so we define it ourselves.
// Why do we want this so badly?  Check out section 3 here:
// http://herbsutter.com/2013/05/29/gotw-89-solution-smart-pointers/
#if defined(_MSC_VER) && _MSC_VER < 1800
    namespace std
    {
        template<typename T>
        unique_ptr<T> make_unique() { return unique_ptr<T>(new T()); }

        template<typename T, typename A1>
        unique_ptr<T> make_unique(A1&& arg1) { return unique_ptr<T>(new T(arg1)); }

        template<typename T, typename A1, typename A2>
        unique_ptr<T> make_unique(A1&& arg1, A2&& arg2) { return unique_ptr<T>(new T(arg1, arg2)); }

        template<typename T, typename A1, typename A2, typename A3>
        unique_ptr<T> make_unique(A1&& arg1, A2&& arg2, A3&& arg3) { return unique_ptr<T>(new T(arg1)); }

        // ...continue adding more as necessary
    }
#endif

#endif // header guard
