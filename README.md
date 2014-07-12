Distributed Simulation Bus
==========================

Introduction
------------
The Virtual Prototyping Framework (VPF) is an open framework for connecting
mathematical models and simulations, from a variety of physics/engineering
domains and modelling tools, to simulate complex marine systems and operations.
The VPF is currently under development as part of the KMB ViProMa project.

The Distributed Simulation Bus (DSB) is a fundamental software component of
the VPF.  It is the lowest layer of the VPF software stack, and its job is to
connect the various subsystems involved in a simulation.  More precisely, it
takes care of:

  - *Abstraction*, in that it hides the implementation details of each subsystem
    behind a common communication interface.
  - *Initialisation*, by detecting which subsystems are available, starting the
    ones that are needed in the current simulation, connecting them with each
    other, and initialising each one.
  - *Communication*, enabling distributed simulations, where different
    subsystems may run in different processes on one computer, or on different
    computers in a network.
  - *Time synchronisation*, to ensure that all parts of a simulation follow the
    same clock.

The subsystems can be simulation tools (such as Simulink, Modelica
implementations, etc.), hardware interfaces, control systems, visualisation
tools, data loggers, and so on.


Requirements
------------
DSB may currently be built on the following platforms, using the following
compilers:

  - Windows: Visual Studio 2010 or newer.
  - Linux:   GCC 4.8 or newer.

In addition, the following tools are needed:

  - [CMake](http://cmake.org) v2.8.11 or newer, to generate the build system.
  - The [Protocol Buffers](https://code.google.com/p/protobuf/) compiler, to
    parse the protocol buffer files and generate C++ code for them.
  - [Doxygen](http://doxygen.org), to generate API documentation (optional).

Finally, the following libraries are used by DSB and must therefore be present:

  - [Boost](http://boost.org) (only tested with v1.55, but older versions may
    still work).
  - [ZeroMQ](http://zeromq.org) v3.2 or newer.
  - [Protocol Buffers](https://code.google.com/p/protobuf/) v2.5 or newer.

Tip to Debian users: You can obtain all of these by running the following
command (at least on the "testing" version):

    sudo apt-get install g++ cmake protobuf-compiler doxygen libboost-all-dev libzmq-dev libprotobuf-dev

Building
--------
The DSB is built using a fairly standard CMake procedure, so we refer to the
[CMake documentation](http://cmake.org/cmake/help/documentation.html) for
details and advanced usage, and only give a quick walk-through of the procedure
here.

### Build system generation ###

In a terminal/command-line window, change to the root source directory (i.e.,
the one which contains the present README file), and enter the following
commands:

    mkdir build
    cd build
    cmake -DDSB_BUILD_PRIVATE_API_DOCS=ON ..

The -D switch may be omitted, in which case Doxygen will only generate API
documentation for the headers in the `include` directory, and not those in
`src/include`.

The above may or may not succeed, depending on whether the various libraries and
other tools are installed in standard locations.  Look for messages that say
"not found" or similar.  (Pay special attention to Protobuf, as CMake stupidly
does not give a fatal error when that's not found, even though it's a mandatory
dependency.)  The locations of missing dependencies can usually be specified
by running CMake again, this time providing explicit paths with the -D switch.
For example:

    cmake -DBOOST_ROOT=/path/to/boost

If this becomes necessary, you may want to run one or more of the following
commands to obtain information about which variables can be set in this manner:

    cmake --help-module FindDoxygen
    cmake --help-module FindBoost
    cmake --help-module FindProtobuf
    cmake --help-module FindZMQ -DCMAKE_MODULE_PATH=../cmake

### Compilation ###

Having located all dependencies and completed the build system generation, it is
time to build the library.  Do this by entering the following command (still
within the `build` directory):

    cmake --build .

### Testing ###

Finally, it is a good idea to run the tests, to see that everything works as
it should.  The command for this is (unfortunately) platform dependent:

    cmake --build . --target RUN_TESTS          &:: Windows
    cmake --build . --target test               #   Linux

### Packaging/installation ###

[To be described later.]
