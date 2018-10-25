# Installs a target's prerequisites (i.e., the shared libraries it
# depends on).
function(install_prerequisites target)
    add_custom_command(
        TARGET "${target}"
        POST_BUILD
        COMMAND
            "${CMAKE_COMMAND}"
            "-E"
            "make_directory"
            "${CMAKE_BINARY_DIR}/prerequisites/${target}/$<CONFIG>"
        COMMAND
            "${CMAKE_COMMAND}"
            "-DEXECUTABLE=$<TARGET_FILE:${target}>"
            "-DTARGET_DIR=${CMAKE_BINARY_DIR}/prerequisites/${target}/$<CONFIG>"
            "-DSEARCH_DIRS=${CMAKE_PREFIX_PATH}"
            "-P" "${CMAKE_SOURCE_DIR}/cmake/CopyPrerequisites.cmake"
        VERBATIM)
    install(
        DIRECTORY "${CMAKE_BINARY_DIR}/prerequisites/${target}/$<CONFIG>/"
        DESTINATION "lib"
        USE_SOURCE_PERMISSIONS
        FILES_MATCHING PATTERN "*.so*")
    install(
        DIRECTORY "${CMAKE_BINARY_DIR}/prerequisites/${target}/$<CONFIG>/"
        DESTINATION "bin"
        USE_SOURCE_PERMISSIONS
        FILES_MATCHING PATTERN "*.dll")
endfunction(install_prerequisites)
