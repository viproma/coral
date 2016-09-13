Coral
=====

Introduction
------------
The Virtual Prototyping Framework (VPF) is an open framework for connecting
mathematical models and simulations, from a variety of physics/engineering
domains and modelling tools, to simulate complex marine systems and operations.
The VPF is currently under development as part of the KMB ViProMa project.

Coral is a C++ library and a set of applications for performing distributed
cos-simulations, and it is a fundamental software component of the VPF.
Its job is to connect the various subsystems involved in a simulation.
More precisely, it takes care of:

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
Coral may currently be built on the following platforms, using the following
compilers:

  - Windows: Visual Studio 2013 or newer.
  - Linux:   GCC 4.9 or newer.

In addition, the following tools are needed:

  - [CMake](http://cmake.org) v3.0 or newer, to generate the build system.
  - The [Protocol Buffers](https://developers.google.com/protocol-buffers/)
    compiler, to parse the protocol buffer files and generate C++ code for them.
  - [Doxygen](http://doxygen.org), to generate API documentation (optional).

Finally, the following libraries are used by Coral and must therefore be present:

  - [Boost](http://boost.org) (currently tested with v1.58, but older and newer
    versions are likely to work as well).
  - [ZeroMQ](http://zeromq.org) v4.0 or newer.
  - [FMI Library](http://jmodelica.org/FMILibrary) v2.0 or newer.
  - [Protocol Buffers](https://developers.google.com/protocol-buffers/) v2.5 or
    newer.
  - [libzip](http://www.nih.at/libzip/) v1.1 or newer.
  - [zlib](http://www.zlib.net/)


Building
--------
Coral is built using a fairly standard CMake procedure, so we refer to the
[CMake documentation](http://cmake.org/cmake/help/documentation.html) for
details and advanced usage, and only give a quick walk-through of the procedure
here.

### Build system generation ###

In a terminal/command-line window, change to the root source directory (i.e.,
the one which contains the present README file), and enter the following
commands:

    mkdir build
    cd build
    cmake ..

The above may or may not succeed, depending on whether the various libraries and
other tools are installed in standard locations.  Look for messages that say
"not found" or similar.  (Pay special attention to Protobuf, as CMake stupidly
does not give a fatal error when that's not found, even though it's a mandatory
dependency.)  The locations of missing dependencies can usually be specified
by running CMake again, this time providing explicit paths with the -D switch.
For example, the path where the Boost libraries are installed can be specified
with the `BOOST_ROOT` variable, like this:

    cmake -DBOOST_ROOT=/path/to/boost ..

The variables that may need to be set in this manner are:

    BOOST_ROOT                  Path to Boost
    FMILIB_DIR                  Path to FMI Library
    ZMQ_DIR                     Path to ZeroMQ
    CPPZMQ_DIR                  Path to zmq.hpp
    PROTOBUF_SRC_ROOT_FOLDER    The Protocol Buffers source directory (Windows
                                only; Protobuf must be built first)
    LIBZIP_DIR                  Path to libzip
    ZLIB_ROOT                   Path to zlib

With the exception of the last one, the paths specified in this manner are
typically "prefix paths", i.e. the ones that contain `bin/`, `lib/`, `include/`
etc. for the library in question.  For more detailed information, try the
following commands:

    cmake --help-module FindDoxygen
    cmake --help-module FindBoost
    cmake --help-module FindProtobuf
    cmake --help-module FindZLIB

For details about finding ZMQ, CPPZMQ and FMI Library, please look at the
comments at the start of the relevant `cmake/FindXXX.cmake` files.

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
