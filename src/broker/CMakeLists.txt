find_package (BoostTarget REQUIRED COMPONENTS "filesystem" "thread" "chrono" "system") 
find_package (CPPZMQ REQUIRED)
find_package (Protobuf REQUIRED)

# Main executable target
set (_target "broker")
add_executable (${_target} "main.cpp" $<TARGET_OBJECTS:dsbproto>)
target_link_libraries (${_target}
    PRIVATE "dsb"
            ${CPPZMQ_LIBRARIES}
            ${PROTOBUF_LIBRARIES}
            "boost"
)
target_include_directories (${_target}
    PRIVATE ${publicHeaderDir}
            ${privateHeaderDir}
            $<TARGET_PROPERTY:dsbproto,INTERFACE_INCLUDE_DIRECTORIES>
)
