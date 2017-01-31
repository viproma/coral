/*
Copyright 2013-2017, SINTEF Ocean and the Coral contributors.
This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at http://mozilla.org/MPL/2.0/.

NOTE TO USERS:
This header file is meant for internal use in this library, and should
normally not be included directly by client code.  Its purpose is to
aid in maintaining a cross-platform code base.

NOTE TO CONTRIBUTORS:
This file intentionally has a ".h" extension, as it is supposed to
be a valid C header.  C++-specific code should therefore be placed in
#ifdef __cplusplus blocks.
*/
#ifndef CORAL_CONFIG_H
#define CORAL_CONFIG_H

// Program name and version number
#define CORAL_PROGRAM_NAME "Coral"

#define CORAL_VERSION_MAJOR 0
#define CORAL_VERSION_MINOR 6
#define CORAL_VERSION_PATCH 0

#define CORAL_VERSION_STRINGIFY(a, b, c) #a "." #b "." #c
#define CORAL_VERSION_STRINGIFY_EXPAND(a, b, c) CORAL_VERSION_STRINGIFY(a, b, c)
#define CORAL_VERSION_STRING CORAL_VERSION_STRINGIFY_EXPAND( \
    CORAL_VERSION_MAJOR, CORAL_VERSION_MINOR, CORAL_VERSION_PATCH)

#define CORAL_PROGRAM_NAME_VERSION CORAL_PROGRAM_NAME " " CORAL_VERSION_STRING

// Unified GCC version macro
#ifdef __GNUC__
#   ifdef __GNUC_PATCHLEVEL__
#       define CORAL_GNUC_VERSION (__GNUC__*100000 + __GNUC_MINOR__*100 + __GNUC_PATCHLEVEL__)
#   else
#       define CORAL_GNUC_VERSION (__GNUC__*100000 + __GNUC_MINOR__*100)
#   endif
#endif

// Microsoft Visual C++ version macros
#ifdef _MSC_VER
#   define CORAL_MSC10_VER 1600 // VS 2010
#   define CORAL_MSC11_VER 1700 // VS 2012
#   define CORAL_MSC12_VER 1800 // VS 2013
#   define CORAL_MSC14_VER 1900 // VS 2015
#endif

// Support for 'noexcept' (C++11) was introduced in Visual Studio 2015
#ifdef __cplusplus
#   if defined(_MSC_VER) && (_MSC_VER < CORAL_MSC14_VER)
#       define CORAL_NOEXCEPT throw()
#   else
#       define CORAL_NOEXCEPT noexcept
#   endif
#endif

// Visual Studio does not support the 'noreturn' attribute
#ifdef __cplusplus
#   ifdef _MSC_VER
#       define CORAL_NORETURN __declspec(noreturn)
#   else
#       define CORAL_NORETURN [[noreturn]]
#   endif
#endif

// Visual Studio (2013 and 2015, at the time of writing) supports C++11's
// explicitly defaulted and deleted functions, BUT with the exception that
// it cannot generate default memberwise move constructors and move assignment
// operators (cf. https://msdn.microsoft.com/en-us/library/dn457344.aspx).
//
// Therefore, we define a macro to generate memberwise move operations for
// classes where such are appropriate.  For compilers that *do* have full
// support for these, the macro will just expand to the C++11 "=default"
// syntax.
//
// Usage is as follows:
//
//      class MyClass {
//          int x;
//          SomeNonCopyableType y;
//          CORAL_DEFINE_DEFAULT_MOVE(MyClass, x, y)
//      }
//
// It is crucial that *all* members be included as arguments to the macro,
// or they will simply not be moved.
#ifdef __cplusplus
#   if defined(_MSC_VER)
        // This is a slightly modified version of a trick which is explained
        // in detail here: http://stackoverflow.com/a/16683147
#       define CORAL_EVALUATE_MACRO(code) code
#       define CORAL_CONCATENATE_MACROS(A, B) A ## B
#       define CORAL_BUILD_MACRO_NAME(PREFIX, SUFFIX) CORAL_CONCATENATE_MACROS(PREFIX ## _, SUFFIX)
#       define CORAL_VA_SHIFT(_1, _2, _3, _4, _5, _6, _7, _8, _9, thats_the_one, ...) thats_the_one
#       define CORAL_VA_SIZE(...) CORAL_EVALUATE_MACRO(CORAL_VA_SHIFT(__VA_ARGS__, 9, 8, 7, 6, 5, 4, 3, 2, 1))
#       define CORAL_SELECT(PREFIX, ...) CORAL_BUILD_MACRO_NAME(PREFIX, CORAL_VA_SIZE(__VA_ARGS__))(__VA_ARGS__)

#       define CORAL_MOVE_CTOR_INITIALISER(...) CORAL_SELECT(CORAL_MOVE_CTOR_INITIALISER, __VA_ARGS__)
#       define CORAL_MOVE_CTOR_INITIALISER_1(m)                                   m(std::move(other.m))
#       define CORAL_MOVE_CTOR_INITIALISER_2(m1, m)                               CORAL_MOVE_CTOR_INITIALISER_1(m1), m(std::move(other.m))
#       define CORAL_MOVE_CTOR_INITIALISER_3(m1, m2, m)                           CORAL_MOVE_CTOR_INITIALISER_2(m1, m2), m(std::move(other.m))
#       define CORAL_MOVE_CTOR_INITIALISER_4(m1, m2, m3, m)                       CORAL_MOVE_CTOR_INITIALISER_3(m1, m2, m3), m(std::move(other.m))
#       define CORAL_MOVE_CTOR_INITIALISER_5(m1, m2, m3, m4, m)                   CORAL_MOVE_CTOR_INITIALISER_4(m1, m2, m3, m4), m(std::move(other.m))
#       define CORAL_MOVE_CTOR_INITIALISER_6(m1, m2, m3, m4, m5, m)               CORAL_MOVE_CTOR_INITIALISER_5(m1, m2, m3, m4, m5), m(std::move(other.m))
#       define CORAL_MOVE_CTOR_INITIALISER_7(m1, m2, m3, m4, m5, m6, m)           CORAL_MOVE_CTOR_INITIALISER_6(m1, m2, m3, m4, m5, m6), m(std::move(other.m))
#       define CORAL_MOVE_CTOR_INITIALISER_8(m1, m2, m3, m4, m5, m6, m7, m)       CORAL_MOVE_CTOR_INITIALISER_7(m1, m2, m3, m4, m5, m6, m7), m(std::move(other.m))
#       define CORAL_MOVE_CTOR_INITIALISER_9(m1, m2, m3, m4, m5, m6, m7, m8, m)   CORAL_MOVE_CTOR_INITIALISER_8(m1, m2, m3, m4, m5, m6, m7, m8), m(std::move(other.m))

#       define CORAL_MOVE_OPER_ASSIGNMENT(...) CORAL_SELECT(CORAL_MOVE_OPER_ASSIGNMENT, __VA_ARGS__)
#       define CORAL_MOVE_OPER_ASSIGNMENT_1(m)                                    m = std::move(other.m);
#       define CORAL_MOVE_OPER_ASSIGNMENT_2(m1, m)                                CORAL_MOVE_OPER_ASSIGNMENT_1(m1) m = std::move(other.m);
#       define CORAL_MOVE_OPER_ASSIGNMENT_3(m1, m2, m)                            CORAL_MOVE_OPER_ASSIGNMENT_2(m1, m2) m = std::move(other.m);
#       define CORAL_MOVE_OPER_ASSIGNMENT_4(m1, m2, m3, m)                        CORAL_MOVE_OPER_ASSIGNMENT_3(m1, m2, m3) m = std::move(other.m);
#       define CORAL_MOVE_OPER_ASSIGNMENT_5(m1, m2, m3, m4, m)                    CORAL_MOVE_OPER_ASSIGNMENT_4(m1, m2, m3, m4) m = std::move(other.m);
#       define CORAL_MOVE_OPER_ASSIGNMENT_6(m1, m2, m3, m4, m5, m)                CORAL_MOVE_OPER_ASSIGNMENT_5(m1, m2, m3, m4, m5) m = std::move(other.m);
#       define CORAL_MOVE_OPER_ASSIGNMENT_7(m1, m2, m3, m4, m5, m6, m)            CORAL_MOVE_OPER_ASSIGNMENT_6(m1, m2, m3, m4, m5, m6) m = std::move(other.m);
#       define CORAL_MOVE_OPER_ASSIGNMENT_8(m1, m2, m3, m4, m5, m6, m7, m)        CORAL_MOVE_OPER_ASSIGNMENT_7(m1, m2, m3, m4, m5, m6, m7) m = std::move(other.m);
#       define CORAL_MOVE_OPER_ASSIGNMENT_9(m1, m2, m3, m4, m5, m6, m7, m8, m)    CORAL_MOVE_OPER_ASSIGNMENT_8(m1, m2, m3, m4, m5, m6, m7, m8) m = std::move(other.m);

#       define CORAL_DEFINE_DEFAULT_MOVE_CONSTRUCTOR(ClassName, ...) \
            ClassName(ClassName&& other) CORAL_NOEXCEPT : CORAL_MOVE_CTOR_INITIALISER(__VA_ARGS__) { }
#       define CORAL_DEFINE_DEFAULT_MOVE_ASSIGNMENT(ClassName, ...) \
            ClassName& operator=(ClassName&& other) CORAL_NOEXCEPT { CORAL_MOVE_OPER_ASSIGNMENT(__VA_ARGS__) return *this; }

#   else
#       define CORAL_DEFINE_DEFAULT_MOVE_CONSTRUCTOR(ClassName, ...) \
            ClassName(ClassName&&) = default;
#       define CORAL_DEFINE_DEFAULT_MOVE_ASSIGNMENT(ClassName, ...) \
            ClassName& operator=(ClassName&&) = default;
#   endif
#   define CORAL_DEFINE_DEFAULT_MOVE(ClassName, /* all members: */ ...) \
        CORAL_DEFINE_DEFAULT_MOVE_CONSTRUCTOR(ClassName, __VA_ARGS__) \
        CORAL_DEFINE_DEFAULT_MOVE_ASSIGNMENT(ClassName, __VA_ARGS__)
#endif


#ifdef __cplusplus
#   define CORAL_DEFINE_BITWISE_ENUM_OPERATORS(EnumName) \
        inline EnumName operator|(EnumName a, EnumName b) { \
            return static_cast<EnumName>(static_cast<int>(a) | static_cast<int>(b)); } \
        inline EnumName operator&(EnumName a, EnumName b) { \
            return static_cast<EnumName>(static_cast<int>(a) & static_cast<int>(b)); } \
        inline EnumName& operator|=(EnumName& a, EnumName b) { \
            *reinterpret_cast<int*>(&a) |= static_cast<int>(b); \
            return a; } \
        inline EnumName& operator&=(EnumName& a, EnumName b) { \
            *reinterpret_cast<int*>(&a) &= static_cast<int>(b); \
            return a; }
#endif

// This is as good a place as any to put top-level documentation.
/**
\mainpage

Coral is a C++ library for performing distributed co-simulations, built from the
ground up with [FMI](https://www.fmi-standard.org) support in mind.  As such,
it is based on a master/slave model of control and communication.

If you are implementing a simulation *master*, i.e., the entity that controls
the whole simulation, check out the stuff in coral::master.

If you are implementing a simulation *slave*, aka. sub-simulator, have a
look at coral::slave.

If you are implementing a *slave provider*, a type of server software which
is responsible for spawning new slaves at the request of a master, coral::provider
is for you.
*/

#endif // header guard
