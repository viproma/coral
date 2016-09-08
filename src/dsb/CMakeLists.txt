set (_target "dsb")

set (_publicHeaders
    "dsb/config.h"
    "dsb/domain/controller.hpp"
    "dsb/domain/slave_provider.hpp"
    "dsb/execution/controller.hpp"
    "dsb/execution/logging_slave.hpp"
    "dsb/execution/slave.hpp"
    "dsb/execution/variable_io.hpp"
    "dsb/fmi/fmu.hpp"
    "dsb/fmi/fmu1.hpp"
    "dsb/fmi/importer.hpp"
    "dsb/log.hpp"
    "dsb/model.hpp"
    "dsb/net.hpp"
)
set (_privateHeaders
    "dsb/async.hpp"
    "dsb/bus/execution_manager.hpp"
    "dsb/bus/execution_manager_private.hpp"
    "dsb/bus/execution_state.hpp"
    "dsb/bus/slave_agent.hpp"
    "dsb/bus/slave_controller.hpp"
    "dsb/bus/slave_control_messenger.hpp"
    "dsb/bus/slave_control_messenger_v0.hpp"
    "dsb/bus/slave_provider_comm.hpp"
    "dsb/bus/slave_setup.hpp"
    "dsb/net/ip.hpp"
    "dsb/net/reactor.hpp"
    "dsb/net/reqrep.hpp"
    "dsb/net/service.hpp"
    "dsb/net/udp.hpp"
    "dsb/net/zmqx.hpp"
    "dsb/error.hpp"
    "dsb/fmi/glue.hpp"
    "dsb/protobuf.hpp"
    "dsb/protocol/domain.hpp"
    "dsb/protocol/exe_data.hpp"
    "dsb/protocol/execution.hpp"
    "dsb/protocol/glue.hpp"
    "dsb/util.hpp"
    "dsb/util/console.hpp"
    "dsb/util/zip.hpp"
)
set (_sources
    "fmi_glue.cpp"
    "protocol_glue.cpp"

    "async.cpp"
    "bus_execution_manager.cpp"
    "bus_execution_manager_private.cpp"
    "bus_execution_state.cpp"
    "bus_slave_agent.cpp"
    "bus_slave_controller.cpp"
    "bus_slave_control_messenger.cpp"
    "bus_slave_control_messenger_v0.cpp"
    "bus_slave_provider_comm.cpp"
    "bus_slave_setup.cpp"
    "error.cpp"
    "net_ip.cpp"
    "net_reactor.cpp"
    "net_reqrep.cpp"
    "net_service.cpp"
    "net_udp.cpp"
    "net_zmqx_messaging.cpp"
    "net_zmqx_sockets.cpp"
    "net_zmqx_util.cpp"
    "protobuf.cpp"
    "protocol_domain.cpp"
    "protocol_exe_data.cpp"
    "protocol_execution.cpp"
    "util.cpp"
    "util_console.cpp"
    "util_zip.cpp"

    "domain_controller.cpp"
    "domain_slave_provider.cpp"
    "execution_controller.cpp"
    "execution_logging_slave.cpp"
    "execution_slave.cpp"
    "execution_variable_io.cpp"
    "fmi_fmu1.cpp"
    "fmi_importer.cpp"
    "log.cpp"
    "model.cpp"
    "net.cpp"
)
set (_testSources
    "domain_controller_test.cpp"
    "execution_controller_test.cpp"
    "execution_variable_io_test.cpp"

    "async_test.cpp"
    "error_test.cpp"
    "fmi_fmu1_test.cpp"
    "net_test.cpp"
    "net_reactor_test.cpp"
    "net_reqrep_test.cpp"
    "net_service_test.cpp"
    "net_zmqx_messaging_test.cpp"
    "net_zmqx_sockets_test.cpp"
    "net_zmqx_util_test.cpp"
    "protobuf_test.cpp"
    "protocol_domain_test.cpp"
    "protocol_exe_data_test.cpp"
    "protocol_execution_test.cpp"
    "util_test.cpp"
    "util_console_test.cpp"
    "util_zip_test.cpp"
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
find_package (BoostTarget REQUIRED
    COMPONENTS program_options filesystem thread system chrono random)
find_package (CPPZMQ REQUIRED)
find_package (ProtobufTarget REQUIRED)
find_package (FMILIB REQUIRED)
find_package (LIBZIP REQUIRED)

# Main library target
add_library (${_target} STATIC
    ${_publicHeadersFull}
    ${_privateHeadersFull}
    ${_sources}
    $<TARGET_OBJECTS:dsbproto>)
target_include_directories(${_target}
    PRIVATE $<TARGET_PROPERTY:dsbproto,INTERFACE_INCLUDE_DIRECTORIES>)
target_link_libraries (${_target}
    PRIVATE $<BUILD_INTERFACE:cppzmq>
            $<INSTALL_INTERFACE:zmq>
    PUBLIC  ${FMILIB_LIBRARIES}
            "libzip::libzip"
            "protobuf"
            "boost"
)
if (WIN32)
    target_link_libraries (${_target} PUBLIC "ws2_32" "iphlpapi")
endif()
if (UNIX)
    target_compile_options (${_target} PRIVATE "-fPIC")
endif()

install (TARGETS ${_target} EXPORT ${exportTarget} ${targetInstallDestinations})

# Test target
set (_testTarget "${_target}_test")
add_executable (${_testTarget} ${_testSources})
target_link_libraries (${_testTarget}
    PRIVATE ${_target}
            "gtest_main"
            ${FMILIB_LIBRARIES}
            ${CPPZMQ_LIBRARIES}
            "boost"
)
# FindBoost uses "old style" library references
target_include_directories(${_testTarget}
    PRIVATE "$<TARGET_PROPERTY:gtest_main,INCLUDE_DIRECTORIES>"
            $<TARGET_PROPERTY:dsbproto,INTERFACE_INCLUDE_DIRECTORIES>
)
target_compile_definitions(${_testTarget} PRIVATE
    "DSB_TEST_FMU_DIRECTORY=${CMAKE_SOURCE_DIR}/external/fmus"
)
add_test (NAME ${_testTarget} COMMAND ${_testTarget})
set_tests_properties(${_testTarget} PROPERTIES
    ENVIRONMENT "DSB_TEST_DATA_DIR=${CMAKE_SOURCE_DIR}/test_data"
)
