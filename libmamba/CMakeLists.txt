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
# Store the binary directory for use in component CMakeLists.txt files
set(LIBMAMBA_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR})

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

option(ENABLE_ASAN "Enable AddressSanitizer (currently only supported on GCC and Clang)" OFF)
if(ENABLE_ASAN)
    message(
        WARNING
            "AddressSanitizer instrumentation will be injected into binaries - do not release these binaries"
    )
    add_compile_options(-fno-omit-frame-pointer -fsanitize=address)
    add_link_options(-fno-omit-frame-pointer -fsanitize=address)
endif()

option(ENABLE_TSAN "Enable ThreadSanitizer (currently only supported on GCC and Clang)" OFF)
if(ENABLE_TSAN)
    message(
        WARNING
            "ThreadSanitizer instrumentation will be injected into binaries - do not release these binaries"
    )
    add_compile_options(-fno-omit-frame-pointer -fsanitize=thread)
    add_link_options(-fno-omit-frame-pointer -fsanitize=thread)
endif()

option(
    ENABLE_UBSAN
    "Enable UndefinedBehaviorSanitizer (currently only supported on GCC and Clang)"
    OFF
)
if(ENABLE_UBSAN)
    message(
        WARNING
            "UndefinedBehaviorSanitizer instrumentation will be injected into binaries - do not release these binaries"
    )
    add_compile_options(-fno-omit-frame-pointer -fsanitize=undefined)
    add_link_options(-fno-omit-frame-pointer -fsanitize=undefined)
endif()

# Source files
# ============

find_package(Python3 COMPONENTS Interpreter)

# Shell scripts need to be defined before including component CMakeLists.txt files so they can be
# added to the common component sources
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

# Generate shell script .cpp files before including component CMakeLists.txt so the components can
# reference them
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/shell_scripts)
foreach(script ${SHELL_SCRIPTS})
    string(REPLACE "." "_" script_var ${script})
    # Use absolute path for OUTPUT to ensure consistent path resolution when referenced from
    # component CMakeLists.txt files
    add_custom_command(
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/shell_scripts/${script}.cpp
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/data/${script}
        COMMAND
            ${Python3_EXECUTABLE} ${LIBMAMBA_DATA_DIR}/bin2header.py --extern -v data_${script_var}
            -i ${CMAKE_CURRENT_SOURCE_DIR}/data/${script} -o
            ${CMAKE_CURRENT_BINARY_DIR}/shell_scripts/${script}.cpp
    )
endforeach()

# Include component CMakeLists.txt files
# =======================================
include(${CMAKE_CURRENT_SOURCE_DIR}/common/CMakeLists.txt)
include(${CMAKE_CURRENT_SOURCE_DIR}/solver/CMakeLists.txt)
include(${CMAKE_CURRENT_SOURCE_DIR}/network/CMakeLists.txt)
include(${CMAKE_CURRENT_SOURCE_DIR}/archive/CMakeLists.txt)

# Source files are now organized in component CMakeLists.txt files Legacy LIBMAMBA_SOURCES kept for
# API files that depend on all components
set(
    LIBMAMBA_API_SOURCES
    longpath.manifest
    # API (high-level) - depends on all components
    ${LIBMAMBA_SOURCE_DIR}/api/c_api.cpp
    ${LIBMAMBA_SOURCE_DIR}/api/channel_loader.cpp
    ${LIBMAMBA_SOURCE_DIR}/api/clean.cpp
    ${LIBMAMBA_SOURCE_DIR}/api/config.cpp
    ${LIBMAMBA_SOURCE_DIR}/api/configuration.cpp
    ${LIBMAMBA_SOURCE_DIR}/api/create.cpp
    ${LIBMAMBA_SOURCE_DIR}/api/env.cpp
    ${LIBMAMBA_SOURCE_DIR}/api/info.cpp
    ${LIBMAMBA_SOURCE_DIR}/api/install.cpp
    ${LIBMAMBA_SOURCE_DIR}/api/list.cpp
    ${LIBMAMBA_SOURCE_DIR}/api/utils.cpp
    ${LIBMAMBA_SOURCE_DIR}/api/utils.hpp
    ${LIBMAMBA_SOURCE_DIR}/api/remove.cpp
    ${LIBMAMBA_SOURCE_DIR}/api/repoquery.cpp
    ${LIBMAMBA_SOURCE_DIR}/api/shell.cpp
    ${LIBMAMBA_SOURCE_DIR}/api/update.cpp
    # Core API files that depend on all components
    ${LIBMAMBA_SOURCE_DIR}/core/repo_checker_store.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/transaction.cpp
)
# TODO: remove when switch to C++20
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
    # This file uses capturing structured bindings, which was fixed in C++20
    set_source_files_properties(
        ${LIBMAMBA_SOURCE_DIR}/download/mirror_impl.cpp
        PROPERTIES COMPILE_FLAGS -Wno-c++20-extensions
    )
endif()

# Shell scripts are used by common component (shell_init.cpp, activation.cpp,
# transaction_context.cpp) They're added to common component's sources, not API sources

# Public headers are now organized in component CMakeLists.txt files This list is for API headers
# that are part of the aggregated libmamba target
set(
    LIBMAMBA_API_PUBLIC_HEADERS
    # API (high-level)
    ${LIBMAMBA_INCLUDE_DIR}/mamba/api/c_api.h
    ${LIBMAMBA_INCLUDE_DIR}/mamba/api/channel_loader.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/api/clean.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/api/config.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/api/configuration_impl.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/api/configuration.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/api/constants.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/api/create.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/api/env.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/api/info.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/api/install.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/api/list.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/api/remove.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/api/repoquery.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/api/shell.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/api/update.hpp
    # Core API files
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/repo_checker_store.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/transaction.hpp
)

# Targets and link
# ================

find_package(fmt CONFIG REQUIRED)
find_package(tl-expected CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(simdjson CONFIG REQUIRED)
find_package(yaml-cpp CONFIG REQUIRED)
find_package(reproc CONFIG REQUIRED)
find_package(reproc++ CONFIG REQUIRED)
find_package(Libsolv MODULE REQUIRED)
find_package(msgpack-c CONFIG REQUIRED)
add_subdirectory(ext/solv-cpp)

# Build components
# ================
# Build order: common -> network -> solver -> archive Note: common depends on network (Context has
# mirror_map), creating a circular dependency We handle this by building common first, then network,
# then linking common to network
if(BUILD_SHARED)
    libmamba_common_create_target(libmamba-common-dyn SHARED libmamba-common)
    libmamba_network_create_target(libmamba-network-dyn SHARED libmamba-network)
    libmamba_solver_create_target(libmamba-solver-dyn SHARED libmamba-solver)
    libmamba_archive_create_target(libmamba-archive-dyn SHARED libmamba-archive)
    # Link network to archive after archive is built (network uses MultiPackageCache and
    # PackageExtractTask) Note: Cannot link archive to network for shared libraries due to circular
    # dependency (archive needs network symbols, network needs archive symbols). Archive will use
    # undefined symbol flags to allow linking, and symbols will be resolved by the aggregated
    # target.
    if(TARGET libmamba-archive-dyn)
        target_link_libraries(libmamba-network-dyn PUBLIC mamba::libmamba-archive-dyn)
    endif()
    # Note: Cannot link common to network/solver for shared libraries due to circular dependency
    # (network depends on common, solver depends on common and network) However, we need to add
    # DT_NEEDED entries to libmamba-common.so so that when it's loaded at runtime, the dynamic
    # linker will also load libmamba-solver.so and libmamba-network.so to resolve undefined symbols.
    # We do this by setting LINK_LIBRARIES property directly after all components are built. This
    # adds the libraries to the link line without creating CMake dependency edges that would trigger
    # cycle detection. Note: This must be done after all component targets are created to avoid
    # generator expression evaluation issues during add_library.
endif()

# Add DT_NEEDED entries to libmamba-common-dyn after all components are built This ensures the
# dynamic linker loads solver and network libraries when common is loaded We use target_link_options
# with generator expressions to add the libraries to the link line without creating CMake dependency
# edges that would trigger cycle detection
if(BUILD_SHARED)
    if(
        TARGET libmamba-common-dyn
        AND TARGET libmamba-solver-dyn
        AND TARGET libmamba-network-dyn
    )
        # Use --no-as-needed to ensure libraries are added to DT_NEEDED even if symbols aren't
        # directly referenced at link time (since we're using --allow-shlib-undefined)
        target_link_options(
            libmamba-common-dyn
            PRIVATE
            -Wl,--no-as-needed
            $<TARGET_FILE:mamba::libmamba-solver-dyn>
            $<TARGET_FILE:mamba::libmamba-network-dyn>
            -Wl,--as-needed
        )
    endif()
endif()

if(BUILD_STATIC)
    if(CMAKE_HOST_WIN32)
        libmamba_common_create_target(libmamba-common-static STATIC libmamba_common_static)
        libmamba_network_create_target(libmamba-network-static STATIC libmamba_network_static)
        # Link common to network after network is built (circular dependency resolution) For static
        # libraries, PUBLIC is fine as CMake allows circular dependencies for static libs
        if(TARGET libmamba-network-static)
            target_link_libraries(libmamba-common-static PUBLIC libmamba-network-static)
        endif()
        libmamba_solver_create_target(libmamba-solver-static STATIC libmamba_solver_static)
        libmamba_archive_create_target(libmamba-archive-static STATIC libmamba_archive_static)
        # Link network to archive (network uses MultiPackageCache and PackageExtractTask)
        if(TARGET libmamba-archive-static)
            target_link_libraries(libmamba-network-static PUBLIC mamba::libmamba-archive-static)
        endif()
        # Link archive to network (archive uses download::Request and util::URL) For static
        # libraries, circular dependencies are allowed
        if(TARGET libmamba-network-static)
            target_link_libraries(libmamba-archive-static PUBLIC mamba::libmamba-network-static)
        endif()
    else()
        libmamba_common_create_target(libmamba-common-static STATIC libmamba-common)
        libmamba_network_create_target(libmamba-network-static STATIC libmamba-network)
        # Link common to network after network is built (circular dependency resolution) For static
        # libraries, PUBLIC is fine as CMake allows circular dependencies for static libs
        if(TARGET libmamba-network-static)
            target_link_libraries(libmamba-common-static PUBLIC libmamba-network-static)
        endif()
        libmamba_solver_create_target(libmamba-solver-static STATIC libmamba-solver)
        libmamba_archive_create_target(libmamba-archive-static STATIC libmamba-archive)
        # Link network to archive (network uses MultiPackageCache and PackageExtractTask)
        if(TARGET libmamba-archive-static)
            target_link_libraries(libmamba-network-static PUBLIC mamba::libmamba-archive-static)
        endif()
        # Link archive to network (archive uses download::Request and util::URL) For static
        # libraries, circular dependencies are allowed
        if(TARGET libmamba-network-static)
            target_link_libraries(libmamba-archive-static PUBLIC mamba::libmamba-network-static)
        endif()
    endif()
endif()

# Create aggregated libmamba target for backward compatibility
# ============================================================
# This target links all components together for backward compatibility
macro(libmamba_create_target target_name linkage output_name)
    string(TOUPPER "${linkage}" linkage_upper)
    if(NOT ${linkage_upper} MATCHES "^(SHARED|STATIC)$")
        message(FATAL_ERROR "Invalid library linkage: ${linkage}")
    endif()

    # Determine component target names
    if(${linkage_upper} STREQUAL "SHARED")
        set(common_target libmamba-common-dyn)
        set(solver_target libmamba-solver-dyn)
        set(network_target libmamba-network-dyn)
        set(archive_target libmamba-archive-dyn)
    else()
        set(common_target libmamba-common-static)
        set(solver_target libmamba-solver-static)
        set(network_target libmamba-network-static)
        set(archive_target libmamba-archive-static)
    endif()

    # Create a real library that aggregates all components and includes API sources API sources
    # depend on all components, so they need to be in this aggregated target
    add_library(
        ${target_name} ${linkage_upper} ${LIBMAMBA_API_PUBLIC_HEADERS} ${LIBMAMBA_API_SOURCES}
    )

    # Link all components
    target_link_libraries(
        ${target_name}
        PUBLIC
            mamba::${common_target}
            mamba::${solver_target}
            mamba::${network_target}
            mamba::${archive_target}
    )

    # Context uses get_root_prefix from API, so link API target to itself (get_root_prefix is
    # implemented in api/configuration.cpp which is in this target)

    target_include_directories(
        ${target_name}
        PUBLIC $<BUILD_INTERFACE:${LIBMAMBA_INCLUDE_DIR}> $<INSTALL_INTERFACE:include>
        PRIVATE ${LIBMAMBA_SOURCE_DIR}
    )

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

    if(UNIX)
        math(EXPR LIBMAMBA_BINARY_COMPATIBLE "${LIBMAMBA_BINARY_CURRENT} - ${LIBMAMBA_BINARY_AGE}")
        set_target_properties(
            ${target_name}
            PROPERTIES
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
                COMPILE_DEFINITIONS "LIBMAMBA_EXPORTS"
                PREFIX ""
                VERSION ${LIBMAMBA_BINARY_VERSION}
                SOVERSION ${LIBMAMBA_BINARY_CURRENT}
                OUTPUT_NAME "${output_name}"
        )
        target_compile_definitions(${target_name} PUBLIC GHC_WIN_DISABLE_WSTRING_STORAGE_TYPE)
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
    message(STATUS "Adding shared libmamba target (aggregated)")
    libmamba_create_target(libmamba-dyn SHARED libmamba)
    if(ENABLE_MAMBA_ROOT_PREFIX_FALLBACK)
        # Use mamba installation prefix to set root prefix (base)
        target_compile_definitions(libmamba-dyn PUBLIC MAMBA_USE_INSTALL_PREFIX_AS_BASE)
    endif()
endif()

if(BUILD_STATIC)
    message(STATUS "Adding static libmamba target (aggregated)")

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

# Install all component targets and aggregated target Component targets must be explicitly included
# in the export set so that CMake can resolve dependencies when exporting the aggregated target
set(all_libmamba_targets ${libmamba_targets})
if(BUILD_SHARED)
    list(
        APPEND
        all_libmamba_targets
        ${libmamba_common_targets}
        ${libmamba_solver_targets}
        ${libmamba_network_targets}
        ${libmamba_archive_targets}
    )
endif()
if(BUILD_STATIC)
    list(
        APPEND
        all_libmamba_targets
        ${libmamba_common_targets}
        ${libmamba_solver_targets}
        ${libmamba_network_targets}
        ${libmamba_archive_targets}
    )
endif()
# Remove duplicates to avoid CMake errors about targets being included more than once
list(REMOVE_DUPLICATES all_libmamba_targets)

install(
    TARGETS ${all_libmamba_targets}
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
    ${PROJECT_NAME}Config.cmake.in "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
    INSTALL_DESTINATION ${LIBMAMBA_CMAKECONFIG_INSTALL_DIR}
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake
    VERSION ${LIBMAMBA_VERSION_MAJOR}.${LIBMAMBA_VERSION_MINOR}.${LIBMAMBA_VERSION_PATCH}
    COMPATIBILITY AnyNewerVersion
)
install(
    FILES ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake
          ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake
    DESTINATION ${LIBMAMBA_CMAKECONFIG_INSTALL_DIR}
)

install(
    EXPORT ${PROJECT_NAME}Targets
    NAMESPACE mamba::
    DESTINATION ${LIBMAMBA_CMAKECONFIG_INSTALL_DIR}
    COMPONENT Mamba_Development
)
export(EXPORT ${PROJECT_NAME}Targets NAMESPACE mamba::)
