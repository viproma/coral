# ==============================================================================
# Find YAML-CPP
#
# This module searches for the YAML-CPP library and header files, using
# pkg-config if available.  If found, the following variables are defined:
#
#     YAMLCPP_FOUND        - True
#
#     YAMLCPP_LIBRARIES    - The name of an imported target which contains
#                            references to both the library file and the
#                            include directory
#
#     YAMLCPP_INCLUDE_DIRS - The include directory
#
# If pkg-config was available and able to find the library, the following
# variables may be defined (otherwise they are empty):
#
#     YAMLCPP_VERSION      - Library version
#
#     YAMLCPP_DEFINITIONS  - Additional compiler flags
#
# ==============================================================================

# Use pkg-config if possible
find_package (PkgConfig QUIET)
if (PKG_CONFIG_FOUND)
    pkg_check_modules (_yamlcpp_pc QUIET "yaml-cpp")
endif ()
set (YAMLCPP_DEFINITIONS ${_yamlcpp_pc_CFLAGS_OTHER})
set (YAMLCPP_VERSION     ${_yamlcpp_pc_VERSION})

# Additional search paths on Windows, since files are typically not placed
# directly under the prefix paths.
set (_yamlcpp_extraSearchPaths)
if (WIN32)
    foreach (p ${CMAKE_PREFIX_PATH} ${CMAKE_SYSTEM_PREFIX_PATH})
        file (GLOB d "${p}/*yaml*")
        list (APPEND _yamlcpp_extraSearchPaths ${d})
    endforeach ()
endif ()

# Construct the HINTS directive for find_XXX()
set (_yamlcpp_findHints
    $ENV{YAMLCPP_DIR}
    ${_yamlcpp_pc_INCLUDEDIR}
    ${_yamlcpp_pc_INCLUDE_DIRS}
    ${_yamlcpp_pc_LIBDIR}
    ${_yamlcpp_pc_LIBRARY_DIRS}
    ${_yamlcpp_extraSearchPaths}
)
if (_yamlcpp_findHints)
    set (_yamlcpp_findHints "HINTS" ${_yamlcpp_findHints})
endif ()

# Find files
find_path (YAMLCPP_INCLUDE_DIR "yaml-cpp/yaml.h"
    ${_yamlcpp_findHints}
    PATH_SUFFIXES "include"
)
find_library (YAMLCPP_LIBRARY "yaml-cpp"
    ${_yamlcpp_findHints}
    PATH_SUFFIXES "lib"
)

# Define imported target
add_library ("yaml-cpp" UNKNOWN IMPORTED)
set_target_properties ("yaml-cpp" PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${YAMLCPP_INCLUDE_DIR}"
    IMPORTED_CONFIGURATIONS "RELEASE"
    IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "CXX"
    IMPORTED_LOCATION_RELEASE "${YAMLCPP_LIBRARY}"
)

# Define standard "find script" variables
set (YAMLCPP_INCLUDE_DIRS ${YAMLCPP_INCLUDE_DIR})
set (YAMLCPP_LIBRARIES    "yaml-cpp")

# Handle standard args
set (_yamlcpp_versionCheck)
if (YAMLCPP_VERSION)
    set (_yamlcpp_versionCheck "VERSION_VAR" "YAMLCPP_VERSION")
endif ()
include (FindPackageHandleStandardArgs)
find_package_handle_standard_args("YAMLCPP"
    REQUIRED_VARS YAMLCPP_INCLUDE_DIRS YAMLCPP_LIBRARIES
    ${_yamlcpp_versionCheck}
)
