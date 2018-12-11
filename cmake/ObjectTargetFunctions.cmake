# Defines a couple of functions which allows one to treat OBJECT library
# targets more like other library targets, in particular with regards to
# propagation of transitive usage requirements.
#
cmake_minimum_required(VERSION 3.1)


# Links a target to one or more OBJECT library targets.
#
# Transitive usage requirements specified in the INTERFACE_* properties
# of each OBJECT target will be propagated, but only as part of the build
# interface.  They will not be part of the export (install) interface.
#
# Usage:
#
#   target_link_objects(
#       <target>
#       <PRIVATE|PUBLIC|INTERFACE> <object target> ...
#       [<PRIVATE|PUBLIC|INTERFACE> <object target> ...]
#       ...
#   )
#
function(target_link_objects target)
    set(privacy "PRIVATE")
    foreach(arg IN LISTS ARGN)
        if((arg STREQUAL "PRIVATE") OR (arg STREQUAL "PUBLIC") OR (arg STREQUAL "INTERFACE"))
            set(privacy "${arg}")
        else()
            target_sources(${target} PRIVATE $<TARGET_OBJECTS:${arg}>)
            target_compile_definitions(${target} ${privacy} $<BUILD_INTERFACE:$<TARGET_PROPERTY:${arg},INTERFACE_COMPILE_DEFINITIONS>>)
            target_compile_features   (${target} ${privacy} $<BUILD_INTERFACE:$<TARGET_PROPERTY:${arg},INTERFACE_COMPILE_FEATURES>>)
            target_compile_options    (${target} ${privacy} $<BUILD_INTERFACE:$<TARGET_PROPERTY:${arg},INTERFACE_COMPILE_OPTIONS>>)
            target_include_directories(${target} ${privacy} $<BUILD_INTERFACE:$<TARGET_PROPERTY:${arg},INTERFACE_INCLUDE_DIRECTORIES>>)
            target_link_libraries     (${target} ${privacy} $<BUILD_INTERFACE:$<TARGET_PROPERTY:${arg},INTERFACE_LINK_LIBRARIES>>)
        endif()
    endforeach()
endfunction(target_link_objects)


# Propagates usage requirements from one or more library targets to an
# OBJECT library target.
#
# Usage:
#
#   object_target_link_libraries(
#       <object target>
#       <PRIVATE|BULIC|INTERFACE> <library target> ...
#       [<PRIVATE|BULIC|INTERFACE> <library target> ...]
#       ...
#   )
#
function(object_target_link_libraries target)
    set(privacy "PRIVATE")
    foreach(arg IN LISTS ARGN)
        if((arg STREQUAL "PRIVATE") OR (arg STREQUAL "PUBLIC") OR (arg STREQUAL "INTERFACE"))
            set(privacy "${arg}")
        else()
            target_compile_definitions(${target} ${privacy} $<TARGET_PROPERTY:${arg},INTERFACE_COMPILE_DEFINITIONS>)
            target_compile_features(${target} ${privacy}    $<TARGET_PROPERTY:${arg},INTERFACE_COMPILE_FEATURES>)
            target_compile_options(${target} ${privacy}     $<TARGET_PROPERTY:${arg},INTERFACE_COMPILE_OPTIONS>)
            target_include_directories(${target} ${privacy} $<TARGET_PROPERTY:${arg},INTERFACE_INCLUDE_DIRECTORIES>)
            if ((privacy STREQUAL "PUBLIC") OR (privacy STREQUAL "INTERFACE"))
                set_property(
                    TARGET ${target}
                    APPEND
                    PROPERTY INTERFACE_LINK_LIBRARIES ${arg}
                )
                get_target_property(deps ${arg} INTERFACE_LINK_LIBRARIES)
                if (deps)
                    set_property(
                        TARGET ${target}
                        APPEND
                        PROPERTY INTERFACE_LINK_LIBRARIES ${deps}
                    )
                endif ()
            endif()
        endif()
    endforeach()
endfunction(object_target_link_libraries)
