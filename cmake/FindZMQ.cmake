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


# A function which prefixes all items in a list with a string.
function (_prefixStrings targetVarName prefix)
    set (v)
    foreach (s ${ARGN})
        list (APPEND v "${prefix}${s}")
    endforeach ()
    set (${targetVarName} ${v} PARENT_SCOPE)
endfunction ()

# Generates a HINTS directive for a find_XXX command, based on
# a known file path.
function (_getHintsDirective targetVarName knownPath)
    if (existingPath)
        get_filename_component (d "${knownPath}" DIRECTORY)
        set (${targetVarName} "HINTS" "${d}/.." PARENT_SCOPE)
    else ()
        set (${targetVarName} "" PARENT_SCOPE)
    endif ()
endfunction ()


# The function which searches for the DLLs, their corresponding import
# libraries, and include files on Windows.
function (_findWinLibs releaseDll releaseLib debugDll debugLib includeDir)
    set (canFind TRUE)

    # Extra search paths (typically "C:/Program Files/ZeroMQ ...")
    set (extraPrefixPaths)
    foreach (p ${CMAKE_SYSTEM_PREFIX_PATH})
        file (GLOB d "${p}/zeromq*" "${p}/zmq*")
        list (APPEND extraPrefixPaths ${d})
    endforeach ()

    # Supported compilers
    if (MSVC90)
        set (compiler "v90")
    elseif (MSVC10)
        set (compiler "v100")
    elseif (MSVC11)
        set (compiler "v110")
    elseif (MSVC12)
        set (compiler "v120")
    else ()
        set (compiler "NOTFOUND")
        set (canFind FALSE)
        message (WARNING "Compiler version not supported by the FindZMQ module")
    endif ()

    # If no version is requested, start at 3.0.0.
    if (NOT ZMQ_FIND_VERSION)
        set (ZMQ_FIND_VERSION_MAJOR 3)
        set (ZMQ_FIND_VERSION_MINOR 0)
        set (ZMQ_FIND_VERSION_PATCH 0)
    endif ()

    # Make a list of possible version names, starting with the one we are
    # asked to search for.
    set (versionNames)
    if (ZMQ_FIND_VERSION_MAJOR EQUAL 3)
        foreach (m RANGE ${ZMQ_FIND_VERSION_MINOR} 2)
            set (pMin 0)
            if (m EQUAL ZMQ_FIND_VERSION_MINOR)
                set (pMin ${ZMQ_FIND_VERSION_PATCH})
            endif ()
            foreach (p RANGE ${pMin} 5)
                list (APPEND versionNames "3_${m}_${p}") 
            endforeach()
        endforeach ()
    endif ()
    set (mMin 0)
    if (ZMQ_FIND_VERSION_MAJOR EQUAL 4)
        set (mMin ${ZMQ_FIND_VERSION_MINOR})
    endif ()
    foreach (m RANGE ${mMin} 4)
        set (pMin 0)
        if ((ZMQ_FIND_VERSION_MAJOR EQUAL 4) AND (m EQUAL ZMQ_FIND_VERSION_MINOR))
            set (pMin ${ZMQ_FIND_VERSION_PATCH})
        endif ()
        foreach (p RANGE ${pMin} 9)
            list (APPEND versionNames "4_${m}_${p}") 
        endforeach()
    endforeach ()

    # Generate possible library file names
    if ((ZMQ_FIND_VERSION_MAJOR GREATER 2) AND (ZMQ_FIND_VERSION_MINOR LESS 5))
        _prefixStrings(releaseLibNames "libzmq-${compiler}-mt-"    ${versionNames})
        _prefixStrings(debugLibNames   "libzmq-${compiler}-mt-gd-" ${versionNames})
    else ()
        set (canFind FALSE)
        message (WARNING "ZMQ version not supported by the FindZMQ module: ${ZMQ_FIND_VERSION}")
    endif ()

    if (canFind)
        message (STATUS "Searching for ZMQ files. This may take a while...")
        set (CMAKE_FIND_LIBRARY_SUFFIXES ".lib")
        find_library (ZMQ_RELEASE_LIB
            NAMES ${releaseLibNames}
            PATHS $ENV{ZMQ_DIR} ${extraPrefixPaths}
            PATH_SUFFIXES "lib")
        _getHintsDirective(hints ${ZMQ_RELEASE_LIB})
        find_library (ZMQ_DEBUG_LIB
            NAMES ${debugLibNames}
            ${hints}
            PATHS $ENV{ZMQ_DIR} ${extraPrefixPaths}
            PATH_SUFFIXES "lib")
        set (CMAKE_FIND_LIBRARY_SUFFIXES ".dll")
        find_library (ZMQ_RELEASE_DLL
            NAMES ${releaseLibNames}
            ${hints}
            PATHS $ENV{ZMQ_DIR} ${extraPrefixPaths}
            PATH_SUFFIXES "bin" "lib")
        find_library (ZMQ_DEBUG_DLL
            NAMES ${debugLibNames}
            ${hints}
            PATHS $ENV{ZMQ_DIR} ${extraPrefixPaths}
            PATH_SUFFIXES "bin" "lib")
        find_path (ZMQ_HEADER_DIR "zmq.h"
            ${_hints}
            PATHS $ENV{ZMQ_DIR} ${extraPrefixPaths}
            PATH_SUFFIXES "include")

        set (${releaseDll} "${ZMQ_RELEASE_DLL}" PARENT_SCOPE)
        set (${releaseLib} "${ZMQ_RELEASE_LIB}" PARENT_SCOPE)
        set (${debugDll}   "${ZMQ_DEBUG_DLL}"   PARENT_SCOPE)
        set (${debugLib}   "${ZMQ_DEBUG_LIB}"   PARENT_SCOPE)
        set (${includeDir} "${ZMQ_HEADER_DIR}"  PARENT_SCOPE)
    else ()
        set (${releaseDll} "NOTFOUND" PARENT_SCOPE)
        set (${releaseLib} "NOTFOUND" PARENT_SCOPE)
        set (${debugDll}   "NOTFOUND" PARENT_SCOPE)
        set (${debugLib}   "NOTFOUND" PARENT_SCOPE)
        set (${includeDir} "NOTFOUND" PARENT_SCOPE)
    endif ()
endfunction ()

# The function which searches for the libraries and headers on *NIX.
function (_findUnixLibs library includeDir)
    find_library (ZMQ_LIBRARY "zmq"
        PATHS $ENV{ZMQ_DIR}
        PATH_SUFFIXES "lib")
    _getHintsDirective(hints ${ZMQ_LIBRARY})
    find_path (ZMQ_HEADER_DIR "zmq.h"
        ${_hints}
        PATHS $ENV{ZMQ_DIR}
        PATH_SUFFIXES "include")
    set (${library} "${ZMQ_LIBRARY}" PARENT_SCOPE)
    set (${includeDir} "${ZMQ_HEADER_DIR}"  PARENT_SCOPE)
endfunction ()

# A function which extracts the ZMQ version from macro definitions in zmq.h.
function (_getZMQVersion targetVarName includeDir)
    file (STRINGS "${includeDir}/zmq.h" versionDefines REGEX "define +ZMQ_VERSION_")
    if (versionDefines)
        string (REGEX REPLACE
            ".*MAJOR ([0-9]+).*MINOR ([0-9]+).*PATCH ([0-9]+).*"
            "\\1.\\2.\\3" versionString "${versionDefines}")
    endif ()
    set (${targetVarName} "${versionString}" PARENT_SCOPE)
endfunction ()


unset (ZMQ_LIBRARIES)
unset (ZMQ_INCLUDE_DIRS)
unset (ZMQ_VERSION_STRING)

if (WIN32)
    _findWinLibs(_releaseLocation _releaseImplib _debugLocation _debugImplib ZMQ_INCLUDE_DIRS)
    set (_releaseLinkLib _releaseImplib)
    set (_debugLinkLib _debugImplib)
else ()
    _findUnixLibs(_releaseLocation ZMQ_INCLUDE_DIRS)
    unset (_releaseImplib)
    unset (_debugLocation)
    unset (_debugImplib)
    set (_releaseLinkLib _releaseLocation)
    unset (_debugLinkLib)
endif ()

if (_releaseLinkLib OR _debugLinkLib)
    add_library ("zmq" SHARED IMPORTED)
    set (ZMQ_LIBRARIES "zmq")
    set_target_properties ("zmq" PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${ZMQ_INCLUDE_DIRS}"
    )
    if (_releaseLinkLib)
        set_property (TARGET "zmq" APPEND PROPERTY IMPORTED_CONFIGURATIONS "RELEASE")
        set_property (TARGET "zmq" PROPERTY IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "C")
        if (_releaseLocation)
            set_property (TARGET "zmq" PROPERTY IMPORTED_LOCATION_RELEASE "${_releaseLocation}")
        endif ()
        if (_releaseImplib)
            set_property (TARGET "zmq" PROPERTY IMPORTED_IMPLIB_RELEASE "${_releaseImplib}")
        endif ()
    endif ()
    if (_debugLinkLib)
        set_property (TARGET "zmq" APPEND PROPERTY IMPORTED_CONFIGURATIONS "DEBUG")
        set_property (TARGET "zmq" PROPERTY IMPORTED_LINK_INTERFACE_LANGUAGES_DEBUG "C")
        if (_debugLocation)
            set_property (TARGET "zmq" PROPERTY IMPORTED_LOCATION_DEBUG "${_debugLocation}")
        endif ()
        if (_debugImplib)
            set_property (TARGET "zmq" PROPERTY IMPORTED_IMPLIB_DEBUG "${_debugImplib}")
        endif ()
    endif ()
endif ()

if (ZMQ_INCLUDE_DIRS)
    _getZMQVersion(ZMQ_VERSION_STRING "${ZMQ_INCLUDE_DIRS}")
endif ()

include (FindPackageHandleStandardArgs)
find_package_handle_standard_args (ZMQ
    FOUND_VAR ZMQ_FOUND
    REQUIRED_VARS ZMQ_LIBRARIES ZMQ_INCLUDE_DIRS
    VERSION_VAR ZMQ_VERSION_STRING)
