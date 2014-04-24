#set (_target "broker")

set (_publicHeaders )
set (_internalHeaders )
set (_sources )
set (_testSources )

# Public API headers are located elsewhere
include_directories (${publicHeaderDir})

set (_publicHeadersFull)
foreach (_hdr ${_publicHeaders})
    list (APPEND _publicHeadersFull "${publicHeaderDir}/${_hdr}")
endforeach ()
source_group ("Public Header Files" FILES ${_publicHeadersFull})

# Dependencies
find_package (Boost REQUIRED COMPONENTS "thread" "system")
set (ZMQ_LIBRARIES "zmq")

# Main library target
foreach (_target "pubsub" "control")
    set (_sources "${_target}.cpp")
    add_executable (${_target}
        ${_publicHeadersFull}
        ${_internalHeaders}
        ${_sources})
    target_include_directories (${_target}
        PRIVATE ${Boost_INCLUDE_DIRS} ${CPPZMQ_INCLUDE_DIRS})
    target_link_libraries (${_target} ${Boost_LIBRARIES} ${ZMQ_LIBRARIES})
endforeach ()
#install (TARGETS ${_target} ${targetInstallDestinations})

# Test target
#set (_testTarget "${_target}_test")
#add_executable (${_testTarget} ${_testSources})
#target_link_libraries (${_testTarget} ${_target} "gtest_main")
#target_include_directories(
#    ${_testTarget}
#    PRIVATE "$<TARGET_PROPERTY:gtest_main,INCLUDE_DIRECTORIES>")
#add_test (NAME ${_testTarget} COMMAND ${_testTarget})
