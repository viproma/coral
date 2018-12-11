# Copies an executable file's prerequisites to some directory
#
# Usage:
#    cmake
#       -DEXECUTABLE="path/to/executable"
#       -DTARGET_DIR="path/to/target/dir"
#       -DSEARCH_DIRS="list/of;dirs/to/search"
#       -P CopyPrerequisites.cmake

# Ensure we use an absolute path to the executable.
if(NOT EXISTS "${EXECUTABLE}")
    message(FATAL_ERROR "EXECUTABLE does not exist (value: ${EXECUTABLE})")
elseif(IS_ABSOLUTE "${EXECUTABLE}")
    set(absExecutable "${EXECUTABLE}")
else()
    set(absExecutable "${CMAKE_CURRENT_BINARY_DIR}/${EXECUTABLE}")
endif()

if (NOT IS_DIRECTORY "${TARGET_DIR}")
    message(FATAL_ERROR "TARGET_DIR is not a directory (value: ${TARGET_DIR})")
endif()

set(searchDirs)
foreach(d IN LISTS SEARCH_DIRS)
    list(APPEND searchDirs "${d}" "${d}/lib" "${d}/bin")
endforeach()

include(GetPrerequisites)
get_prerequisites("${absExecutable}" prerequisites 1 1 "" "${searchDirs}")
foreach(p IN LISTS prerequisites)
    gp_resolve_item("${absExecutable}" "${p}" "" "${searchDirs}" fullPath)
    message(STATUS "Copying '${fullPath}' to '${TARGET_DIR}'")
    file(COPY "${fullPath}" DESTINATION "${TARGET_DIR}" USE_SOURCE_PERMISSIONS)
endforeach()
