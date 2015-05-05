find_package (Boost REQUIRED COMPONENTS thread chrono filesystem system)
find_package (CPPZMQ REQUIRED)
find_package (FMILIB REQUIRED)

set (_target "slave")
add_executable (${_target} "main.cpp")
target_link_libraries (${_target}
    PRIVATE "dsb"
            "dsbproto"
            ${FMILIB_LIBRARIES}
            ${CPPZMQ_LIBRARIES}
)
target_link_libraries (${_target} LINK_PRIVATE ${Boost_LIBRARIES}) # FindBoost uses old style library specs
target_include_directories (${_target}
    PRIVATE ${publicHeaderDir}
            ${privateHeaderDir}
            ${Boost_INCLUDE_DIRS})
