find_package (CPPZMQ REQUIRED)
find_package (LIBZIP REQUIRED)
find_package (ProtobufTarget REQUIRED)

# Main executable target
set (_target "broker")
add_executable (${_target} "main.cpp" $<TARGET_OBJECTS:dsbproto>)
target_link_libraries (${_target}
    PRIVATE "dsb"
            ${CPPZMQ_LIBRARIES}
            "protobuf"
)
target_include_directories (${_target}
    PRIVATE ${publicHeaderDir}
            ${privateHeaderDir}
            $<TARGET_PROPERTY:dsbproto,INTERFACE_INCLUDE_DIRECTORIES>
)
install (TARGETS ${_target} ${targetInstallDestinations})
