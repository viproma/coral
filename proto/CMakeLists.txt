# Creates a library target that contains the generated protocol buffers code.
set (protoFiles
    "control.proto"
    "testing.proto"
    "variable.proto"
)
source_group("Protobuf Files" FILES ${protoFiles})

find_package (Protobuf REQUIRED)
PROTOBUF_GENERATE_CPP (sources headers ${protoFiles})
set (target "dsbproto")
add_library (${target} STATIC ${sources} ${headers})
target_include_directories (${target}
    INTERFACE ${CMAKE_CURRENT_BINARY_DIR}
    PUBLIC ${PROTOBUF_INCLUDE_DIRS})
target_link_libraries(${target}
    INTERFACE ${PROTOBUF_LIBRARIES})
