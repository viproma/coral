**NOTE:** Coral is no longer maintained, as it has been superseded by the [_Open Simulation Platform_](https://opensimulationplatform.com). More specifically, the library part of Coral has been superseded by [_libcosim_](https://github.com/open-simulation-platform/libcosim), while the [_cosim_](https://github.com/open-simulation-platform/cosim-cli) CLI is a replacement for _coralmaster_. (_coralslave_ and _coralslaveprovider_ don't have direct equivalents, as _cosim_ does things slightly differently.)

Coral
=====
Coral is **free and open-source co-simulation software** built from the ground up with support for [FMI](https://fmi-standard.org) and distributed simulations in mind. It is primarily a **C++ library** that can be embedded into any application that needs to perform co-simulations. However, we've also made some simple **command-line applications** for testing, demonstration and research purposes.

Coral was developed as part of the R&D project [Virtual Prototyping of Maritime Systems and Operations](http://viproma.no) (ViProMa) and maintained by [SINTEF Ocean](http://www.sintef.no/en/ocean/).

Terms of use
------------
Coral is free and open-source software released under the terms of the
[Mozilla Public License v. 2.0](http://mozilla.org/MPL/2.0/). For more
information, see the [MPL 2.0 FAQ](https://www.mozilla.org/en-US/MPL/2.0/FAQ/).

Documentation
-------------
[Browse the API documentation online.](https://viproma.github.io/coral)

Build requirements
------------------
The version/release numbers specified for compilers, tools and libraries below
are the lowest ones used for the official Coral builds, and are therefore known
to work.  Other versions are likely to work too, especially if they are newer
or at least have the same major release number, but this is not guaranteed.

Supported platforms and compilers:

  - Windows: Visual Studio 2015 or newer.
  - Linux:   GCC 4.9 or newer.

Required build tools:

  - [CMake](http://cmake.org) v3.6, to generate the build system.
  - The [Protocol Buffers](https://developers.google.com/protocol-buffers/)
    compiler, to parse the protocol buffer files and generate C++ code for them.
  - [Doxygen](http://doxygen.org), to generate API documentation (optional).

Required libraries:

  - [Boost](http://boost.org) v1.55.0
  - [ZeroMQ](http://zeromq.org) v4.0 (including
    [cppzmq](https://github.com/zeromq/cppzmq), which may not be included in
    the default installation for all platforms)
  - [FMI Library](http://jmodelica.org/FMILibrary) v2.0.3
  - [Protocol Buffers](https://developers.google.com/protocol-buffers/) v2.6
  - [libzip](http://www.nih.at/libzip/) v1.1
  - [zlib](http://www.zlib.net/) v1.2 (a dependency of libzip)

Optional libraries (only necessary if you want to build and run tests):

  - [Google Test](https://github.com/google/googletest)

The recommended way to obtain all these, which works on all supported
platforms, is to use [vcpkg](https://github.com/Microsoft/vcpkg) and install
the packages listed in [`vcpkg-deps.txt`](./vcpkg-deps.txt) and optionally
[`vcpkg-test-deps.txt`](./vcpkg-test-deps.txt).


Building
--------
Coral is built using a fairly standard CMake procedure, so we refer to the
[CMake documentation](http://cmake.org/cmake/help/documentation.html) for
details and advanced usage, and only give a quick walk-through of the procedure
here.

First, in a terminal/command-line window, change to the root source directory
(i.e., the one which contains the present README file), and enter the following
commands:

    mkdir build
    cd build
    cmake ..

This will locate dependencies and generate the platform-specific build system.
Next, build the software by entering the following command (still within the
`build` directory):

    cmake --build .

Finally, it is a good idea to run the tests, to see that everything works as
it should.  The command for this is (unfortunately) platform dependent:

    cmake --build . --target RUN_TESTS          &:: Windows
    cmake --build . --target test               #   Linux
