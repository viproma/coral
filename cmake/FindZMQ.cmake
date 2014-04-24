# - Finds ZeroMQ 3.x or 4.x
#
# The following variables are set if ZeroMQ is found:
#
#   ZMQ_FOUND           - Set to TRUE
#   ZMQ_LIBRARIES       - The name of an imported target that refers to the
#                         ZeroMQ libraries.
#   ZMQ_INCLUDE_DIRS    - The ZeroMQ include directory (which contains zmq.h).
#                         This is also added as a dependency to the imported
#                         target.
#   ZMQ_VERSION_STRING  - The version of ZeroMQ which was found
#
# If ZeroMQ was not found, ZMQ_FOUND is set to false.
#
cmake_minimum_required (VERSION 2.8.11)


# The following function generates a list of version names on the form
# [3,4]_[0,9]_[0,9], excluding versions lower than verMajor_verMinor_verPatch.
function (_generateVersionNames targetVarName verMajor verMinor verPatch)
    set (${targetVarName} "" PARENT_SCOPE)
    foreach (i RANGE ${verMajor} 4)
        if (j EQUAL verMajor)
            set (jMin ${verMinor})
        else ()
            set (jMin 0)
        endif ()
        foreach (j RANGE ${jMin} 9)
            if ((i EQUAL verMajor) AND (j EQUAL verMinor))
                set (kMin ${verPatch})
            else ()
                set (kMin 0)
            endif ()
            foreach (k RANGE ${kMin} 9)
                list (APPEND ${targetVarName} "${i}_${j}_${k}")
            endforeach()
        endforeach ()
    endforeach ()
endfunction ()

# This function prefixes all strings in a list with a string.
function (_prefixStrings targetVarName prefix)
    set (${targetVarName} "" PARENT_SCOPE)
    foreach (s ${ARGN})
        list (APPEND ${targetVarName} "${prefix}${s}")
    endforeach ()
endfunction ()

# On Windows, ZMQ uses a library file naming scheme that includes
# compiler versions, release/debug mode, etc.  In addition, libraries
# are typically installed in subfolders of the standard prefix path(s).
set (_canFind TRUE)
set (_extraPrefixPaths)
if (WIN32)
    # Compiler versions
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

    # Extra search paths
    foreach (_prefix ${CMAKE_SYSTEM_PREFIX_PATH})
        file (GLOB _dirs "${_prefix}/zeromq*" "${_prefix}/zmq*")
        list (APPEND _extraPrefixPaths ${_dirs})
        unset (_dirs)
    endforeach ()

    # Version names
    if (NOT ZMQ_FIND_VERSION)
        set (ZMQ_FIND_VERSION_MAJOR 3)
        set (ZMQ_FIND_VERSION_MINOR 0)
        set (ZMQ_FIND_VERSION_PATCH 0)
    endif ()
    if ((ZMQ_FIND_VERSION_MAJOR GREATER 2) AND (ZMQ_FIND_VERSION_MAJOR LESS 5))
        _generateVersionNames(_versionNames
            ${ZMQ_FIND_VERSION_MAJOR} ${ZMQ_FIND_VERSION_MINOR} ${ZMQ_FIND_VERSION_PATCH})
        _prefixStrings(_releaseLibs "libzmq-${_compiler}-mt-"    ${_versionNames})
        _prefixStrings(_debugLibs   "libzmq-${_compiler}-mt-gd-" ${_versionNames})
        message (STATUS "Searching for ZMQ. This may take a while...")
    else ()
        set (_canFind FALSE)
        message (WARNING "ZMQ version not supported by the FindZMQ module: ${ZMQ_FIND_VERSION}")
    endif ()
else ()
    # We don't need any special adaptations on *NIX.
    set (_releaseLibs "zmq")
    unset (_debugLibs)
endif ()

unset (ZMQ_LIBRARIES)
unset (ZMQ_INCLUDE_DIRS)
unset (ZMQ_VERSION_STRING)
if (_canFind)
    # Find "normal" library
    find_library (_libzmq_release
        NAMES ${_releaseLibs}
        PATHS $ENV{ZMQ_DIR} ${_extraPrefixPaths}
        PATH_SUFFIXES "lib")

    # Find "debug" library (Windows only)
    unset (_libzmq_debug)
    if (_debugLibs)
        find_library (_libzmq_debug
            NAMES ${_debugLibs}
            PATHS $ENV{ZMQ_DIR} ${_extraPrefixPaths}
            PATH_SUFFIXES "lib")
    endif ()

    # Find include directory
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
