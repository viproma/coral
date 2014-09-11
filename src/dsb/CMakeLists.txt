set (_target "dsb")

set (_publicHeaders
    "dsb/config.h"
    "dsb/execution.hpp"
    "dsb/sequence.hpp"
)
set (_privateHeaders
    "dsb/bus/execution_agent.hpp"
    "dsb/bus/execution_state.hpp"
    "dsb/bus/slave_agent.hpp"
    "dsb/bus/slave_tracker.hpp"
    "dsb/comm.hpp"
    "dsb/compat_helpers.hpp"
    "dsb/control.hpp"
    "dsb/error.hpp"
    "dsb/protobuf.hpp"
    "dsb/util.hpp"
)
set (_internalHeaders ) # Only visible inside the "dsb" module.
set (_sources
    "dsb_bus_execution_agent.cpp"
    "dsb_bus_execution_state.cpp"
    "dsb_bus_slave_agent.cpp"
    "dsb_bus_slave_tracker.cpp"
    "dsb_comm.cpp"
    "dsb_control.cpp"
    "dsb_protobuf.cpp"
    "dsb_util.cpp"

    "dsb_execution.cpp"
)
set (_testSources
    "dsb_comm_test.cpp"
    "dsb_compat_helpers_test.cpp"
    "dsb_protobuf_test.cpp"
    "dsb_control_test.cpp"
    "dsb_sequence_test.cpp"
    "dsb_util_test.cpp"
)

# Add full path to non-internal headers
set (_publicHeadersFull)
foreach (h ${_publicHeaders})
    list (APPEND _publicHeadersFull "${publicHeaderDir}/${h}")
endforeach ()
set (_privateHeadersFull)
foreach (h ${_privateHeaders})
    list (APPEND _privateHeadersFull "${privateHeaderDir}/${h}")
endforeach ()

source_group ("Public Header Files" FILES ${_publicHeadersFull})
source_group ("Private Header Files" FILES ${_privateHeadersFull})
source_group ("Internal Header Files" FILES ${_internalHeaders})
include_directories (${publicHeaderDir} ${privateHeaderDir})

# Dependencies
find_package (Boost REQUIRED)
find_package (ZMQ REQUIRED)

# Main library target
add_library (${_target} STATIC
    ${_publicHeadersFull}
    ${_privateHeadersFull}
    ${_internalHeaders}
    ${_sources})
target_include_directories(${_target}
    PRIVATE "$<TARGET_PROPERTY:dsbproto,INTERFACE_INCLUDE_DIRECTORIES>"
            ${ZMQ_INCLUDE_DIRS}
            ${CPPZMQ_INCLUDE_DIRS}
            ${Boost_INCLUDE_DIRS}
)
install (TARGETS ${_target} EXPORT ${exportTarget} ${targetInstallDestinations})

# Test target
set (_testTarget "${_target}_test")
add_executable (${_testTarget} ${_testSources})
target_link_libraries (${_testTarget} ${_target} "gtest_main" "dsbproto" ${ZMQ_LIBRARIES})
target_include_directories(${_testTarget}
    PRIVATE "$<TARGET_PROPERTY:gtest_main,INCLUDE_DIRECTORIES>"
            ${ZMQ_INCLUDE_DIRS}
            ${CPPZMQ_INCLUDE_DIRS}
)
add_test (NAME ${_testTarget} COMMAND ${_testTarget})
