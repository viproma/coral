set (_headers
    "config_parser.hpp"
)
set (_sources
    "config_parser.cpp"
    "main.cpp"
)

find_package (CPPZMQ REQUIRED)
find_package (LIBZIP REQUIRED)

set (_target "dsbexec")
add_executable (${_target} ${_headers} ${_sources})
target_link_libraries (${_target} PRIVATE "dsb" ${CPPZMQ_LIBRARIES})
target_include_directories (${_target}
    PRIVATE ${publicHeaderDir}
            ${privateHeaderDir})
install (TARGETS ${_target} ${targetInstallDestinations})
