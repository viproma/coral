find_package (Boost REQUIRED COMPONENTS thread chrono filesystem system)
find_package (ZMQ REQUIRED)
find_package (FMILIB REQUIRED)

set (_target "slave")
add_executable (${_target} "main.cpp")
target_include_directories (${_target}
    PRIVATE ${publicHeaderDir}
            ${privateHeaderDir}
            ${CPPZMQ_INCLUDE_DIRS}
            ${Boost_INCLUDE_DIRS})
target_link_libraries (${_target}
    "dsb" "dsbproto"
    ${FMILIB_LIBRARIES}
    ${ZMQ_LIBRARIES}
    ${Boost_LIBRARIES})
