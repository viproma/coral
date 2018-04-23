# .rst
# FindBoostTarget
# ---------------
#
# Wraps the old-fashioned FindBoost script bundled with CMake, providing a
# more modern variant based on imported targets.
cmake_minimum_required(VERSION 3.0)

set(_self "BoostTarget")

# First, we call find_package(Boost), forwarding all find_package arguments
# to it.

set(_findPackageArgs ${${_self}_FIND_VERSION})
if(${_self}_FIND_VERSION_EXACT)
    list(APPEND _findPackageArgs "EXACT")
endif()
if (${_self}_FIND_QUIET)
    list(APPEND _findPackageArgs "QUIET")
endif()
if (${_self}_FIND_REQUIRED)
    list(APPEND _findPackageArgs "REQUIRED")
endif()

set(_requiredComponents)
set(_optionalComponents)
if(${_self}_FIND_COMPONENTS)
    foreach(_comp ${${_self}_FIND_COMPONENTS})
        if (${_self}_FIND_REQUIRED_${_comp})
            list(APPEND _requiredComponents ${_comp})
        else()
            list(APPEND _optionalComponents ${_comp})
        endif()
    endforeach()
endif()
if(_requiredComponents)
    set(_requiredComponents "COMPONENTS" ${_requiredComponents})
endif()
if(_optionalComponents)
    set(_optionalComponents "OPTIONAL_COMPONENTS" ${_optionalComponents})
endif()

find_package(Boost ${_findPackageArgs} ${_requiredComponents} ${_optionalComponents})

# If Boost was found, we create IMPORTED "boost::<comp>" targets for all
# components as well as a main "boost" target which depends on them.
if(Boost_FOUND)
    set(_subLibs)
    foreach(_c ${${_self}_FIND_COMPONENTS})
        string(TOUPPER "${_c}" _COMP)
        string(TOLOWER "${_c}" _comp)
        if(Boost_${_COMP}_FOUND)
            set(_tgt "boost::${_comp}")
            add_library(${_tgt} SHARED IMPORTED)
            set_property(TARGET ${_tgt} PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${Boost_INCLUDE_DIRS})

            if(Boost_${_COMP}_LIBRARY_RELEASE)
                set_property(TARGET ${_tgt} APPEND PROPERTY IMPORTED_CONFIGURATIONS "RELEASE")
                set_property(TARGET ${_tgt} APPEND PROPERTY IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "CXX")
                if(WIN32)
                    set_property(TARGET ${_tgt} PROPERTY IMPORTED_IMPLIB_RELEASE "${Boost_${_COMP}_LIBRARY_RELEASE}")
                else()
                    set_property(TARGET ${_tgt} PROPERTY IMPORTED_LOCATION_RELEASE "${Boost_${_COMP}_LIBRARY_RELEASE}")
                endif()
            endif()

            if(Boost_${_COMP}_LIBRARY_DEBUG)
                set_property(TARGET ${_tgt} APPEND PROPERTY IMPORTED_CONFIGURATIONS "DEBUG")
                set_property(TARGET ${_tgt} APPEND PROPERTY IMPORTED_LINK_INTERFACE_LANGUAGES_DEBUG "CXX")
                if(WIN32)
                    set_property(TARGET ${_tgt} PROPERTY IMPORTED_IMPLIB_DEBUG "${Boost_${_COMP}_LIBRARY_DEBUG}")
                else()
                    set_property(TARGET ${_tgt} PROPERTY IMPORTED_LOCATION_DEBUG "${Boost_${_COMP}_LIBRARY_DEBUG}")
                endif()
            endif()
        endif()
        list(APPEND _subLibs ${_tgt})
    endforeach()

    add_library(boost INTERFACE IMPORTED)
    set_property(TARGET boost PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${Boost_INCLUDE_DIRS})
    if(_subLibs)
        set_property(TARGET boost PROPERTY INTERFACE_LINK_LIBRARIES ${_subLibs})
        if(UNIX)
            set_property(TARGET boost APPEND PROPERTY INTERFACE_LINK_LIBRARIES "-lpthread")
        elseif(WIN32)
            set_property(TARGET boost APPEND PROPERTY INTERFACE_LINK_LIBRARIES "bcrypt")
        endif()
    endif()
endif()

set(${_self}_FOUND ${Boost_FOUND})
