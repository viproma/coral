cmake_minimum_required (VERSION 3.5)

# CMake guidelines/tips for this project:
#
# - The "Common variables" section (below) contains variables which are useful
#   not only in here in the top-level file, but in subdirectories as well.
#
# - In install() commands, use the directories specified in the "Common
#   variables" section, and avoid using absolute directories.  This makes for a
#   consistent directory structure and allows for easy packaging with CPack.
#
project ("coral")
set (projectSummary "A C++ library for distributed co-simulation")

option (CORAL_BUILD_PRIVATE_API_DOCS
        "Whether to generate API documentation for the non-public API"
        OFF)
option (CORAL_ENABLE_DEBUG_LOGGING
        "Whether to log messages of severity 'debug' also in release mode. (This is always enabled in debug mode.)"
        OFF)
option (CORAL_ENABLE_TRACE_LOGGING
        "Whether to log messages of severity 'trace' in both debug and release mode. (Implies CORAL_ENABLE_DEBUG_LOGGING.)"
        OFF)
option (FMILIB_USE_SHARED_LIB
        "Whether to link against the shared library version of FMI Library"
        ON)
option (CORAL_BUILD_TESTS
        "Whether to build tests"
        ON)

# ==============================================================================
# Common variables
# ==============================================================================

# The directory in which public headers are located.
# By "public" we mean those that are required to *use* the library, and which
# thus constitute its public API.  Internal headers should be placed under
# their respective src/ subdirectories.
set (publicHeaderDir "${CMAKE_SOURCE_DIR}/include")

# The following variable specifies where various target files should be
# installed, and must be used in all install(TARGETS) commands.
# Example:
#
#   install(TARGETS "myprog" ${targetInstallDestinations})
#
set (targetInstallDestinations ARCHIVE DESTINATION "lib"
                               LIBRARY DESTINATION "lib"
                               RUNTIME DESTINATION "bin"
                               INCLUDES DESTINATION "include")

# Special-purpose and/or platform dependent installation directories.
# Mostly useful in the present file.
set (headerInstallDir "include")
if (WIN32)
    set (cmakeFilesInstallDir "cmake")
    set (docInstallDir        "doc")
else ()
    set (cmakeFilesInstallDir "share/${CMAKE_PROJECT_NAME}/cmake")
    set (docInstallDir        "share/doc/${CMAKE_PROJECT_NAME}")
endif ()

# The export name to use for libraries.
set (exportTarget "${CMAKE_PROJECT_NAME}-targets")

# ==============================================================================
# Build settings
# ==============================================================================

# The settings in this section apply to the entire project.  Please don't change
# them without consulting the rest of the team, as it may break others' code.

# Compiler/language settings
if (CMAKE_COMPILER_IS_GNUCXX)
    # Use C++11/C++14 features (to the extent possible), treat all warnings
    # as errors.
    if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS "4.9.0")
        add_compile_options("-std=c++0x")
    else ()
        add_compile_options("-std=c++1y")
    endif ()
elseif (MSVC)
    # This disables some instances of compiler warning C4996, which are not even
    # disabled by compiler option /W0.
    add_compile_options("/wd4996")
    add_definitions("-D_SCL_SECURE_NO_WARNINGS")

    # Use a Boost-like library naming convention where toolchain version and
    # ABI is encoded in the name.
    if (MSVC_VERSION LESS 1900)
        math (EXPR vcVersion "(${MSVC_VERSION}-600)/10")
    else ()
        math (EXPR vcVersion "(${MSVC_VERSION}-500)/10")
    endif ()
    set (CMAKE_DEBUG_POSTFIX          "-vc${vcVersion}-gd")
    set (CMAKE_RELEASE_POSTFIX        "-vc${vcVersion}")
    set (CMAKE_RELWITHDEBINFO_POSTFIX "-vc${vcVersion}")
    set (CMAKE_MINSIZEREL_POSTFIX     "-vc${vcVersion}")
endif()

# Boost settings
# Note that find_package(Boost) has to be called for the individual
# subdirectories/targets.
set (Boost_USE_MULTITHREADED ON)
set (Boost_USE_STATIC_LIBS ON)
add_definitions("-DBOOST_SYSTEM_NO_DEPRECATED") # Disable some deprecated stuff.

# Workaround to enable compilation of Google Test 1.7.0 on VS2012
# https://stackoverflow.com/questions/12558327/google-test-in-visual-studio-2012
if (MSVC11)
    add_definitions("-D_VARIADIC_MAX=10")
endif()

# ==============================================================================
# Other initialisation
# ==============================================================================

# Read version numbers from config.h
file (READ "${publicHeaderDir}/coral/config.h" configDotH)
string (REGEX MATCH "#define +CORAL_VERSION_MAJOR +([0-9]+)" dummy "${configDotH}")
set (versionMajor "${CMAKE_MATCH_1}")
string (REGEX MATCH "#define +CORAL_VERSION_MINOR +([0-9]+)" dummy "${configDotH}")
set (versionMinor "${CMAKE_MATCH_1}")
string (REGEX MATCH "#define +CORAL_VERSION_PATCH +([0-9]+)" dummy "${configDotH}")
set (versionPatch "${CMAKE_MATCH_1}")

# Get rid of a CMake warning wrt. INTERFACE_LINK_LIBRARIES target property.
# Note: This has to be done globally due to some issues with policy scope and
# whatnot.  (Otherwise it would be defined in proto/CMakeLists.txt.)
cmake_policy (SET CMP0022 NEW)

if (CORAL_BUILD_TESTS)
    enable_testing ()
endif ()

# Allow local CMake modules (e.g. find scripts) under cmake/
list (APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

# ==============================================================================
# Dependencies
# ==============================================================================
if (CORAL_BUILD_TESTS)
    find_package (GTest REQUIRED)
    set (testBoostLibs chrono thread)
else ()
    set (testBoostLibs)
endif ()

find_package (Boost
    REQUIRED
    COMPONENTS filesystem program_options random ${testBoostLibs}
)
find_package (CPPZMQ REQUIRED)
find_package (FMILIB REQUIRED)
find_package (LIBZIP REQUIRED)
include("CompatFindProtobuf")

# ==============================================================================
# Subdirectories
# ==============================================================================
add_subdirectory ("proto")
add_subdirectory ("src")

# ==============================================================================
# API documentation generation
# ==============================================================================
find_package (Doxygen)
if (DOXYGEN_FOUND)
    set (documentedDirectories "\"${CMAKE_SOURCE_DIR}/include\"")
    if (CORAL_BUILD_PRIVATE_API_DOCS)
        set (documentedDirectories
            "${documentedDirectories} \"${CMAKE_SOURCE_DIR}/src/include\"")
    endif ()
    set (doxyfile "${CMAKE_BINARY_DIR}/Doxyfile")
    configure_file ("${CMAKE_SOURCE_DIR}/cmake/Doxyfile.in" ${doxyfile} @ONLY)
    add_custom_target ("doc" ALL ${DOXYGEN_EXECUTABLE} ${doxyfile}
                       WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
                       COMMENT "Generating API documentation with Doxygen" VERBATIM)
    install (DIRECTORY "${CMAKE_BINARY_DIR}/html/" DESTINATION "${docInstallDir}/api")
endif ()

# ==============================================================================
# Package-config setup
# ==============================================================================
include (CMakePackageConfigHelpers)

# Create and install package-config file
set (configFile "${CMAKE_BINARY_DIR}/${CMAKE_PROJECT_NAME}-config.cmake")
set (exportFileConfig "${cmakeFilesInstallDir}/${exportTarget}.cmake")
set (extraModuleDirConfig "${cmakeFilesInstallDir}")
set (includeDirsConfig "include")
configure_package_config_file (
    "${CMAKE_SOURCE_DIR}/cmake/project-config.cmake.in"
    ${configFile}
    INSTALL_DESTINATION ${cmakeFilesInstallDir}
    PATH_VARS exportFileConfig extraModuleDirConfig includeDirsConfig)
install (FILES ${configFile} DESTINATION ${cmakeFilesInstallDir})

# Create and install package-version file
set (versionFile "${CMAKE_BINARY_DIR}/${CMAKE_PROJECT_NAME}-config-version.cmake")
write_basic_package_version_file (
    ${versionFile}
    VERSION "${versionMajor}.${versionMinor}.${versionPatch}"
    COMPATIBILITY "SameMajorVersion")
install (FILES ${versionFile} DESTINATION ${cmakeFilesInstallDir})

# ==============================================================================
# Installation of remaining files
# ==============================================================================

# Public headers
install (DIRECTORY "${publicHeaderDir}/" DESTINATION ${headerInstallDir})

# Exports files
install (EXPORT ${exportTarget} DESTINATION ${cmakeFilesInstallDir})

# Other files
set (readmeFile "${CMAKE_SOURCE_DIR}/README.md")
install (FILES ${readmeFile} DESTINATION ${docInstallDir})
set (licenceFile "${CMAKE_SOURCE_DIR}/LICENCE.txt")
install (FILES ${licenceFile} DESTINATION ${docInstallDir})
install (DIRECTORY "${CMAKE_SOURCE_DIR}/manuals/" DESTINATION ${docInstallDir})

install (FILES
        "cmake/FindBoostTarget.cmake"
        "cmake/FindFMILIB.cmake"
        "cmake/FindLIBZIP.cmake"
        "cmake/FindProtobufTarget.cmake"
        "cmake/FindZMQ.cmake"
    DESTINATION
        "${cmakeFilesInstallDir}"
)

# Build information
include ("cmake/build-info-file.cmake")

# ==============================================================================
# Packaging
# ==============================================================================
set (CPACK_PACKAGE_CONTACT              "Lars T. Kyllingstad <lars.kyllingstad@sintef.no>")
set (CPACK_PACKAGE_DESCRIPTION_SUMMARY  "${projectSummary}")
set (CPACK_PACKAGE_DESCRIPTION_FILE     "${readmeFile}")
set (CPACK_PACKAGE_VENDOR               "SINTEF Fisheries and Aquaculture")
set (CPACK_PACKAGE_VERSION_MAJOR        ${versionMajor})
set (CPACK_PACKAGE_VERSION_MINOR        ${versionMinor})
set (CPACK_PACKAGE_VERSION_PATCH        ${versionPatch})
set (CPACK_RESOURCE_FILE_README         "${readmeFile}")
set (CPACK_RESOURCE_FILE_LICENSE        "${licenceFile}")
include (CPack)
