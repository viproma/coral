set (_target "dsb")

set (_publicHeaders "dsb.hpp")
set (_internalHeaders "dsb_internal.hpp")
set (_sources "dsb.cpp")

set (_publicHeadersFull)
foreach (_hdr ${_publicHeaders})
    list (APPEND _publicHeadersFull "${publicHeaderDir}/${_hdr}")
endforeach ()

source_group ("Public Header Files" ${_publicHeadersFull})

include_directories (${publicHeaderDir})
add_library (${_target} STATIC
    ${_publicHeadersFull}
    ${_internalHeaders}
    ${_sources})
install (TARGETS ${_target} EXPORT ${exportTarget} ${targetInstallDestinations})
