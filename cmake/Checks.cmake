# Module to make checks/assertions

# Check that the  target has the proper type.
#
# This is useful for targets that can be either static or dynamic but have the same name.
function(mamba_target_check_type target expected_type log_level)
    get_target_property(actual_type ${target} TYPE)
    if(NOT actual_type STREQUAL expected_type)
        message(
            ${log_level}
            "Expected type \"${expected_type}\" for target \"${target}\""
            " but found \"${actual_type}\""
        )
    endif()
endfunction()
