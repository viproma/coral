# .rst
# FindProtobufTarget
# ------------------
#
# Wraps the old-fashioned FindProtobuf script bundled with CMake, providing a
# more modern variant based on imported targets.
cmake_minimum_required(VERSION 3.0)

set(_self "ProtobufTarget")
set(_tgt "protobuf")

# We have to do the job of CMakeFindDependencyMacro/find_dependency() here,
# because FindProtobuf does not respect the convention that its variables
# should be prefixed with "Protobuf_".  (It uses all-caps, "PROTOBUF_".)
set(_findPackageArgs ${${_self}_FIND_VERSION})
if(${_self}_FIND_VERSION_EXACT)
    list(APPEND _findPackageArgs "EXACT")
endif()
if (${_self}_FIND_QUIET)
    list(APPEND _findPackageArgs "QUIET")
endif()
if (${_self}_FIND_REQUIRED)
    list(APPEND _findPackageArgs "REQUIRED")
endif()
find_package(Protobuf ${_findPackageArgs})

if(PROTOBUF_FOUND)
    string(REGEX MATCH "[.]so$" _soExtension PROTOBUF_LIBRARY)
    if(_soExtension)
        add_library(${_tgt} SHARED IMPORTED)
    else()
        add_library(${_tgt} STATIC IMPORTED)
    endif()
    set_property(TARGET ${_tgt} PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${PROTOBUF_INCLUDE_DIRS})

    if(PROTOBUF_LIBRARY)
        set_property(TARGET ${_tgt} APPEND PROPERTY IMPORTED_CONFIGURATIONS "RELEASE")
        set_property(TARGET ${_tgt} APPEND PROPERTY IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "CXX")
        set_property(TARGET ${_tgt} PROPERTY IMPORTED_LOCATION_RELEASE "${PROTOBUF_LIBRARY}")
    endif()
    if(PROTOBUF_LIBRARY_DEBUG)
        set_property(TARGET ${_tgt} APPEND PROPERTY IMPORTED_CONFIGURATIONS "DEBUG")
        set_property(TARGET ${_tgt} APPEND PROPERTY IMPORTED_LINK_INTERFACE_LANGUAGES_DEBUG "CXX")
        set_property(TARGET ${_tgt} PROPERTY IMPORTED_LOCATION_DEBUG "${PROTOBUF_LIBRARY_DEBUG}")
    endif()
    if(UNIX)
        set_property(TARGET ${_tgt} PROPERTY INTERFACE_LINK_LIBRARIES "-lpthread")
    endif()
endif()

set(${_self}_FOUND ${PROTOBUF_FOUND})
