# Detect resolvo-cpp >= 0.3.0 builds that export ConditionId / ConditionalRequirement. win-64 0.3.0
# (h0a879f1_1) predates this API; linux-64 0.3.0 (hf9e0b3e_1) includes it. Temporary until
# conda-forge republishes a consistent resolvo-cpp 0.3.0 on all platforms.

set(LIBMAMBA_RESOLVO_HAS_CONDITIONS FALSE)

find_file(
    _libmamba_resolvo_dep_header
    NAMES resolvo/resolvo_dependency_provider.h
    PATHS ${CMAKE_PREFIX_PATH}
    PATH_SUFFIXES include Library/include
    NO_DEFAULT_PATH
)
if(NOT _libmamba_resolvo_dep_header)
    find_file(
        _libmamba_resolvo_dep_header
        NAMES resolvo/resolvo_dependency_provider.h
        PATH_SUFFIXES include Library/include
    )
endif()

if(_libmamba_resolvo_dep_header)
    file(READ "${_libmamba_resolvo_dep_header}" _libmamba_resolvo_dep_header_content)
    if(_libmamba_resolvo_dep_header_content MATCHES "using cbindgen_private::ConditionId;")
        set(LIBMAMBA_RESOLVO_HAS_CONDITIONS TRUE)
    endif()
endif()

if(LIBMAMBA_RESOLVO_HAS_CONDITIONS)
    message(STATUS "resolvo-cpp: conditional requirements API available")
else()
    message(
        STATUS
            "resolvo-cpp: legacy API without conditional requirements (temporary platform workaround)"
    )
endif()

unset(_libmamba_resolvo_dep_header)
unset(_libmamba_resolvo_dep_header_content)
