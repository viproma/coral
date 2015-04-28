set (_headers
    "config_parser.hpp"
)
set (_sources
    "config_parser.cpp"
    "main.cpp"
)

find_package (Boost REQUIRED COMPONENTS filesystem thread system chrono program_options)
find_package (CPPZMQ REQUIRED)

set (_target "dsbexec")
add_executable (${_target} ${_headers} ${_sources})
target_link_libraries (${_target} PRIVATE "dsb" "dsbproto" ${CPPZMQ_LIBRARIES})
target_link_libraries (${_target} LINK_PRIVATE ${Boost_LIBRARIES}) # FindBoost uses old style library specs
target_include_directories (${_target}
    PRIVATE ${publicHeaderDir}
            ${privateHeaderDir}
            ${Boost_INCLUDE_DIRS})
