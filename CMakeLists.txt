cmake_minimum_required (VERSION 2.8.11)

# CMake guidelines/tips for this project:
#
# - The "Common variables" section (below) contains variables which are useful
#   not only in here in the top-level file, but in subdirectories as well.
#
# - In install() commands, use the directories specified in the "Common
#   variables" section, and avoid using absolute directories.  This makes for a
#   consistent directory structure and allows for easy packaging with CPack.
#
project ("dsb")
set (projectSummary "Distributed Simulation Bus")
set (versionMajor "0")
set (versionMinor "1")
set (versionPatch "0")

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
    set (readmeInstallDir     "doc")
else ()
    set (cmakeFilesInstallDir "share/${CMAKE_PROJECT_NAME}/cmake")
    set (docInstallDir        "share/doc/${CMAKE_PROJECT_NAME}")
    set (readmeInstallDir     "share/${CMAKE_PROJECT_NAME}")
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
    # Use C++11 features, treat all warnings as errors.
    set(CMAKE_CXX_FLAGS
        "${CMAKE_CXX_FLAGS} -std=c++11 -Wall -Werror -Wno-unused-function")
elseif (MSVC)
    # Compile with warning level 3 and treat all warnings as errors,
    # but disable the following warnings:
    #     C4996: Function call with parameters that may be unsafe [...] use
    #            Visual C++ 'Checked Iterators'
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W3 /WX /wd4996")
endif()
set (CMAKE_DEBUG_POSTFIX "_d")

# Boost settings
# Note that find_package(Boost) has to be called for the individual
# subdirectories/targets.
set (Boost_USE_MULTITHREADED ON)
set (Boost_USE_STATIC_LIBS ON)
add_definitions("-DBOOST_ALL_NO_LIB") # Disables Boost's auto-linking feature.
add_definitions("-DBOOST_SYSTEM_NO_DEPRECATED") # Disable some deprecated stuff.

# ==============================================================================
# Other initialisation
# ==============================================================================

# Enable testing and load Google Test
enable_testing ()
set (gtest_force_shared_crt ON CACHE INTERNAL "Google Test setting override")
add_subdirectory ("external/gtest-1.7.0")

# Allow local CMake modules (e.g. find scripts) under cmake/
list (APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

# ==============================================================================
# Subdirectories
# ==============================================================================
add_subdirectory ("src/dsb")
add_subdirectory ("src/dsbexec")

# ==============================================================================
# API documentation generation
# ==============================================================================
find_package (Doxygen)
if (DOXYGEN_FOUND)
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
set (includeDirsConfig "include")
configure_package_config_file (
    "${CMAKE_SOURCE_DIR}/cmake/project-config.cmake.in"
    ${configFile}
    INSTALL_DESTINATION ${cmakeFilesInstallDir}
    PATH_VARS exportFileConfig includeDirsConfig)
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
set (readmeFile "${CMAKE_SOURCE_DIR}/README.txt")
install (FILES ${readmeFile} DESTINATION ${readmeInstallDir})
set (licenceFile "${CMAKE_SOURCE_DIR}/LICENCE.txt")
install (FILES ${licenceFile} DESTINATION ${readmeInstallDir})

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
