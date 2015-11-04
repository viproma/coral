set (_target "dsb")

set (_publicHeaders
    "dsb/config.h"
    "dsb/domain/controller.hpp"
    "dsb/domain/slave_provider.hpp"
    "dsb/execution/controller.hpp"
    "dsb/execution/slave.hpp"
    "dsb/fmi.hpp"
    "dsb/model.hpp"
    "dsb/net.hpp"
)
set (_privateHeaders
    "dsb/bus/domain_data.hpp"
    "dsb/bus/execution_manager.hpp"
    "dsb/bus/execution_manager_private.hpp"
    "dsb/bus/execution_state.hpp"
    "dsb/bus/slave_agent.hpp"
    "dsb/bus/slave_controller.hpp"
    "dsb/bus/slave_control_messenger.hpp"
    "dsb/bus/slave_control_messenger_v0.hpp"
    "dsb/bus/slave_setup.hpp"
    "dsb/comm/messaging.hpp"
    "dsb/comm/p2p.hpp"
    "dsb/comm/proxy.hpp"
    "dsb/comm/reactor.hpp"
    "dsb/comm/util.hpp"
    "dsb/comm.hpp"
    "dsb/error.hpp"
    "dsb/fmi/glue.hpp"
    "dsb/fmi/slave.hpp"
    "dsb/fmi/slave_type.hpp"
    "dsb/fmilib/fmu.hpp"
    "dsb/fmilib/fmu1.hpp"
    "dsb/fmilib/importcontext.hpp"
    "dsb/fmilib/logger.hpp"
    "dsb/fmilib/streamlogger.hpp"
    "dsb/inproc_rpc.hpp"
    "dsb/protobuf.hpp"
    "dsb/protocol/domain.hpp"
    "dsb/protocol/execution.hpp"
    "dsb/protocol/glue.hpp"
    "dsb/util.hpp"
)
set (_sources
    "dsb_fmi_glue.cpp"
    "dsb_fmi_slave.cpp"
    "dsb_fmi_slave_type.cpp"
    "dsb_protocol_glue.cpp"

    "dsb_bus_domain_data.cpp"
    "dsb_bus_execution_manager.cpp"
    "dsb_bus_execution_manager_private.cpp"
    "dsb_bus_execution_state.cpp"
    "dsb_bus_slave_agent.cpp"
    "dsb_bus_slave_controller.cpp"
    "dsb_bus_slave_control_messenger.cpp"
    "dsb_bus_slave_control_messenger_v0.cpp"
    "dsb_bus_slave_setup.cpp"
    "dsb_comm_messaging.cpp"
    "dsb_comm_p2p.cpp"
    "dsb_comm_proxy.cpp"
    "dsb_comm_reactor.cpp"
    "dsb_comm_util.cpp"
    "dsb_error.cpp"
    "dsb_fmilib_fmu.cpp"
    "dsb_fmilib_fmu1.cpp"
    "dsb_fmilib_importcontext.cpp"
    "dsb_fmilib_streamlogger.cpp"
    "dsb_inproc_rpc.cpp"
    "dsb_protobuf.cpp"
    "dsb_protocol_domain.cpp"
    "dsb_protocol_execution.cpp"
    "dsb_util.cpp"

    "dsb_domain_controller.cpp"
    "dsb_domain_slave_provider.cpp"
    "dsb_execution_controller.cpp"
    "dsb_execution_slave.cpp"
    "dsb_fmi.cpp"
    "dsb_model.cpp"
    "dsb_net.cpp"
)
set (_testSources
    "dsb_comm_messaging_test.cpp"
    "dsb_comm_p2p_test.cpp"
    "dsb_comm_proxy_test.cpp"
    "dsb_comm_reactor_test.cpp"
    "dsb_comm_util_test.cpp"
    "dsb_error_test.cpp"
    "dsb_fmilib_fmu1_test.cpp"
    "dsb_fmilib_importcontext_test.cpp"
    "dsb_fmilib_streamlogger_test.cpp"
    "dsb_protobuf_test.cpp"
    "dsb_protocol_domain_test.cpp"
    "dsb_protocol_execution_test.cpp"
    "dsb_domain_controller_test.cpp"
    "dsb_execution_controller_test.cpp"
    "dsb_inproc_rpc_test.cpp"
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
        string (REGEX REPLACE ".*dsb/([a-zA-Z0-9_]+)/[a-zA-Z0-9_]+.hpp" "\\1" _pkg "${_hdr}")
        if (_pkg STREQUAL _hdr)
            source_group ("${_superGroup}" FILES "${_hdr}")
        else ()
            source_group ("${_superGroup}\\${_pkg}" FILES "${_hdr}")
        endif ()
    endforeach()
endmacro(CreateHeaderGroups)
CreateHeaderGroups("Public Header Files" ${_publicHeadersFull})
CreateHeaderGroups("Private Header Files" ${_privateHeadersFull})

# Dependencies
find_package (Boost REQUIRED COMPONENTS filesystem thread system chrono)
find_package (CPPZMQ REQUIRED)
find_package (FMILIB REQUIRED)

# Main library target
add_library (${_target} STATIC
    ${_publicHeadersFull}
    ${_privateHeadersFull}
    ${_sources})
target_link_libraries (${_target}
    PRIVATE "dsbproto"
            ${FMILIB_LIBRARIES}
            ${CPPZMQ_LIBRARIES}
)
# FindBoost uses old CMake conventions
target_link_libraries (${_target} LINK_PRIVATE ${Boost_LIBRARIES})
target_include_directories(${_target} PRIVATE ${Boost_INCLUDE_DIRS})

install (TARGETS ${_target} EXPORT ${exportTarget} ${targetInstallDestinations})

# Test target
set (_testTarget "${_target}_test")
add_executable (${_testTarget} ${_testSources})
target_link_libraries (${_testTarget}
    PRIVATE ${_target}
            "gtest_main"
            "dsbproto"
            ${FMILIB_LIBRARIES}
            ${CPPZMQ_LIBRARIES}
)
# FindBoost uses "old style" library references
target_link_libraries (${_target} LINK_PRIVATE ${Boost_LIBRARIES})
target_include_directories(${_testTarget}
    PRIVATE "$<TARGET_PROPERTY:gtest_main,INCLUDE_DIRECTORIES>"
            ${Boost_INCLUDE_DIRS}
)
target_compile_definitions(${_testTarget} PRIVATE
    "DSB_TEST_FMU_DIRECTORY=${CMAKE_SOURCE_DIR}/external/fmus"
)
add_test (NAME ${_testTarget} COMMAND ${_testTarget})
