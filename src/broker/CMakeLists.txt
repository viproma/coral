find_package (Boost REQUIRED COMPONENTS "filesystem" "thread" "chrono" "system") 
find_package (ZMQ REQUIRED)

# Main executable target
set (_target "broker")
add_executable (${_target} "main.cpp")
target_include_directories (${_target}
    PRIVATE ${publicHeaderDir}
            ${privateHeaderDir}
            ${CPPZMQ_INCLUDE_DIRS}
            ${Boost_INCLUDE_DIRS})
target_link_libraries (${_target} "dsb" ${ZMQ_LIBRARIES} ${Boost_LIBRARIES})
