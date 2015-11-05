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
#   define DSB_MSC10_VER 1600 // VS 2010
#   define DSB_MSC11_VER 1700 // VS 2012
#   define DSB_MSC12_VER 1800 // VS 2013
#   define DSB_MSC14_VER 1900 // VS 2015
#endif

// Support for 'noexcept' (C++11) was introduced in Visual Studio 2015
#ifdef __cplusplus
#   if defined(_MSC_VER) && (_MSC_VER < DSB_MSC14_VER)
#       define DSB_NOEXCEPT throw()
#   else
#       define DSB_NOEXCEPT noexcept
#   endif
#endif

// Visual Studio does not support the 'noreturn' attribute
#ifdef __cplusplus
#   ifdef _MSC_VER
#       define DSB_NORETURN __declspec(noreturn)
#   else
#       define DSB_NORETURN [[noreturn]]
#   endif
#endif

// Visual Studio does not support the =default and =delete constructor
// attributes properly.
#ifdef __cplusplus
#   ifdef _MSC_VER
#       define DSB_HAS_EXPLICIT_DEFAULTED_DELETED_FUNCS 0
#   else
#       define DSB_HAS_EXPLICIT_DEFAULTED_DELETED_FUNCS 1
#   endif
#endif


#endif // header guard
