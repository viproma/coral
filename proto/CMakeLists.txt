# Creates a library target that contains the generated protocol buffers code.
set (protoFiles
    "broker.proto"
    "domain.proto"
    "domain_controller.proto"
    "execution.proto"
    "execution_controller.proto"
    "exe_data.proto"
    "model.proto"
    "net.proto"
    "testing.proto"
)

# We want the files to appear in the Visual Studio UI, so we add them
# as source files in the library below, but prevent compilation with
# a CMake property.
set_source_files_properties (${protoFiles} PROPERTIES HEADER_FILE_ONLY TRUE)
source_group("Protobuf Files" FILES ${protoFiles})

find_package (ProtobufTarget REQUIRED)
PROTOBUF_GENERATE_CPP (sources headers ${protoFiles})
set (target "dsbproto")
add_library (${target} OBJECT ${sources} ${headers} ${protoFiles})
target_include_directories (${target}
    INTERFACE ${CMAKE_CURRENT_BINARY_DIR}
    PUBLIC $<TARGET_PROPERTY:protobuf,INTERFACE_INCLUDE_DIRECTORIES>)
if (UNIX)
    target_compile_options (${target} PRIVATE "-fPIC")
endif()
