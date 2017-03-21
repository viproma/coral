set (_target "coral")

set (_publicHeaders
    "coral/config.h"
    "coral/bus/variable_io.hpp"
    "coral/fmi.hpp"
    "coral/fmi/fmu.hpp"
    "coral/fmi/fmu1.hpp"
    "coral/fmi/fmu2.hpp"
    "coral/fmi/importer.hpp"
    "coral/log.hpp"
    "coral/master.hpp"
    "coral/master/cluster.hpp"
    "coral/master/execution.hpp"
    "coral/master/execution_options.hpp"
    "coral/model.hpp"
    "coral/net.hpp"
    "coral/provider.hpp"
    "coral/provider/provider.hpp"
    "coral/provider/slave_creator.hpp"
    "coral/slave.hpp"
    "coral/slave/exception.hpp"
    "coral/slave/instance.hpp"
    "coral/slave/logging.hpp"
    "coral/slave/runner.hpp"
    "coral/util/filesystem.hpp"
)
set (_privateHeaders
    "coral/async.hpp"
    "coral/bus/execution_manager.hpp"
    "coral/bus/execution_manager_private.hpp"
    "coral/bus/execution_state.hpp"
    "coral/bus/slave_agent.hpp"
    "coral/bus/slave_controller.hpp"
    "coral/bus/slave_control_messenger.hpp"
    "coral/bus/slave_control_messenger_v0.hpp"
    "coral/bus/slave_provider_comm.hpp"
    "coral/bus/slave_setup.hpp"
    "coral/net/ip.hpp"
    "coral/net/reactor.hpp"
    "coral/net/reqrep.hpp"
    "coral/net/service.hpp"
    "coral/net/udp.hpp"
    "coral/net/zmqx.hpp"
    "coral/error.hpp"
    "coral/fmi/glue.hpp"
    "coral/fmi/windows.hpp"
    "coral/protobuf.hpp"
    "coral/protocol/domain.hpp"
    "coral/protocol/exe_data.hpp"
    "coral/protocol/execution.hpp"
    "coral/protocol/glue.hpp"
    "coral/util.hpp"
    "coral/util/console.hpp"
    "coral/util/zip.hpp"
)
set (_sources
    "bus_variable_io.cpp"
    "fmi_fmu1.cpp"
    "fmi_fmu2.cpp"
    "fmi_importer.cpp"
    "log.cpp"
    "master_cluster.cpp"
    "master_execution.cpp"
    "model.cpp"
    "provider_provider.cpp"
    "slave_logging.cpp"
    "slave_runner.cpp"
    "net.cpp"
    "util_filesystem.cpp"

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
    "fmi_glue.cpp"
    "fmi_windows.cpp"
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
    "protocol_glue.cpp"
    "util.cpp"
    "util_console.cpp"
    "util_zip.cpp"
)
set (_testSources
    "bus_variable_io_test.cpp"

    "async_test.cpp"
    "error_test.cpp"
    "fmi_fmu1_test.cpp"
    "fmi_fmu2_test.cpp"
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
    "util_filesystem_test.cpp"
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
# "package name", i.e. the name of their subdirectory under "coral/".
macro (CreateHeaderGroups _superGroup)
    foreach (_hdr ${ARGN})
        unset (_pkg)
        string (REGEX REPLACE ".*coral/([a-zA-Z0-9_]+)/[a-zA-Z0-9_]+.hpp" "\\1" _pkg "${_hdr}")
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
    $<TARGET_OBJECTS:coralproto>)
target_include_directories(${_target}
    PRIVATE $<TARGET_PROPERTY:coralproto,INTERFACE_INCLUDE_DIRECTORIES>)
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
if (CORAL_BUILD_TESTS)
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
                $<TARGET_PROPERTY:coralproto,INTERFACE_INCLUDE_DIRECTORIES>
    )
    target_compile_definitions(${_testTarget} PRIVATE
        "CORAL_TEST_FMU_DIRECTORY=${CMAKE_SOURCE_DIR}/external/fmus"
    )
    add_test (NAME ${_testTarget} COMMAND ${_testTarget})
    set_tests_properties(${_testTarget} PROPERTIES
        ENVIRONMENT "CORAL_TEST_DATA_DIR=${CMAKE_SOURCE_DIR}/test_data"
    )
endif ()
