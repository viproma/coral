find_package (Boost REQUIRED COMPONENTS "thread" "chrono" "system") 
find_package (ZMQ REQUIRED)

set (_headers "p2p_proxy.hpp")
set (_sources "p2p_proxy.cpp")
set (_testSources "p2p_proxy_test.cpp")

# Main executable target
set (_target "domain_broker")
add_executable (${_target} "main.cpp" ${_headers} ${_sources})
target_include_directories (${_target}
    PRIVATE ${publicHeaderDir}
            ${privateHeaderDir}
            ${CPPZMQ_INCLUDE_DIRS}
            ${Boost_INCLUDE_DIRS})
target_link_libraries (${_target} "dsb" ${ZMQ_LIBRARIES} ${Boost_LIBRARIES})

# Test target
set (_testTarget "${_target}_test")
add_executable (${_testTarget} ${_headers} ${_sources} ${_testSources})
target_link_libraries (${_testTarget}
    "dsb"
    "gtest_main"
    ${ZMQ_LIBRARIES}
    ${Boost_LIBRARIES}
)
target_include_directories(${_testTarget}
    PRIVATE "$<TARGET_PROPERTY:gtest_main,INCLUDE_DIRECTORIES>"
            ${publicHeaderDir}
            ${privateHeaderDir}
            ${CPPZMQ_INCLUDE_DIRS}
            ${Boost_INCLUDE_DIRS}
)
add_test (NAME ${_testTarget} COMMAND ${_testTarget})
