# Module to set Link Time Optimization flags

include(CheckIPOSupported)


# Detect is setting Link Time Optimization is recommended.
#
# Currenlty checks if LTO is supported and if the build is a release.
function(mamba_should_lto)
    # Names of option parameters (without arguments)
    set(options)
    # Names of named parameters with a single argument
    set(oneValueArgs RESULT OUTPUT)
    # Names of named parameters with a multiple arguments
    set(multiValueArgs)
    cmake_parse_arguments(arg "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
    # Extra arguments not accounted for
    if(arg_UNPARSED_ARGUMENTS)
        message(
            AUTHOR_WARNING
            "Unrecoginzed options passed to ${CMAKE_CURRENT_FUNCTION}: "
            "${ARG_UNPARSED_ARGUMENTS}"
        )
    endif()

    # Check if we are building in a release-like build
    string(TOLOWER "${CMAKE_BUILD_TYPE}" build_type_lower)
    set(valid_release_names "release" "relwithdebinfo")
    if(NOT ${build_type_lower} IN_LIST valid_release_names)
        set(${arg_RESULT} FALSE PARENT_SCOPE)
        set(${arg_OUTPUT} "the build type is not a release" PARENT_SCOPE)
        return()
    endif()

    # Check if LTO is supported by compiler
    check_ipo_supported(RESULT lto_is_supported OUTPUT lto_not_supported_reason)
    if(NOT lto_is_supported)
        set(${arg_RESULT} FALSE PARENT_SCOPE)
        set(${arg_OUTPUT} "${lto_not_supported_reason}" PARENT_SCOPE)
    endif()

    set(${arg_RESULT} TRUE PARENT_SCOPE)
endfunction()


# Set Link Time Optimization on a given target.
#
# MODE parameter takes the possible values
# - A false constant: deactivate LTO
# - A true constant: activate LTO, fails if this is not supported by the compiler
# - "Default" or "Auto": set LTO if supported and the build type is a release.
function(mamba_target_set_lto target)
    # Names of option parameters (without arguments)
    set(options)
    # Names of named parameters with a single argument
    set(oneValueArgs MODE)
    # Names of named parameters with a multiple arguments
    set(multiValueArgs)
    cmake_parse_arguments(arg "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
    # Extra arguments not accounted for
    if(arg_UNPARSED_ARGUMENTS)
        message(
            AUTHOR_WARNING
            "Unrecoginzed parameter passed to ${CMAKE_CURRENT_FUNCTION}: "
            "'${arg_UNPARSED_ARGUMENTS}'"
        )
        return()
    endif()

    mamba_should_lto(RESULT should_lto OUTPUT lto_message)
    string(TOLOWER ${arg_MODE} arg_MODE_lower)
    set(valid_default_names "default" "auto" "")
    if(arg_MODE_lower IN_LIST valid_default_names)
        set(is_default TRUE)
    endif()

    if("${arg_MODE}" OR (is_default AND should_lto))
        message(STATUS "Setting LTO for target ${PROJECT_NAME}::${target}")
        set_property(TARGET ${arg_TARGET} PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
    else()
        if(is_default)
            message(STATUS "Skipping LTO for target ${PROJECT_NAME}::${target}, ${lto_message}")
        else()
            message(STATUS "Skipping LTO for target ${PROJECT_NAME}::${target}")
        endif()
    endif()
endfunction()
