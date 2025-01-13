# Copyright (c) 2019, QuantStack and Mamba Contributors
#
# Distributed under the terms of the BSD 3-Clause License.
#
# The full license is in the file LICENSE, distributed with this software.

cmake_minimum_required(VERSION 3.16)
cmake_policy(SET CMP0025 NEW) # Introduced in cmake 3.0
cmake_policy(SET CMP0077 NEW) # Introduced in cmake 3.13
project(libmamba)

set(LIBMAMBA_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)
set(LIBMAMBA_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(LIBMAMBA_DATA_DIR ${CMAKE_CURRENT_SOURCE_DIR}/data)

# Versioning
# ===========
file(
    STRINGS "${LIBMAMBA_INCLUDE_DIR}/mamba/version.hpp" libmamba_version_defines
    REGEX "#define LIBMAMBA_VERSION_(MAJOR|MINOR|PATCH)"
)
foreach(ver ${libmamba_version_defines})
    if(ver MATCHES "#define LIBMAMBA_VERSION_(MAJOR|MINOR|PATCH) +([^ ]+)$")
        set(
            LIBMAMBA_VERSION_${CMAKE_MATCH_1}
            "${CMAKE_MATCH_2}"
            CACHE INTERNAL ""
        )
    endif()
endforeach()
set(
    ${PROJECT_NAME}_VERSION
    ${LIBMAMBA_VERSION_MAJOR}.${LIBMAMBA_VERSION_MINOR}.${LIBMAMBA_VERSION_PATCH}
)
message(STATUS "Building libmamba v${${PROJECT_NAME}_VERSION}")

# Binary version See the following URL for explanations about the binary versioning
# https://www.gnu.org/software/libtool/manual/html_node/Updating-version-info.html#Updating-version-info
file(
    STRINGS "${LIBMAMBA_INCLUDE_DIR}/mamba/version.hpp" libmamba_version_defines
    REGEX "#define LIBMAMBA_BINARY_(CURRENT|REVISION|AGE)"
)
foreach(ver ${libmamba_version_defines})
    if(ver MATCHES "#define LIBMAMBA_BINARY_(CURRENT|REVISION|AGE) +([^ ]+)$")
        set(
            LIBMAMBA_BINARY_${CMAKE_MATCH_1}
            "${CMAKE_MATCH_2}"
            CACHE INTERNAL ""
        )
    endif()
endforeach()
set(
    LIBMAMBA_BINARY_VERSION
    ${LIBMAMBA_BINARY_CURRENT}.${LIBMAMBA_BINARY_REVISION}.${LIBMAMBA_BINARY_AGE}
)
message(STATUS "libmamba binary version: v${LIBMAMBA_BINARY_VERSION}")

# Build options
# =============

set(
    BUILD_LOG_LEVEL
    "TRACE"
    CACHE STRING "Logger active level at compile time"
)

if(NOT ${BUILD_LOG_LEVEL} MATCHES "^(TRACE|DEBUG|INFO|WARN|ERROR|CRITICAL|OFF)$")
    message(
        FATAL_ERROR
            "Invalid log level: ${BUILD_LOG_LEVEL}, should be one of { TRACE, DEBUG, INFO, WARN, ERROR, CRITICAL, OFF }"
    )
endif()

# TODO: move this into the mamba_create_target macro
if(BUILD_STATIC)
    add_definitions(-DLIBMAMBA_STATIC_DEPS)
endif()

if(WIN32)
    set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif()

option(ENABLE_ASAN "Enable Address-Sanitizer (currently only supported on GCC and Clang)" OFF)

if(ENABLE_ASAN)
    message(
        WARNING
            "Address-Sanitizer instrumentation will be injected into binaries - do not release these binaries"
    )
    add_compile_options(-fno-omit-frame-pointer -fsanitize=address)
    add_link_options(-fno-omit-frame-pointer -fsanitize=address)
endif()

if(ENABLE_TSAN)
    message(
        WARNING
            "Thread-Sanitizer instrumentation will be injected into binaries - do not release these binaries"
    )
    add_compile_options(-fno-omit-frame-pointer -fsanitize=thread)
    add_link_options(-fno-omit-frame-pointer -fsanitize=thread)
endif()

if(ENABLE_USAN)
    message(
        WARNING
            "Undefined-Sanitizer instrumentation will be injected into binaries - do not release these binaries"
    )
    add_compile_options(-fno-omit-frame-pointer -fsanitize=undefined)
    add_link_options(-fno-omit-frame-pointer -fsanitize=undefined)
endif()

# Source files
# ============

find_package(Python3 COMPONENTS Interpreter)

set(
    SHELL_SCRIPTS
    mamba.sh
    mamba.csh
    mamba.bat
    activate.bat
    _mamba_activate.bat
    mamba_hook.bat
    mamba_hook.ps1
    Mamba.psm1
    mamba.xsh
    mamba.fish
    compile_pyc.py
    mamba_completion.posix
)

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/shell_scripts)
foreach(script ${SHELL_SCRIPTS})
    string(REPLACE "." "_" script_var ${script})
    add_custom_command(
        OUTPUT shell_scripts/${script}.cpp
        DEPENDS data/${script}
        COMMAND
            ${Python3_EXECUTABLE} ${LIBMAMBA_DATA_DIR}/bin2header.py --extern -v data_${script_var}
            -i ${CMAKE_CURRENT_SOURCE_DIR}/data/${script} -o
            ${CMAKE_CURRENT_BINARY_DIR}/shell_scripts/${script}.cpp
    )
endforeach()

set(
    LIBMAMBA_SOURCES
    longpath.manifest
    ${LIBMAMBA_SOURCE_DIR}/version.cpp
    # Filesystem library
    ${LIBMAMBA_SOURCE_DIR}/fs/filesystem.cpp
    # C++ utility library
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
    ${LIBMAMBA_SOURCE_DIR}/util/url.cpp
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
    # Solver generic interface
    ${LIBMAMBA_SOURCE_DIR}/solver/helpers.cpp
    ${LIBMAMBA_SOURCE_DIR}/solver/problems_graph.cpp
    # Solver libsolv implementation
    ${LIBMAMBA_SOURCE_DIR}/solver/libsolv/database.cpp
    ${LIBMAMBA_SOURCE_DIR}/solver/libsolv/helpers.cpp
    ${LIBMAMBA_SOURCE_DIR}/solver/libsolv/matcher.cpp
    ${LIBMAMBA_SOURCE_DIR}/solver/libsolv/parameters.cpp
    ${LIBMAMBA_SOURCE_DIR}/solver/libsolv/repo_info.cpp
    ${LIBMAMBA_SOURCE_DIR}/solver/libsolv/solver.cpp
    ${LIBMAMBA_SOURCE_DIR}/solver/libsolv/unsolvable.cpp
    # Artifacts validation
    ${LIBMAMBA_SOURCE_DIR}/validation/errors.cpp
    ${LIBMAMBA_SOURCE_DIR}/validation/keys.cpp
    ${LIBMAMBA_SOURCE_DIR}/validation/repo_checker.cpp
    ${LIBMAMBA_SOURCE_DIR}/validation/tools.cpp
    ${LIBMAMBA_SOURCE_DIR}/validation/update_framework_v0_6.cpp
    ${LIBMAMBA_SOURCE_DIR}/validation/update_framework_v1.cpp
    ${LIBMAMBA_SOURCE_DIR}/validation/update_framework.cpp
    # Downloaders and mirrors
    ${LIBMAMBA_SOURCE_DIR}/download/compression.cpp
    ${LIBMAMBA_SOURCE_DIR}/download/compression.hpp
    ${LIBMAMBA_SOURCE_DIR}/download/curl.cpp
    ${LIBMAMBA_SOURCE_DIR}/download/curl.hpp
    ${LIBMAMBA_SOURCE_DIR}/download/downloader_impl.hpp
    ${LIBMAMBA_SOURCE_DIR}/download/downloader.cpp
    ${LIBMAMBA_SOURCE_DIR}/download/mirror_impl.cpp
    ${LIBMAMBA_SOURCE_DIR}/download/mirror_impl.hpp
    ${LIBMAMBA_SOURCE_DIR}/download/mirror_map.cpp
    ${LIBMAMBA_SOURCE_DIR}/download/mirror.cpp
    ${LIBMAMBA_SOURCE_DIR}/download/request.cpp
    # Core API (low-level)
    ${LIBMAMBA_SOURCE_DIR}/core/activation.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/channel_context.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/context.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/download_progress_bar.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/env_lockfile.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/environments_manager.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/error_handling.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/execution.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/fsutil.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/history.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/link.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/menuinst.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/output.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/package_cache.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/package_database_loader.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/package_fetcher.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/package_handling.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/package_paths.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/pinning.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/prefix_data.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/progress_bar_impl.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/progress_bar.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/query.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/repo_checker_store.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/run.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/shell_init.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/singletons.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/subdirdata.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/thread_utils.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/timeref.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/transaction_context.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/transaction.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/util_os.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/util.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/virtual_packages.cpp
    # API (high-level)
    ${LIBMAMBA_SOURCE_DIR}/api/c_api.cpp
    ${LIBMAMBA_SOURCE_DIR}/api/channel_loader.cpp
    ${LIBMAMBA_SOURCE_DIR}/api/clean.cpp
    ${LIBMAMBA_SOURCE_DIR}/api/config.cpp
    ${LIBMAMBA_SOURCE_DIR}/api/configuration.cpp
    ${LIBMAMBA_SOURCE_DIR}/api/create.cpp
    ${LIBMAMBA_SOURCE_DIR}/api/info.cpp
    ${LIBMAMBA_SOURCE_DIR}/api/install.cpp
    ${LIBMAMBA_SOURCE_DIR}/api/list.cpp
    ${LIBMAMBA_SOURCE_DIR}/api/utils.cpp
    ${LIBMAMBA_SOURCE_DIR}/api/utils.hpp
    ${LIBMAMBA_SOURCE_DIR}/api/remove.cpp
    ${LIBMAMBA_SOURCE_DIR}/api/repoquery.cpp
    ${LIBMAMBA_SOURCE_DIR}/api/shell.cpp
    ${LIBMAMBA_SOURCE_DIR}/api/update.cpp
)
# TODO: remove when switch to C++20
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
    # This file uses capturing structured bindings, which was fixed in C++20
    set_source_files_properties(
        ${LIBMAMBA_SOURCE_DIR}/download/mirror_impl.cpp
        PROPERTIES COMPILE_FLAGS -Wno-c++20-extensions
    )
endif()

foreach(script ${SHELL_SCRIPTS})
    list(APPEND LIBMAMBA_SOURCES shell_scripts/${script}.cpp)
endforeach()

set(
    LIBMAMBA_PUBLIC_HEADERS
    ${LIBMAMBA_INCLUDE_DIR}/mamba/version.hpp
    # Filesystem library
    ${LIBMAMBA_INCLUDE_DIR}/mamba/fs/filesystem.hpp
    # Utility library
    ${LIBMAMBA_INCLUDE_DIR}/mamba/util/build.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/util/cast.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/util/cfile.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/util/compare.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/util/conditional.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/util/cryptography.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/util/deprecation.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/util/encoding.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/util/environment.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/util/flat_binary_tree.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/util/flat_bool_expr_tree.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/util/flat_set.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/util/functional.hpp
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
    ${LIBMAMBA_INCLUDE_DIR}/mamba/util/tuple_hash.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/util/type_traits.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/util/url_manip.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/util/url.hpp
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
    # Solver generic interface
    ${LIBMAMBA_INCLUDE_DIR}/mamba/solver/problems_graph.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/solver/request.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/solver/solution.hpp
    # Solver libsolv implementation
    ${LIBMAMBA_INCLUDE_DIR}/mamba/solver/libsolv/database.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/solver/libsolv/parameters.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/solver/libsolv/repo_info.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/solver/libsolv/solver.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/solver/libsolv/unsolvable.hpp
    # Artifacts validation
    ${LIBMAMBA_INCLUDE_DIR}/mamba/validation/errors.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/validation/keys.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/validation/repo_checker.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/validation/tools.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/validation/update_framework_v0_6.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/validation/update_framework_v1.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/validation/update_framework.hpp
    # Downloaders and mirrors
    ${LIBMAMBA_INCLUDE_DIR}/mamba/download/downloader.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/download/mirror_map.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/download/mirror.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/download/request.hpp
    # Core API (low-level)
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/activation.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/channel_context.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/common_types.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/context.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/download_progress_bar.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/env_lockfile.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/environments_manager.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/error_handling.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/execution.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/fsutil.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/history.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/invoke.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/link.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/menuinst.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/output.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/package_cache.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/package_database_loader.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/package_fetcher.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/package_handling.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/package_paths.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/palette.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/pinning.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/prefix_data.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/progress_bar.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/query.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/repo_checker_store.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/run.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/shell_init.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/subdirdata.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/tasksync.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/thread_utils.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/timeref.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/transaction_context.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/transaction.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/util_os.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/util_scope.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/util.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/virtual_packages.hpp
    # API (high-level)
    ${LIBMAMBA_INCLUDE_DIR}/mamba/api/c_api.h
    ${LIBMAMBA_INCLUDE_DIR}/mamba/api/channel_loader.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/api/clean.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/api/config.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/api/configuration_impl.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/api/configuration.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/api/constants.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/api/create.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/api/info.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/api/install.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/api/list.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/api/remove.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/api/repoquery.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/api/shell.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/api/update.hpp
)

# Targets and link
# ================

find_package(fmt CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(tl-expected CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(simdjson CONFIG REQUIRED)
find_package(yaml-cpp CONFIG REQUIRED)
find_package(reproc CONFIG REQUIRED)
find_package(reproc++ CONFIG REQUIRED)
find_package(Libsolv MODULE REQUIRED)
add_subdirectory(ext/solv-cpp)

macro(libmamba_create_target target_name linkage output_name)
    string(TOUPPER "${linkage}" linkage_upper)
    if(NOT ${linkage_upper} MATCHES "^(SHARED|STATIC)$")
        message(FATAL_ERROR "Invalid library linkage: ${linkage}")
    endif()

    # Output
    # ======
    add_library(${target_name} ${linkage_upper} ${LIBMAMBA_PUBLIC_HEADERS} ${LIBMAMBA_SOURCES})

    target_include_directories(
        ${target_name}
        PUBLIC $<BUILD_INTERFACE:${LIBMAMBA_INCLUDE_DIR}> $<INSTALL_INTERFACE:include>
        PRIVATE ${LIBMAMBA_SOURCE_DIR}
    )

    # Header only libraries are always linked the same way
    target_link_libraries(${target_name} PUBLIC tl::expected nlohmann_json::nlohmann_json)

    target_compile_features(${target_name} PUBLIC cxx_std_17)

    mamba_target_add_compile_warnings(${target_name} WARNING_AS_ERROR ${MAMBA_WARNING_AS_ERROR})
    mamba_target_set_lto(${target_name} MODE ${MAMBA_LTO})

    # For some reasons, using target_compile_definitions does not set the definitions properly
    add_compile_definitions(
        SPDLOG_FMT_EXTERNAL "SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_${BUILD_LOG_LEVEL}"
    )

    if(${linkage_upper} STREQUAL "STATIC")
        message("   -> Statically linking against libmamba (static) dependencies")

        mamba_target_check_type(yaml-cpp::yaml-cpp STATIC_LIBRARY FATAL_ERROR)
        mamba_target_check_type(reproc STATIC_LIBRARY FATAL_ERROR)
        mamba_target_check_type(reproc++ STATIC_LIBRARY FATAL_ERROR)

        target_link_libraries(
            ${target_name}
            PUBLIC fmt::fmt-header-only spdlog::spdlog_header_only yaml-cpp::yaml-cpp
            PRIVATE
                reproc
                reproc++
                simdjson::simdjson_static
                solv::libsolv_static
                solv::libsolvext_static
                solv::cpp
        )

        if(UNIX)

            set(
                REQUIRED_STATIC_DEPS
                libcurl.a
                libssh2.a
                libgssapi_krb5.a
                libkrb5.a
                libk5crypto.a
                libkrb5support.a
                libcom_err.a
                libssl.a
                libcrypto.a
                libarchive.a
                libiconv.a
                libbz2.a
                liblz4.a
                libzstd.a
                libz.a
                liblzma.a
                libnghttp2.a
            )
            if(APPLE)
                set(REQUIRED_STATIC_DEPS ${REQUIRED_STATIC_DEPS} libc++.a)
            endif()

            if(UNIX AND NOT APPLE)
                list(REMOVE_ITEM REQUIRED_STATIC_DEPS libiconv.a)
            endif()

            set(STATIC_DEPS "")
            foreach(LIB ${REQUIRED_STATIC_DEPS})
                set(TMP_LIB "${LIB}-NOTFOUND")
                find_library(TMP_LIB NAMES "${LIB}")
                if(NOT ${TMP_LIB} STREQUAL "TMP_LIB-NOTFOUND")
                    list(APPEND STATIC_DEPS "${TMP_LIB}")
                else()
                    list(APPEND STATIC_DEPS "${LIB}-NOTFOUND")
                endif()
            endforeach(LIB)

            if(APPLE)
                find_library(SECURITY_LIBRARY Security)
                find_library(SYSTEMCONFIGURATION_LIBRARY SystemConfiguration)
                find_library(COREFOUNDATION_LIBRARY CoreFoundation)
                message("Found library: ${SECURITY_LIBRARY}\n${COREFOUNDATION_LIBRARY}")
                list(
                    APPEND
                    STATIC_DEPS
                    ${COREFOUNDATION_LIBRARY}
                    ${SECURITY_LIBRARY}
                    ${SYSTEMCONFIGURATION_LIBRARY}
                )
            endif()

            message("   -> Found static dependencies:")
            foreach(LIB ${STATIC_DEPS})
                message("      - ${LIB}")
            endforeach(LIB)

            if(APPLE)
                set(MAMBA_FORCE_DYNAMIC_LIBS resolv c++abi)
                target_link_options(${target_name} PRIVATE -nostdlib++)
            elseif(UNIX)
                set(MAMBA_FORCE_DYNAMIC_LIBS rt dl resolv)
                target_link_options(${target_name} PUBLIC -static-libstdc++ -static-libgcc)
            endif()

            target_link_libraries(${target_name} PUBLIC ${STATIC_DEPS} ${MAMBA_FORCE_DYNAMIC_LIBS})

        elseif(WIN32)

            set(CMAKE_PREFIX_PATH "$ENV{VCPKG_ROOT}/installed/x64-windows-static-md/")

            # For Windows we have a vcpkg based build system right now.
            find_package(LibArchive MODULE REQUIRED)
            find_package(CURL CONFIG REQUIRED)
            find_library(LIBLZMA_LIBRARIES lzma REQUIRED)
            find_library(LZ4_LIBRARY NAMES lz4)
            find_library(LZO2_LIBRARY NAMES lzo2)
            find_package(zstd CONFIG REQUIRED)
            find_library(BZIP2_LIBRARIES NAMES bz2)
            find_library(CRYPTO_LIBRARIES NAMES libcrypto)

            find_library(LIBXML2_LIBRARY NAMES libxml2)
            find_library(ICONV_LIBRARY NAMES libiconv iconv)
            find_library(CHARSET_LIBRARY NAMES libcharset charset)
            message("Found: ${LIBXML2_LIBRARY} ${ICONV_LIBRARY} ${CHARSET_LIBRARY}")

            target_link_libraries(
                ${target_name}
                PUBLIC
                    ${CRYPTO_LIBRARIES}
                    ${LibArchive_LIBRARY}
                    ${LIBXML2_LIBRARY}
                    ${ICONV_LIBRARY}
                    ${CHARSET_LIBRARY}
                    zstd::libzstd_static
                    ${LZ4_LIBRARY}
                    ${LZO2_LIBRARY}
                    ${BZIP2_LIBRARIES}
                    ${LIBLZMA_LIBRARIES}
                    CURL::libcurl
                    ${sodium_LIBRARY_RELEASE}
            )

            add_compile_definitions(LIBARCHIVE_STATIC CURL_STATICLIB)
            include_directories($ENV{CONDA_PREFIX}/Library/include/)
            include_directories($ENV{VCPKG_ROOT}/installed/x64-windows-static-md/include/)
        endif()
    else()
        message("   -> Dynamically linking against libmamba (shared) dependencies")

        mamba_target_check_type(yaml-cpp::yaml-cpp SHARED_LIBRARY WARNING)

        find_package(CURL REQUIRED)
        find_package(LibArchive REQUIRED)
        find_package(zstd REQUIRED)
        find_package(BZip2 REQUIRED)
        find_package(OpenSSL REQUIRED)

        target_link_libraries(
            ${target_name}
            PUBLIC
                ${LIBSOLV_LIBRARIES}
                ${LIBSOLVEXT_LIBRARIES}
                yaml-cpp::yaml-cpp
                fmt::fmt
                # Since conda-forge spdlog is built with a bundled version of fmt we use the header
                # only version to avoid chasing after the correct fmt version matching the one used
                # in the bundle
                spdlog::spdlog_header_only
            PRIVATE
                ${LibArchive_LIBRARIES}
                ${CURL_LIBRARIES}
                ${OPENSSL_LIBRARIES}
                BZip2::BZip2
                reproc
                reproc++
                simdjson::simdjson
                zstd::libzstd_shared
                solv::libsolv
                solv::libsolvext
                solv::cpp
        )
        # CMake 3.17 provides a LibArchive::LibArchive target that could be used instead of
        # LIBRARIES/INCLUDE_DIRS
        target_include_directories(${target_name} PRIVATE "${LibArchive_INCLUDE_DIRS}")
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
                # PUBLIC_HEADER "${LIBMAMBA_PUBLIC_HEADERS}"
                COMPILE_DEFINITIONS "LIBMAMBA_EXPORTS"
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
                # PUBLIC_HEADER "${LIBMAMBA_PUBLIC_HEADERS}"
                COMPILE_DEFINITIONS "LIBMAMBA_EXPORTS"
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

    list(APPEND libmamba_targets ${target_name})
    add_library(mamba::${target_name} ALIAS ${target_name})
endmacro()

set(libmamba_targets "")

option(
    ENABLE_MAMBA_ROOT_PREFIX_FALLBACK
    "Enable mamba (shared) root prefix to be set to install prefix"
    ON
)

if(BUILD_SHARED)
    message(STATUS "Adding shared libmamba target")
    libmamba_create_target(libmamba-dyn SHARED libmamba)
    if(ENABLE_MAMBA_ROOT_PREFIX_FALLBACK)
        # Use mamba installation prefix to set root prefix (base)
        target_compile_definitions(libmamba-dyn PUBLIC MAMBA_USE_INSTALL_PREFIX_AS_BASE)
    endif()
endif()

if(BUILD_STATIC)
    message(STATUS "Adding static libmamba target")

    # On Windows, a static library should use a different output name to avoid the conflict with the
    # import library of a shared one.
    if(CMAKE_HOST_WIN32)
        libmamba_create_target(libmamba-static STATIC libmamba_static)
    else()
        libmamba_create_target(libmamba-static STATIC libmamba)
    endif()
endif()

if(BUILD_SHARED_LIBS AND BUILD_SHARED)
    add_library(mamba::libmamba ALIAS libmamba-dyn)
elseif(BUILD_STATIC)
    add_library(mamba::libmamba ALIAS libmamba-static)
elseif(BUILD_SHARED)
    add_library(mamba::libmamba ALIAS libmamba-dyn)
else()
    message(FATAL_ERROR "Select at least a build variant for libmamba")
endif()

# Tests
if(BUILD_LIBMAMBA_TESTS)
    add_subdirectory(tests)
endif()

# Installation
# ============

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(
    LIBMAMBA_CMAKECONFIG_INSTALL_DIR
    "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}"
    CACHE STRING "install path for libmambaConfig.cmake"
)

install(
    TARGETS ${libmamba_targets}
    EXPORT ${PROJECT_NAME}Targets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(
    DIRECTORY "${LIBMAMBA_INCLUDE_DIR}/"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    FILES_MATCHING
    PATTERN "*.hpp"
    PATTERN "*.h"
)

# Configure 'mambaConfig.cmake' for a build tree
set(MAMBA_CONFIG_CODE "####### Expanded from \@MAMBA_CONFIG_CODE\@ #######\n")
set(
    MAMBA_CONFIG_CODE
    "${MAMBA_CONFIG_CODE}set(CMAKE_MODULE_PATH \"${CMAKE_CURRENT_SOURCE_DIR}/cmake;\${CMAKE_MODULE_PATH}\")\n"
)
set(MAMBA_CONFIG_CODE "${MAMBA_CONFIG_CODE}##################################################")
configure_package_config_file(
    ${PROJECT_NAME}Config.cmake.in "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
    INSTALL_DESTINATION ${PROJECT_BINARY_DIR}
)

# Configure 'mambaConfig.cmake' for an install tree
set(MAMBA_CONFIG_CODE "")
configure_package_config_file(
    ${PROJECT_NAME}Config.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/${PROJECT_NAME}Config.cmake"
    INSTALL_DESTINATION ${LIBMAMBA_CMAKECONFIG_INSTALL_DIR}
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake
    VERSION ${LIBMAMBA_VERSION_MAJOR}.${LIBMAMBA_VERSION_MINOR}.${LIBMAMBA_VERSION_PATCH}
    COMPATIBILITY AnyNewerVersion
)
install(
    FILES ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/${PROJECT_NAME}Config.cmake
          ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake
    DESTINATION ${LIBMAMBA_CMAKECONFIG_INSTALL_DIR}
)

install(
    EXPORT ${PROJECT_NAME}Targets
    NAMESPACE mamba::
    DESTINATION ${LIBMAMBA_CMAKECONFIG_INSTALL_DIR}
    COMPONENT Mamba_Development
)

# This profile script is only needed for `libmamba` distributed as a shared library on Unix systems.
if(BUILD_SHARED AND UNIX)
    message(STATUS "Generating profile script for libmamba (i.e. `etc/profile.d/libmamba.sh`)")
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/etc/profile.d/libmamba.sh.in"
        "${CMAKE_CURRENT_BINARY_DIR}/etc/profile.d/libmamba.sh"
    )

    install(
        FILES ${CMAKE_CURRENT_BINARY_DIR}/etc/profile.d/libmamba.sh
        DESTINATION ${CMAKE_INSTALL_PREFIX}/etc/profile.d/
    )
endif()
