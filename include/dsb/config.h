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
//          DSB_DEFINE_DEFAULT_MOVE(MyClass, x, y)
//      }
//
// It is crucial that *all* members be included as arguments to the macro,
// or they will simply not be moved.
#ifdef __cplusplus
#   if defined(_MSC_VER)
        // This is a slightly modified version of a trick which is explained
        // in detail here: http://stackoverflow.com/a/16683147
#       define DSB_EVALUATE_MACRO(code) code
#       define DSB_CONCATENATE_MACROS(A, B) A ## B
#       define DSB_BUILD_MACRO_NAME(PREFIX, SUFFIX) DSB_CONCATENATE_MACROS(PREFIX ## _, SUFFIX)
#       define DSB_VA_SHIFT(_1, _2, _3, _4, _5, thats_the_one, ...) thats_the_one
#       define DSB_VA_SIZE(...) DSB_EVALUATE_MACRO(DSB_VA_SHIFT(__VA_ARGS__, 5, 4, 3, 2, 1))
#       define DSB_SELECT(PREFIX, ...) DSB_BUILD_MACRO_NAME(PREFIX, DSB_VA_SIZE(__VA_ARGS__))(__VA_ARGS__)

#       define DSB_MOVE_CTOR_INITIALISER(...) DSB_SELECT(DSB_MOVE_CTOR_INITIALISER, __VA_ARGS__)
#       define DSB_MOVE_CTOR_INITIALISER_1(m)                  m(std::move(other.m))
#       define DSB_MOVE_CTOR_INITIALISER_2(m1, m)              DSB_MOVE_CTOR_INITIALISER_1(m1), m(std::move(other.m))
#       define DSB_MOVE_CTOR_INITIALISER_3(m1, m2, m)          DSB_MOVE_CTOR_INITIALISER_2(m1, m2), m(std::move(other.m))
#       define DSB_MOVE_CTOR_INITIALISER_4(m1, m2, m3, m)      DSB_MOVE_CTOR_INITIALISER_3(m1, m2, m3), m(std::move(other.m))
#       define DSB_MOVE_CTOR_INITIALISER_5(m1, m2, m3, m4, m)  DSB_MOVE_CTOR_INITIALISER_4(m1, m2, m3, m4), m(std::move(other.m))

#       define DSB_MOVE_OPER_ASSIGNMENT(...) DSB_SELECT(DSB_MOVE_OPER_ASSIGNMENT, __VA_ARGS__)
#       define DSB_MOVE_OPER_ASSIGNMENT_1(m)                   m = std::move(other.m);
#       define DSB_MOVE_OPER_ASSIGNMENT_2(m1, m)               DSB_MOVE_OPER_ASSIGNMENT_1(m1) m = std::move(other.m);
#       define DSB_MOVE_OPER_ASSIGNMENT_3(m1, m2, m)           DSB_MOVE_OPER_ASSIGNMENT_2(m1, m2) m = std::move(other.m);
#       define DSB_MOVE_OPER_ASSIGNMENT_4(m1, m2, m3, m)       DSB_MOVE_OPER_ASSIGNMENT_3(m1, m2, m3) m = std::move(other.m);
#       define DSB_MOVE_OPER_ASSIGNMENT_5(m1, m2, m3, m4, m)   DSB_MOVE_OPER_ASSIGNMENT_4(m1, m2, m3, m4) m = std::move(other.m);

#       define DSB_DEFINE_INLINE_MOVE_CTOR(ClassName, ...) \
            ClassName(ClassName&& other) DSB_NOEXCEPT : DSB_MOVE_CTOR_INITIALISER(__VA_ARGS__) { }
#       define DSB_DEFINE_INLINE_MOVE_OPER(ClassName, ...) \
            ClassName& operator=(ClassName&& other) DSB_NOEXCEPT { DSB_MOVE_OPER_ASSIGNMENT(__VA_ARGS__) return *this; }

#       define DSB_DEFINE_DEFAULT_MOVE(ClassName, /* all members: */ ...) \
            DSB_DEFINE_INLINE_MOVE_CTOR(ClassName, __VA_ARGS__) \
            DSB_DEFINE_INLINE_MOVE_OPER(ClassName, __VA_ARGS__)
#   else
#       define DSB_DEFINE_DEFAULT_MOVE(ClassName, /* all members: */ ...) \
            ClassName(ClassName&&) = default; \
            ClassName& operator=(ClassName&&) = default;
#   endif
#endif


#endif // header guard
