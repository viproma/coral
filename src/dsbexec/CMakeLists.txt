set (_target "dsbexec")
include_directories (${publicHeaderDir})
add_executable (${_target} "main.cpp")
target_link_libraries (${_target} "dsb")
