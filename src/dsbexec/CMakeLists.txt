set (_headers
    "library.hpp"
    "mock_library.hpp"
    "model.hpp"
    "model_parser.hpp"
    "slave_handler.hpp"
)
set (_sources
    "library.cpp"
    "main.cpp"
    "mock_library.cpp"
    "model.cpp"
    "model_parser.cpp"
    "slave_handler.cpp"
)

find_package (Boost REQUIRED)
find_package (ZMQ REQUIRED)

set (_target "dsbexec")
add_executable (${_target} ${_headers} ${_sources})
target_include_directories (${_target}
    PRIVATE ${publicHeaderDir}
            ${privateHeaderDir}
            ${CPPZMQ_INCLUDE_DIRS}
            ${Boost_INCLUDE_DIRS})
target_link_libraries (${_target} "dsb" "dsbproto" ${ZMQ_LIBRARIES})
