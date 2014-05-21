set (_headers
    "model.hpp"
    "model_parser.hpp"
)
set (_sources
    "main.cpp"
    "model.cpp"
    "model_parser.cpp"
)

find_package (Boost REQUIRED)
find_package (ZMQ REQUIRED)

set (_target "dsbexec")
add_executable (${_target} ${_headers} ${_sources})
target_include_directories (${_target} PRIVATE ${privateHeaderDir} ${CPPZMQ_INCLUDE_DIRS})
target_link_libraries (${_target} "dsb" "dsbproto" ${ZMQ_LIBRARIES})
