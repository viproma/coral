set (_target "dsb")

set (_publicHeaders )
set (_privateHeaders
    "dsb/util/encoding.hpp")
set (_internalHeaders ) # Only visible inside the "dsb" module.
set (_sources "dsb_util_encoding.cpp")
set (_testSources "dsb_util_encoding_test.cpp")

# Add full path to non-internal headers
set (_publicHeadersFull)
foreach (h ${_publicHeaders})
    list (APPEND _publicHeadersFull "${publicHeaderDir}/${h}")
endforeach ()
set (_privateHeadersFull)
foreach (h ${_privateHeaders})
    list (APPEND _privateHeadersFull "${privateHeaderDir}/${h}")
endforeach ()

source_group ("Public Header Files" FILES ${_publicHeadersFull})
source_group ("Private Header Files" FILES ${_privateHeadersFull})
source_group ("Internal Header Files" FILES ${_internalHeaders})
include_directories (${publicHeaderDir} ${privateHeaderDir})

# Main library target
add_library (${_target} STATIC
    ${_publicHeadersFull}
    ${_privateHeadersFull}
    ${_internalHeaders}
    ${_sources})
install (TARGETS ${_target} EXPORT ${exportTarget} ${targetInstallDestinations})

# Test target
set (_testTarget "${_target}_test")
add_executable (${_testTarget} ${_testSources})
target_link_libraries (${_testTarget} ${_target} "gtest_main")
target_include_directories(
    ${_testTarget}
    PRIVATE "$<TARGET_PROPERTY:gtest_main,INCLUDE_DIRECTORIES>")
add_test (NAME ${_testTarget} COMMAND ${_testTarget})
