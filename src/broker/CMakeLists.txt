find_package (Boost REQUIRED COMPONENTS "filesystem" "thread" "chrono" "system") 
find_package (CPPZMQ REQUIRED)

# Main executable target
set (_target "broker")
add_executable (${_target} "main.cpp")
target_link_libraries (${_target}
    PRIVATE "dsb"
            "dsbproto"
            ${CPPZMQ_LIBRARIES}
)
target_link_libraries (${_target} LINK_PRIVATE ${Boost_LIBRARIES}) # FindBoost uses old style library specs
target_include_directories (${_target}
    PRIVATE ${publicHeaderDir}
            ${privateHeaderDir}
            ${Boost_INCLUDE_DIRS}
)
