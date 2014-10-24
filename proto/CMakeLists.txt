# Creates a library target that contains the generated protocol buffers code.
set (protoFiles
    "control.proto"
    "domain.proto"
    "testing.proto"
    "variable.proto"
)

# We want the files to appear in the Visual Studio UI, so we add them
# as source files in the library below, but prevent compilation with
# a CMake property.
set_source_files_properties (${protoFiles} PROPERTIES HEADER_FILE_ONLY TRUE)
source_group("Protobuf Files" FILES ${protoFiles})

find_package (Protobuf REQUIRED)
PROTOBUF_GENERATE_CPP (sources headers ${protoFiles})
set (target "dsbproto")
add_library (${target} STATIC ${sources} ${headers} ${protoFiles})
target_include_directories (${target}
    INTERFACE ${CMAKE_CURRENT_BINARY_DIR}
    PUBLIC ${PROTOBUF_INCLUDE_DIRS})
target_link_libraries(${target}
    INTERFACE ${PROTOBUF_LIBRARIES})
