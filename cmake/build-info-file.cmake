# This file generates a custom target which creates a text file that
# contains information about the current build.  The following info
# is included:
#
#   - Hash and timestamp of the last Git commit
#   - The build type
set (_buildInfoFile "${CMAKE_BINARY_DIR}/build-info.txt")

find_package(Git)
if(Git_FOUND OR GIT_FOUND)
    set(_gitCommand "${GIT_EXECUTABLE}" "log" "-n1" "--pretty=format:Git commit: %H %ci%n")
else()
    set(_gitCommand "${CMAKE_COMMAND}" "-E" "echo" "Git commit: (undetermined)")
endif()

add_custom_command(
    OUTPUT "${_buildInfoFile}"
    COMMAND ${_gitCommand} ">" "${_buildInfoFile}"
    COMMAND "${CMAKE_COMMAND}" "-E" "echo" "Build type: $<CONFIG>${CMAKE_BUILD_TYPE}" ">>" "${_buildInfoFile}"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    COMMENT "Generating build information file"
    VERBATIM
)
add_custom_target("build-info" ALL DEPENDS "${_buildInfoFile}")
install(FILES "${_buildInfoFile}" DESTINATION "${readmeInstallDir}")
