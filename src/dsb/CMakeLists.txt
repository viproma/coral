set (_target "dsb")

set (_publicHeaders
    "dsb.hpp"
    "dsb/util.hpp"
    "dsb/util/encoding.hpp")
set (_internalHeaders )
set (_sources "dsb_util_encoding.cpp")
set (_testSources "dsb_util_encoding_test.cpp")

# Public API headers are located elsewhere
include_directories (${publicHeaderDir})
set (_publicHeadersFull)
foreach (_hdr ${_publicHeaders})
    list (APPEND _publicHeadersFull "${publicHeaderDir}/${_hdr}")
endforeach ()
source_group ("Public Header Files" FILES ${_publicHeadersFull})

# Main library target
add_library (${_target} STATIC
    ${_publicHeadersFull}
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
