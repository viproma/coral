find_package (Boost REQUIRED COMPONENTS thread chrono filesystem system)
find_package (ZMQ REQUIRED)
find_package (FMILIBCPP REQUIRED)

set (_target "slave_provider")
add_executable (${_target} "main.cpp")
target_include_directories (${_target}
    PRIVATE ${publicHeaderDir}
            ${privateHeaderDir}
            ${CPPZMQ_INCLUDE_DIRS}
            ${Boost_INCLUDE_DIRS})
target_link_libraries (${_target}
    "dsb" "dsbproto"
    ${FMILIBCPP_LIBRARIES}
    ${FMILIB_LIBRARIES}
    ${ZMQ_LIBRARIES}
    ${Boost_LIBRARIES})
