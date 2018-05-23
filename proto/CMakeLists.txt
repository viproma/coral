# Creates a library target that contains the generated protocol buffers code.
set (protoFiles
    "domain.proto"
    "execution.proto"
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

protobuf_generate_cpp(sources headers ${protoFiles})

set(target "coralproto")
add_library(${target} OBJECT ${sources} ${headers} ${protoFiles})
if (MSVC)
    target_compile_options(${target} PRIVATE "/W0")
endif ()
include(ObjectTargetFunctions)
object_target_link_libraries(${target} PUBLIC protobuf::libprotobuf)
target_include_directories(${target} INTERFACE "${CMAKE_CURRENT_BINARY_DIR}")
if (UNIX)
    target_compile_options (${target} PRIVATE "-fPIC")
endif()
