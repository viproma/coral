set (_headers
    "config_parser.hpp"
)
set (_sources
    "config_parser.cpp"
    "main.cpp"
)

set (_target "coralmaster")
add_executable (${_target} ${_headers} ${_sources})
target_link_libraries (${_target} PRIVATE "coral" ${CPPZMQ_LIBRARIES})
target_include_directories (${_target}
    PRIVATE ${publicHeaderDir}
            ${privateHeaderDir})
install (TARGETS ${_target} ${targetInstallDestinations})
