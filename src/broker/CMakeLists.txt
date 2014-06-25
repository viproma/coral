find_package (Boost REQUIRED COMPONENTS "thread" "chrono" "system") 
find_package (ZMQ REQUIRED)

set (_headers "proxy.hpp")
set (_sources "proxy.cpp")
set (_testSources "proxy_test.cpp")

# Main executable target
set (_target "broker")
add_executable (${_target} "main.cpp" ${_headers} ${_sources})
target_include_directories (${_target}
    PRIVATE ${publicHeaderDir}
            ${privateHeaderDir}
            ${CPPZMQ_INCLUDE_DIRS}
            ${Boost_INCLUDE_DIRS})
target_link_libraries (${_target} ${ZMQ_LIBRARIES} ${Boost_LIBRARIES})

# Test target
set (_testTarget "broker_test")
add_executable (${_testTarget} ${_headers} ${_sources} ${_testSources})
# Use the same libraries and include dirs as main target.  We should in
# principle be able to use generator expressions for target_link_libraries()
# too, but run into trouble when the property itself contains generator
# expressions.
get_target_property (_libraries ${_target} LINK_LIBRARIES)
target_link_libraries (${_testTarget} ${_libraries} "gtest_main")
target_include_directories (${_testTarget}
    PRIVATE "$<TARGET_PROPERTY:${_target},INCLUDE_DIRECTORIES>"
            "$<TARGET_PROPERTY:gtest_main,INCLUDE_DIRECTORIES>"
)
add_test (NAME ${_testTarget} COMMAND ${_testTarget})
