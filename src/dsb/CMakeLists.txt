set (_target "dsb")

set (_publicHeaders
    "dsb/config.h"
    "dsb/domain/controller.hpp"
    "dsb/domain/locator.hpp"
    "dsb/domain/slave_provider.hpp"
    "dsb/execution/controller.hpp"
    "dsb/execution/locator.hpp"
    "dsb/execution/slave.hpp"
    "dsb/fmi.hpp"
    "dsb/model.hpp"
    "dsb/sequence.hpp"
)
set (_privateHeaders
    "dsb/bus/domain_data.hpp"
    "dsb/bus/execution_agent.hpp"
    "dsb/bus/execution_state.hpp"
    "dsb/bus/slave_agent.hpp"
    "dsb/bus/slave_tracker.hpp"
    "dsb/comm.hpp"
    "dsb/compat_helpers.hpp"
    "dsb/error.hpp"
    "dsb/inproc_rpc.hpp"
    "dsb/protobuf.hpp"
    "dsb/protocol/domain.hpp"
    "dsb/protocol/execution.hpp"
    "dsb/proxy.hpp"
    "dsb/util.hpp"
)
set (_internalHeaders
    "dsb/fmi/glue.hpp"
    "dsb/fmi/slave.hpp"
    "dsb/fmi/slave_type.hpp"
    "dsb/protocol/glue.hpp"
)
set (_sources
    "dsb_fmi_glue.cpp"
    "dsb_fmi_slave.cpp"
    "dsb_fmi_slave_type.cpp"
    "dsb_protocol_glue.cpp"

    "dsb_bus_domain_data.cpp"
    "dsb_bus_execution_agent.cpp"
    "dsb_bus_execution_state.cpp"
    "dsb_bus_slave_agent.cpp"
    "dsb_bus_slave_tracker.cpp"
    "dsb_comm.cpp"
    "dsb_inproc_rpc.cpp"
    "dsb_protobuf.cpp"
    "dsb_protocol_domain.cpp"
    "dsb_protocol_execution.cpp"
    "dsb_proxy.cpp"
    "dsb_util.cpp"

    "dsb_domain_controller.cpp"
    "dsb_domain_locator.cpp"
    "dsb_domain_slave_provider.cpp"
    "dsb_execution_controller.cpp"
    "dsb_execution_locator.cpp"
    "dsb_execution_slave.cpp"
    "dsb_fmi.cpp"
    "dsb_model.cpp"
)
set (_testSources
    "dsb_comm_test.cpp"
    "dsb_compat_helpers_test.cpp"
    "dsb_protobuf_test.cpp"
    "dsb_protocol_domain_test.cpp"
    "dsb_protocol_execution_test.cpp"
    "dsb_domain_controller_test.cpp"
    "dsb_execution_controller_test.cpp"
    "dsb_inproc_rpc_test.cpp"
    "dsb_proxy_test.cpp"
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
include_directories (${publicHeaderDir} ${privateHeaderDir})

# Many files have the same (base)name, so we group them based on their
# "package name", i.e. the name of their subdirectory under "dsb/".
macro (CreateHeaderGroups _superGroup)
    foreach (_hdr ${ARGN})
        unset (_pkg)
        string (REGEX REPLACE ".*dsb/([a-zA-Z_]+)/[a-zA-Z_]+.hpp" "\\1" _pkg "${_hdr}")
        if (_pkg STREQUAL _hdr)
            source_group ("${_superGroup}" FILES "${_hdr}")
        else ()
            source_group ("${_superGroup}\\${_pkg}" FILES "${_hdr}")
        endif ()
    endforeach()
endmacro(CreateHeaderGroups)
CreateHeaderGroups("Public Header Files" ${_publicHeadersFull})
CreateHeaderGroups("Private Header Files" ${_privateHeadersFull})
CreateHeaderGroups("Internal Header Files" ${_internalHeaders})

# Dependencies
find_package (Boost REQUIRED COMPONENTS filesystem thread system chrono)
find_package (ZMQ REQUIRED)
find_package (FMILIBCPP REQUIRED)

# Main library target
add_library (${_target} STATIC
    ${_publicHeadersFull}
    ${_privateHeadersFull}
    ${_internalHeaders}
    ${_sources})
target_include_directories(${_target}
    PRIVATE "$<TARGET_PROPERTY:dsbproto,INTERFACE_INCLUDE_DIRECTORIES>"
            ${FMILIBCPP_INCLUDE_DIRS}
            ${FMILIB_INCLUDE_DIRS}
            ${ZMQ_INCLUDE_DIRS}
            ${CPPZMQ_INCLUDE_DIRS}
            ${Boost_INCLUDE_DIRS}
)
install (TARGETS ${_target} EXPORT ${exportTarget} ${targetInstallDestinations})

# Test target
set (_testTarget "${_target}_test")
add_executable (${_testTarget} ${_testSources})
target_link_libraries (${_testTarget}
    ${_target}
    "gtest_main"
    "dsbproto"
    ${FMILIBCPP_LIBRARIES}
    ${FMILIB_LIBRARIES}
    ${ZMQ_LIBRARIES}
    ${Boost_LIBRARIES}
)
target_include_directories(${_testTarget}
    PRIVATE "$<TARGET_PROPERTY:gtest_main,INCLUDE_DIRECTORIES>"
            ${FMILIBCPP_INCLUDE_DIRS}
            ${FMILIB_INCLUDE_DIRS}
            ${ZMQ_INCLUDE_DIRS}
            ${CPPZMQ_INCLUDE_DIRS}
            ${Boost_INCLUDE_DIRS}
)
add_test (NAME ${_testTarget} COMMAND ${_testTarget})
