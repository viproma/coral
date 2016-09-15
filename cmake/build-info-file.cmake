# This file generates a custom target which creates a text file that
# contains information about the current build.  The following info
# is included:
#
#   - Hash and timestamp of the last Git commit
#   - The build type

set (_buildInfoFile "${CMAKE_BINARY_DIR}/build-info.txt")
add_custom_command(
    OUTPUT "${_buildInfoFile}"
    COMMAND "git" "log" "-n1" "--pretty=format:Git commit: %H %ci%n" ">" "${_buildInfoFile}"
    COMMAND "${CMAKE_COMMAND}" "-E" "echo" "Build type: $<CONFIG>" ">>" "${_buildInfoFile}"
    COMMENT "Generating build information file"
    VERBATIM
)
add_custom_target("build-info" ALL DEPENDS "${_buildInfoFile}")
install(FILES "${_buildInfoFile}" DESTINATION "${readmeInstallDir}")
