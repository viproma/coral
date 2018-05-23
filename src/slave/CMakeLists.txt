set (_target "coralslave")
add_executable (${_target} "main.cpp")
target_link_libraries (${_target}
    PRIVATE "coral"
            ${FMILIB_LIBRARIES}
            ${CPPZMQ_LIBRARIES}
)
target_include_directories (${_target}
    PRIVATE ${publicHeaderDir}
            ${privateHeaderDir})
install (TARGETS ${_target} ${targetInstallDestinations})
