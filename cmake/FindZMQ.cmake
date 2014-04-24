# Finds ZeroMQ, sets ZMQ_LIBRARIES and ZMQ_INCLUDE_DIRS
cmake_minimum_required (VERSION 2.8.11)

unset (ZMQ_LIBRARIES)
unset (ZMQ_INCLUDE_DIRS)
unset (ZMQ_VERSION_STRING)

set (_canFind TRUE)
set (_extraPrefixPaths)
if (WIN32)
    # ZMQ may be, and usually is, installed in a folder below the standard prefix.
    foreach (_prefix ${CMAKE_SYSTEM_PREFIX_PATH})
        file (GLOB _dirs "${_prefix}/zeromq*" "${_prefix}/zmq*")
        list (APPEND _extraPrefixPaths ${_dirs})
        unset (_dirs)
    endforeach ()

    # On Windows, ZMQ uses a library file naming scheme that includes
    # compiler versions, release/debug mode, etc.
    if (MSVC90)
        set (_compiler "v90")
    elseif (MSVC10)
        set (_compiler "v100")
    elseif (MSVC11)
        set (_compiler "v110")
    elseif (MSVC12)
        set (_compiler "v120")
    else ()
        set (_canFind FALSE)
        message (WARNING "Compiler version not supported by the FindZMQ module; ZMQ not found.")
    endif ()

    # If version is not requested, assume lowest supported version
    if (NOT ZMQ_FIND_VERSION)
        set (ZMQ_FIND_VERSION_MAJOR 3)
        set (ZMQ_FIND_VERSION_MINOR 0)
        set (ZMQ_FIND_VERSION_PATCH 0)
    endif ()

    # Generate a list of all possible library file names.

    if ((ZMQ_FIND_VERSION_MAJOR GREATER 2) AND (ZMQ_FIND_VERSION_MAJOR LESS 5))
        if ("${ZMQ_FIND_MAX_VERSION_MINOR}" STREQUAL "")
            set (ZMQ_FIND_MAX_VERSION_MINOR 5)
        endif ()
        if ("${ZMQ_FIND_MAX_VERSION_PATCH}" STREQUAL "")
            set (ZMQ_FIND_MAX_VERSION_PATCH 9)
        endif ()
        set (_releaseLibs)
        set (_debugLibs)
        foreach (_majorVer RANGE ${ZMQ_FIND_VERSION_MAJOR} 4)
            foreach (_minorVer RANGE ${ZMQ_FIND_VERSION_MINOR} ${ZMQ_FIND_MAX_VERSION_MINOR})
                foreach (_patchVer RANGE ${ZMQ_FIND_VERSION_PATCH} ${ZMQ_FIND_MAX_VERSION_PATCH})
                    set (_version "${_majorVer}_${_minorVer}_${_patchVer}")
                    list (APPEND _releaseLibs "libzmq-${_compiler}-mt-${_version}")
                    list (APPEND _debugLibs   "libzmq-${_compiler}-mt-gd-${_version}")
                    unset (_version)
                endforeach ()
            endforeach ()
        endforeach ()
        message (STATUS "Searching for ZMQ. This may take a while...")
    else ()
        set (_canFind FALSE)
        message (WARNING "ZMQ version not supported by the FindZMQ module: ${ZMQ_FIND_VERSION}")
    endif ()
else ()
    # On *NIX, it's just "libzmq" as usual.
    set (_releaseLibs "zmq")
    set (_debugLibs ${_releaseLibs})
endif ()

if (_canFind)
    find_library (_libzmq_release
        NAMES ${_releaseLibs}
        PATHS $ENV{ZMQ_DIR} ${_extraPrefixPaths}
        PATH_SUFFIXES "lib")
    find_library (_libzmq_debug
        NAMES ${_debugLibs}
        PATHS $ENV{ZMQ_DIR} ${_extraPrefixPaths}
        PATH_SUFFIXES "lib")
    find_path (ZMQ_INCLUDE_DIRS "zmq.h"
        PATHS $ENV{ZMQ_DIR} ${_extraPrefixPaths}
        PATH_SUFFIXES "include")

    if (_libzmq_release OR _libzmq_debug)
        add_library("zmq" SHARED IMPORTED)
        set_target_properties ("zmq" PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${ZMQ_INCLUDE_DIRS}"
        )
        if (_libzmq_release)
            set_property (TARGET "zmq" APPEND PROPERTY IMPORTED_CONFIGURATIONS "RELEASE")
            set_target_properties ("zmq" PROPERTIES
                IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "C"
                IMPORTED_LOCATION_RELEASE "${_libzmq_release}"
            )
        endif ()
        if (_libzmq_debug)
            set_property (TARGET "zmq" APPEND PROPERTY IMPORTED_CONFIGURATIONS "DEBUG")
            set_target_properties ("zmq" PROPERTIES
                IMPORTED_LINK_INTERFACE_LANGUAGES_DEBUG "C"
                IMPORTED_LOCATION_DEBUG "${_libzmq_debug}"
            )
        endif ()
        set (ZMQ_LIBRARIES "zmq")
    endif ()
    unset (_libzmq_release)
    unset (_libzmq_debug)
endif ()

# Determine ZMQ version from the #defines in zmq.h
if (ZMQ_INCLUDE_DIRS)
    file (STRINGS "${ZMQ_INCLUDE_DIRS}/zmq.h" _versionDefines REGEX "define +ZMQ_VERSION_")
    if (_versionDefines)
        string (REGEX REPLACE
            ".*MAJOR ([0-9]+).*MINOR ([0-9]+).*PATCH ([0-9]+).*"
            "\\1.\\2.\\3" ZMQ_VERSION_STRING "${_versionDefines}")
    endif ()
endif ()

include (FindPackageHandleStandardArgs)
find_package_handle_standard_args (ZMQ
    FOUND_VAR ZMQ_FOUND
    REQUIRED_VARS ZMQ_LIBRARIES ZMQ_INCLUDE_DIRS
    VERSION_VAR ZMQ_VERSION_STRING)
