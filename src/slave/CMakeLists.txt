find_package (ZMQ REQUIRED)

set (_target "slave")
add_executable (${_target} "main.cpp")
target_include_directories (${_target} PRIVATE ${privateHeaderDir} ${CPPZMQ_INCLUDE_DIRS})
target_link_libraries (${_target} "dsb" "dsbproto" ${ZMQ_LIBRARIES})
