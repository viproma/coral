find_package (BoostTarget REQUIRED COMPONENTS program_options filesystem system)
find_package (CPPZMQ REQUIRED)
find_package (FMILIB REQUIRED)

set (_target "slave_provider")
add_executable (${_target} "main.cpp")
target_link_libraries (${_target}
    PRIVATE "dsb"
            ${FMILIB_LIBRARIES}
            ${CPPZMQ_LIBRARIES}
            "boost"
)
target_include_directories (${_target}
    PRIVATE ${publicHeaderDir}
            ${privateHeaderDir})
