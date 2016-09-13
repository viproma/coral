Contributing guidelines
=======================

This document contains a set of rules and guidelines for everyone who wishes
to contribute to the Coral library, its auxiliary libraries and any executables
built together with it.  All of the aforementioned are hereafter referred to as
"the software".

General
-------
All contributions to the software, in the form of changes, removals or
additions to source code and other files under source control, shall be made
via pull requests.  A pull request must always be reviewed and merged by someone
other than its author.

All contributors implicitly agree to license their contribution under the same
terms as the rest of the software, and accept that those terms may change in the
future.  See the `LICENCE.txt` file for details.

Programming language
--------------------
The primary programming language is C++, specifically the subset of C++14 which
is supported by Visual Studio 2013.

Source tree structure
---------------------
The library's public API is defined by the headers in the topmost `include`
directory. All headers must be placed below the `include/coral` subdirectory,
so that `#include` directives in client code always look like this:

    #include "coral/something"

The header directory structure should match the namespace structure, so that
client code that uses things from the `coral::foo` namespace should look like

    #include "coral/foo.hpp"

or, if `coral::foo` is large enough to warrant splitting across several files,

    #include "coral/foo/bar.hpp"
    #include "coral/foo/baz.hpp"

Pure C++ header files should have a `.hpp` extension, while files which are
also designed to be included in C programs should have a `.h` extension.

Source files are located under the topmost `src` directory, with the main
Coral library sources under the `src/lib` subdirectory.
C++ source files should have `.cpp` extension.  The source file hierarchy is
generally flat, with directory/namespace names separated by underscores.
For example, the implementation of `include/coral/foo/bar.hpp` should be in
`lib/foo_bar.cpp`.

All code should have unittests, created using the Google Test framework.
Test sources must be located alongside the other sources and have a
`_test` suffix.  For the example in the previous paragraph we'd thus have
a file called `lib/foo_bar_test.cpp`.

API documentation
-----------------
The entire API should be documented, including the non-public API.
Documentation comments in [Doxygen](http://www.doxygen.org) format should
immediately precede the function/type declarations in the header files.

For unstable, experimental and/or in-development code, minimal documentation
(i.e., a brief description of what a function does or what a class represents)
is OK.  As the API matures and stabilises, higher-quality documentation is
expected.

For functions, high-quality documentation should include:

  * What the function does.
  * Parameter descriptions.
  * Return value description.
  * Which exceptions may be thrown, and under which circumstances.
  * Side effects.
  * If the function allocates/acquires resources, where ownership is not made
    explicit through the type system (e.g. raw owning pointers):

      - Transfer/acquisition of ownership.
      - Requirements or guarantees with regard to lifetime.

  * Preconditions, i.e., any assumptions made by the function about its input,
    and which are not guaranteed to be checked.  (Typically, these are checked
    in debug mode with `assert`.)

For classes, high-quality documentation should include:

  * For base classes: Requirements and guidelines for subclassing.  (Which
    methods should/must be implemented, how to use protected members, etc.)
  * If the class has reference semantics—i.e., if a copy of an object will
    refer to the same data as the original (e.g. `std::shared_ptr`)—this
    should be stated.
  * Lifetime/ownership issues not expressible through the type system.

Resource management and error handling
--------------------------------------
Resources should, with extremely few exceptions, be managed automatically using
standard types or user-defined RAII types.  Specifically, do not use explicit
`new`/`delete` or `malloc`/`free` other than in low-level code where it is
inavoidable. Use smart pointers and standard containers rather than raw
(owning) pointers.

Errors are signaled by means of exceptions.

Naming, formatting and style
----------------------------
The following are *strict rules*:

  * 4 spaces are used for indentation, never tabs.
  * Spaces are used for alignment, never tabs.
  * Files use the UNIX end-of-line convention (i.e., a single line feed, never
    a carriage return).
  * Function names are in PascalCase.
  * Variable names are in camelCase.
  * Private member variables should be named differently from local variables,
    typically using an `m_` prefix or a trailing underscore.
  * Never use the `using` directive (e.g. `using namespace foo;`) in headers.
  * Only use `using` declarations (e.g. `using foo::bar;`) in headers when the
    purpose is to declare symbols which are a part of the API.

The following are *recommendations*:

  * Avoid the `using` directive in source files too.  Prefer to use namespace
    aliases (e.g. `namespace sn = some_verbosely_named_namespace;`) instead.
  * Use ["the one true bracing style"](https://en.wikipedia.org/wiki/Indent_style#Variant:_1TBS)
  * Lines should be broken at 80 columns, except in special cases when this
    makes code particularly ugly/unreadable.

When it comes to purely aesthetic issues, the most important thing is to stay
consistent with surrounding code and not mix coding styles.

With regards to indentation and end-of-line markers, it is recommended that
contributors install the [EditorConfig plugin](http://editorconfig.org/) for
their editor/IDE.  The root source directory contains a `.editorconfig`
file with the appropriate settings.
