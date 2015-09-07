/*
This header file is meant for internal use in this library, and should
normally not be included directly by client code.  Its purpose is to
aid in maintaining a cross-platform code base.

Note to contributors:
This file intentionally has a ".h" extension, as it is supposed to
be a valid C header.  C++-specific code should therefore be placed in
#ifdef __cplusplus blocks.
*/
#ifndef DSB_CONFIG_H
#define DSB_CONFIG_H

// Unified GCC version macro
#ifdef __GNUC__
#   ifdef __GNUC_PATCHLEVEL__
#       define DSB_GNUC_VERSION (__GNUC__*100000 + __GNUC_MINOR__*100 + __GNUC_PATCHLEVEL__)
#   else
#       define DSB_GNUC_VERSION (__GNUC__*100000 + __GNUC_MINOR__*100)
#   endif
#endif

// Microsoft Visual C++ version macros
#ifdef _MSC_VER
#   define DSB_MSC10_VER 1600
#   define DSB_MSC11_VER 1700
#   define DSB_MSC12_VER 1800
#endif

// The 'final' keyword (C++11) was introduced in Visual Studio 2012.
// Before that (since VS2005), 'sealed' was used for the same purpose.
#ifdef __cplusplus
#   if defined(_MSC_VER) && _MSC_VER < DSB_MSC11_VER
#       define DSB_FINAL sealed
#   else
#       define DSB_FINAL final
#   endif
#endif

// The 'noexcept' keyword (C++11) is not yet supported by Visual Studio.
#ifdef __cplusplus
#   ifdef _MSC_VER
#       define DSB_NOEXCEPT throw()
#   else
#       define DSB_NOEXCEPT noexcept
#   endif
#endif

// The 'noreturn' attribute: C++11, supported by GCC >= 4.8
#ifdef __cplusplus
#   if (__cplusplus >= 201103L) || (defined(__GNUC__) && (DSB_GNUC_VERSION >= 40800))
#       define DSB_NORETURN [[noreturn]]
#   elif defined(__GNUC__)
#       define DSB_NORETURN __attribute__ ((noreturn))
#   elif defined(_MSC_VER)
#       define DSB_NORETURN __declspec(noreturn)
#   else
#       define DSB_NORETURN
#   endif
#endif

// Visual Studio versions prior to 2012 have a different emplace_back()
// signature than the standard one, due to lack of support for variadic
// templates.
#ifdef __cplusplus
#   if defined(_MSC_VER) && _MSC_VER < DSB_MSC11_VER
#       define DSB_USE_MSVC_EMPLACE_WORKAROUND 1
#   else
#       define DSB_USE_MSVC_EMPLACE_WORKAROUND 0
#   endif
#endif

// Visual Studio does not support the =default and =delete constructor attributes.
#ifdef __cplusplus
#   if (__cplusplus >= 201103L) || (defined(__GNUC__) && (DSB_GNUC_VERSION >= 40400))
#       define DSB_HAS_EXPLICIT_DEFAULTED_DELETED_FUNCS 1
#   else
#       define DSB_HAS_EXPLICIT_DEFAULTED_DELETED_FUNCS 0
#   endif
#endif


#endif // header guard
