# Copyright (c) 2019, QuantStack and Mamba Contributors
#
# Distributed under the terms of the BSD 3-Clause License.
#
# The full license is in the file LICENSE, distributed with this software.

# Common component - core utilities, specs, context No heavy dependencies (no libarchive, no
# libcurl)

# These variables should be set by the parent CMakeLists.txt
if(NOT DEFINED LIBMAMBA_SOURCE_DIR)
    set(LIBMAMBA_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../src)
endif()
if(NOT DEFINED LIBMAMBA_INCLUDE_DIR)
    set(LIBMAMBA_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../include)
endif()
if(NOT DEFINED SHELL_SCRIPTS)
    # Define empty if not set
    set(SHELL_SCRIPTS "")
endif()

set(
    LIBMAMBA_COMMON_SOURCES
    ${LIBMAMBA_SOURCE_DIR}/version.cpp
    # Filesystem library
    ${LIBMAMBA_SOURCE_DIR}/fs/filesystem.cpp
    # C++ utility library (excluding url.cpp which needs curl)
    ${LIBMAMBA_SOURCE_DIR}/util/cfile.cpp
    ${LIBMAMBA_SOURCE_DIR}/util/cryptography.cpp
    ${LIBMAMBA_SOURCE_DIR}/util/encoding.cpp
    ${LIBMAMBA_SOURCE_DIR}/util/environment.cpp
    ${LIBMAMBA_SOURCE_DIR}/util/os_linux.cpp
    ${LIBMAMBA_SOURCE_DIR}/util/os_osx.cpp
    ${LIBMAMBA_SOURCE_DIR}/util/os_unix.cpp
    ${LIBMAMBA_SOURCE_DIR}/util/os_win.cpp
    ${LIBMAMBA_SOURCE_DIR}/util/parsers.cpp
    ${LIBMAMBA_SOURCE_DIR}/util/path_manip.cpp
    ${LIBMAMBA_SOURCE_DIR}/util/random.cpp
    ${LIBMAMBA_SOURCE_DIR}/util/string.cpp
    ${LIBMAMBA_SOURCE_DIR}/util/url_manip.cpp
    # Implementation of version and matching specs
    ${LIBMAMBA_SOURCE_DIR}/specs/archive.cpp
    ${LIBMAMBA_SOURCE_DIR}/specs/authentication_info.cpp
    ${LIBMAMBA_SOURCE_DIR}/specs/build_number_spec.cpp
    ${LIBMAMBA_SOURCE_DIR}/specs/channel.cpp
    ${LIBMAMBA_SOURCE_DIR}/specs/chimera_string_spec.cpp
    ${LIBMAMBA_SOURCE_DIR}/specs/conda_url.cpp
    ${LIBMAMBA_SOURCE_DIR}/specs/glob_spec.cpp
    ${LIBMAMBA_SOURCE_DIR}/specs/match_spec.cpp
    ${LIBMAMBA_SOURCE_DIR}/specs/package_info.cpp
    ${LIBMAMBA_SOURCE_DIR}/specs/platform.cpp
    ${LIBMAMBA_SOURCE_DIR}/specs/regex_spec.cpp
    ${LIBMAMBA_SOURCE_DIR}/specs/repo_data.cpp
    ${LIBMAMBA_SOURCE_DIR}/specs/unresolved_channel.cpp
    ${LIBMAMBA_SOURCE_DIR}/specs/version_spec.cpp
    ${LIBMAMBA_SOURCE_DIR}/specs/version.cpp
    # Core API (low-level) - common parts
    ${LIBMAMBA_SOURCE_DIR}/core/activation.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/channel_context.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/context.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/env_lockfile.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/env_lockfile_conda.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/env_lockfile_mambajs.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/environments_manager.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/error_handling.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/execution.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/fsutil.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/history.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/link.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/logging.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/menuinst.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/output.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/package_paths.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/pinning.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/prefix_data.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/progress_bar_impl.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/progress_bar.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/query.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/run.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/shell_init.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/singletons.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/thread_utils.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/timeref.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/transaction_context.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/util_os.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/util.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/virtual_packages.cpp
    # Artifacts validation (no download dependency) Note: repo_checker.cpp and
    # update_framework_v0_6.cpp need download, so they're in network component
    ${LIBMAMBA_SOURCE_DIR}/validation/errors.cpp
    ${LIBMAMBA_SOURCE_DIR}/validation/keys.cpp
    ${LIBMAMBA_SOURCE_DIR}/validation/tools.cpp
    ${LIBMAMBA_SOURCE_DIR}/validation/update_framework_v1.cpp
    ${LIBMAMBA_SOURCE_DIR}/validation/update_framework.cpp
)

set(
    LIBMAMBA_COMMON_PUBLIC_HEADERS
    ${LIBMAMBA_INCLUDE_DIR}/mamba/version.hpp
    # Filesystem library
    ${LIBMAMBA_INCLUDE_DIR}/mamba/fs/filesystem.hpp
    # Utility library
    ${LIBMAMBA_INCLUDE_DIR}/mamba/util/build.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/util/cast.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/util/cfile.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/util/conditional.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/util/cryptography.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/util/deprecation.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/util/encoding.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/util/environment.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/util/flat_binary_tree.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/util/flat_bool_expr_tree.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/util/flat_set.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/util/graph.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/util/heap_optional.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/util/iterator.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/util/json.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/util/loop_control.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/util/os_linux.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/util/os_osx.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/util/os_unix.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/util/os_win.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/util/os.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/util/parsers.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/util/path_manip.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/util/random.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/util/string.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/util/synchronized_value.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/util/tuple_hash.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/util/type_traits.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/util/url_manip.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/util/variant_cmp.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/util/weakening_map.hpp
    # Implementation of version and matching specs
    ${LIBMAMBA_INCLUDE_DIR}/mamba/specs/archive.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/specs/authentication_info.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/specs/build_number_spec.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/specs/channel.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/specs/chimera_string_spec.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/specs/conda_url.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/specs/error.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/specs/glob_spec.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/specs/match_spec.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/specs/package_info.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/specs/platform.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/specs/regex_spec.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/specs/repo_data.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/specs/unresolved_channel.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/specs/version_spec.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/specs/version.hpp
    # Core API (low-level)
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/activation.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/channel_context.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/context.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/context_params.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/env_lockfile.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/environments_manager.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/error_handling.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/execution.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/fsutil.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/history.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/invoke.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/logging.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/logging_tools.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/menuinst.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/output.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/package_paths.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/palette.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/pinning.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/prefix_data.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/progress_bar.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/query.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/run.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/shell_init.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/tasksync.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/thread_utils.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/timeref.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/util_os.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/util_scope.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/util.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/virtual_packages.hpp
    # Artifacts validation
    ${LIBMAMBA_INCLUDE_DIR}/mamba/validation/errors.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/validation/keys.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/validation/tools.hpp
    # Note: repo_checker.hpp and update_framework_v0_6.hpp headers stay here for API compatibility
    ${LIBMAMBA_INCLUDE_DIR}/mamba/validation/repo_checker.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/validation/update_framework_v0_6.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/validation/update_framework_v1.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/validation/update_framework.hpp
)

# Shell scripts are generated in the parent CMakeLists.txt They need to be added to common component
# since shell_init.cpp, activation.cpp, and transaction_context.cpp use them The parent generates
# them in ${CMAKE_CURRENT_BINARY_DIR}/shell_scripts/ (which is libmamba/shell_scripts/) We reference
# them with the correct path
if(DEFINED SHELL_SCRIPTS AND SHELL_SCRIPTS)
    # Use LIBMAMBA_BINARY_DIR if defined (set by parent), otherwise compute it
    if(DEFINED LIBMAMBA_BINARY_DIR)
        set(PARENT_BINARY_DIR "${LIBMAMBA_BINARY_DIR}")
    else()
        get_filename_component(PARENT_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}" DIRECTORY)
        get_filename_component(PARENT_BINARY_DIR "${PARENT_BINARY_DIR}" ABSOLUTE)
    endif()
    foreach(script IN LISTS SHELL_SCRIPTS)
        # The custom command in the parent CMakeLists.txt uses OUTPUT shell_scripts/${script}.cpp
        # which is relative to LIBMAMBA_BINARY_DIR. Build the absolute path to match.
        set(SHELL_SCRIPT_PATH "${PARENT_BINARY_DIR}/shell_scripts/${script}.cpp")
        list(APPEND LIBMAMBA_COMMON_SOURCES "${SHELL_SCRIPT_PATH}")
    endforeach()
endif()

# Create the common library target
macro(libmamba_common_create_target target_name linkage output_name)
    string(TOUPPER "${linkage}" linkage_upper)
    if(NOT ${linkage_upper} MATCHES "^(SHARED|STATIC)$")
        message(FATAL_ERROR "Invalid library linkage: ${linkage}")
    endif()

    add_library(
        ${target_name}
        ${linkage_upper} ${LIBMAMBA_COMMON_PUBLIC_HEADERS} ${LIBMAMBA_COMMON_SOURCES}
    )

    # Mark shell scripts as generated files so CMake knows they'll be created
    if(DEFINED SHELL_SCRIPTS AND SHELL_SCRIPTS)
        # Use LIBMAMBA_BINARY_DIR if defined (set by parent), otherwise compute it
        if(DEFINED LIBMAMBA_BINARY_DIR)
            set(PARENT_BINARY_DIR "${LIBMAMBA_BINARY_DIR}")
        else()
            get_filename_component(PARENT_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}" DIRECTORY)
            get_filename_component(PARENT_BINARY_DIR "${PARENT_BINARY_DIR}" ABSOLUTE)
        endif()
        foreach(script IN LISTS SHELL_SCRIPTS)
            set(SHELL_SCRIPT_PATH "${PARENT_BINARY_DIR}/shell_scripts/${script}.cpp")
            set_source_files_properties("${SHELL_SCRIPT_PATH}" PROPERTIES GENERATED TRUE)
        endforeach()
    endif()

    # Context depends on mirror_map (network) - this dependency will be added after network is built
    # to resolve the circular dependency (network also depends on common)

    target_include_directories(
        ${target_name}
        PUBLIC $<BUILD_INTERFACE:${LIBMAMBA_INCLUDE_DIR}> $<INSTALL_INTERFACE:include>
        PRIVATE ${LIBMAMBA_SOURCE_DIR}
    )

    # Header only libraries
    target_link_libraries(${target_name} PUBLIC tl::expected nlohmann_json::nlohmann_json msgpack-c)

    target_compile_features(${target_name} PUBLIC cxx_std_20)
    set_target_properties(
        ${target_name}
        PROPERTIES
            CXX_STANDARD 20
            CXX_STANDARD_REQUIRED YES
            CXX_EXTENSIONS NO
    )

    mamba_target_add_compile_warnings(${target_name} WARNING_AS_ERROR ${MAMBA_WARNING_AS_ERROR})
    mamba_target_set_lto(${target_name} MODE ${MAMBA_LTO})

    # For shared libraries, common uses symbols from network and solver but can't link to them due
    # to circular dependencies. Allow undefined symbols - they'll be resolved when the aggregated
    # libmamba target links all components together. We also need to add DT_NEEDED entries so that
    # when libmamba-common.so is loaded, the dynamic linker will also load libmamba-solver.so and
    # libmamba-network.so. We do this using linker flags to avoid creating circular dependencies.
    if(${linkage_upper} STREQUAL "SHARED")
        if(APPLE)
            target_link_options(${target_name} PRIVATE -Wl,-undefined,dynamic_lookup)
        elseif(UNIX)
            # On Linux, we can use --allow-shlib-undefined or rely on the aggregated target to
            # provide all symbols at link time
            target_link_options(${target_name} PRIVATE -Wl,--allow-shlib-undefined)
            # Add DT_NEEDED entries for solver and network libraries so they're loaded at runtime We
            # use -Wl,--no-as-needed to ensure these libraries are added to DT_NEEDED even if
            # they're not directly referenced at link time. The actual library paths will be
            # resolved by the aggregated target linking all components together. Note: We can't use
            # target_link_libraries here because it would create a circular dependency. Instead,
            # we'll add these as linker options after the components are built.
        elseif(WIN32)
            # On Windows, use /FORCE:UNRESOLVED to allow unresolved symbols. The symbols will be
            # resolved at runtime when the DLLs are loaded. The aggregated libmamba target will
            # ensure all DLLs are available together. Note: We can't use delay loading (/DELAYLOAD)
            # because the DLLs don't exist yet at link time. /FORCE:UNRESOLVED allows the DLL to be
            # created with unresolved symbols, which will be resolved when the DLLs are loaded
            # together.
            target_link_options(${target_name} PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/FORCE:UNRESOLVED>)
        endif()
    endif()

    # singletons.cpp uses CURL, so we need it even in common This is unfortunate but necessary for
    # the singleton initialization order
    if(${linkage_upper} STREQUAL "STATIC")
        mamba_target_check_type(yaml-cpp::yaml-cpp STATIC_LIBRARY FATAL_ERROR)
        mamba_target_check_type(reproc STATIC_LIBRARY FATAL_ERROR)
        mamba_target_check_type(reproc++ STATIC_LIBRARY FATAL_ERROR)

        target_link_libraries(
            ${target_name}
            PUBLIC fmt::fmt-header-only yaml-cpp::yaml-cpp msgpack-c
            PRIVATE reproc reproc++ simdjson::simdjson_static
        )

        # CURL is needed for singletons.cpp (curl_global_init)
        if(UNIX)
            find_library(CURL_LIB_STATIC NAMES libcurl.a curl)
            if(CURL_LIB_STATIC)
                target_link_libraries(${target_name} PRIVATE ${CURL_LIB_STATIC})
            endif()
        elseif(WIN32)
            find_package(CURL CONFIG QUIET)
            if(TARGET CURL::libcurl)
                target_link_libraries(${target_name} PRIVATE CURL::libcurl)
                add_compile_definitions(CURL_STATICLIB)
            endif()
        endif()
    else()
        mamba_target_check_type(yaml-cpp::yaml-cpp SHARED_LIBRARY WARNING)

        target_link_libraries(
            ${target_name}
            PUBLIC yaml-cpp::yaml-cpp fmt::fmt msgpack-c
            PRIVATE reproc reproc++ simdjson::simdjson
        )

        # CURL is needed for singletons.cpp (curl_global_init)
        find_package(CURL QUIET)
        if(CURL_FOUND)
            target_link_libraries(${target_name} PRIVATE ${CURL_LIBRARIES})
            target_include_directories(${target_name} PRIVATE ${CURL_INCLUDE_DIRS})
        endif()

        # OpenSSL is needed for validation/tools.cpp (EVP functions)
        find_package(OpenSSL QUIET)
        if(OpenSSL_FOUND)
            target_link_libraries(${target_name} PRIVATE OpenSSL::SSL OpenSSL::Crypto)
        endif()
    endif()

    if(WIN32)
        find_path(
            WINREG_INCLUDE_DIR
            NAMES WinReg.hpp
            PATH_SUFFIXES winreg
        )
        target_include_directories(${target_name} PRIVATE ${WINREG_INCLUDE_DIR})
    endif()

    if(UNIX)
        math(EXPR LIBMAMBA_BINARY_COMPATIBLE "${LIBMAMBA_BINARY_CURRENT} - ${LIBMAMBA_BINARY_AGE}")
        set_target_properties(
            ${target_name}
            PROPERTIES
                COMPILE_DEFINITIONS "LIBMAMBA_COMMON_EXPORTS"
                PREFIX ""
                VERSION
                "${LIBMAMBA_BINARY_COMPATIBLE}.${LIBMAMBA_BINARY_REVISION}.${LIBMAMBA_BINARY_AGE}"
                SOVERSION ${LIBMAMBA_BINARY_COMPATIBLE}
                OUTPUT_NAME "${output_name}"
        )
    else()
        set_target_properties(
            ${target_name}
            PROPERTIES
                COMPILE_DEFINITIONS "LIBMAMBA_COMMON_EXPORTS"
                PREFIX ""
                VERSION ${LIBMAMBA_BINARY_VERSION}
                SOVERSION ${LIBMAMBA_BINARY_CURRENT}
                OUTPUT_NAME "${output_name}"
        )
        target_compile_definitions(${target_name} PUBLIC GHC_WIN_DISABLE_WSTRING_STORAGE_TYPE)
    endif()

    if(${linkage_upper} STREQUAL "STATIC")
        find_package(Threads REQUIRED)
        target_link_libraries(${target_name} PUBLIC Threads::Threads)
    endif()

    list(APPEND libmamba_common_targets ${target_name})
    add_library(mamba::${target_name} ALIAS ${target_name})
endmacro()

set(libmamba_common_targets "")
