set (_headers
    "library.hpp"
    "mock_library.hpp"
    "model.hpp"
    "config_parser.hpp"
)
set (_sources
    "library.cpp"
    "main.cpp"
    "mock_library.cpp"
    "model.cpp"
    "config_parser.cpp"
)

find_package (Boost REQUIRED COMPONENTS filesystem thread system chrono)
find_package (ZMQ REQUIRED)

set (_target "dsbexec")
add_executable (${_target} ${_headers} ${_sources})
target_include_directories (${_target}
    PRIVATE ${publicHeaderDir}
            ${privateHeaderDir}
            ${CPPZMQ_INCLUDE_DIRS}
            ${Boost_INCLUDE_DIRS})
target_link_libraries (${_target} "dsb" "dsbproto" ${ZMQ_LIBRARIES} ${Boost_LIBRARIES})
