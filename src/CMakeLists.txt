# The directory in which private headers are located.
# These are the headers that should be available to all submodules in this
# project, but which should not be installed as part of the public API.
set (privateHeaderDir "${CMAKE_CURRENT_SOURCE_DIR}/include")

add_subdirectory ("broker")
add_subdirectory ("dsb")
add_subdirectory ("dsbexec")
add_subdirectory ("slave")
add_subdirectory ("slave_provider")
