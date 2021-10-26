# Copyright (c) 2019, QuantStack and Mamba Contributors
#
# Distributed under the terms of the BSD 3-Clause License.
#
# The full license is in the file LICENSE, distributed with this software.

cmake_minimum_required (VERSION 3.2)
if(POLICY CMP0025)
  cmake_policy(SET CMP0025 NEW)
endif()
cmake_policy(SET CMP0077 NEW)

project(libmamba)

set(LIBMAMBA_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)
set(LIBMAMBA_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)

# Versionning
# ===========
file(STRINGS "${LIBMAMBA_INCLUDE_DIR}/mamba/version.hpp" libmamba_version_defines
     REGEX "#define LIBMAMBA_VERSION_(MAJOR|MINOR|PATCH)")
foreach(ver ${libmamba_version_defines})
    if(ver MATCHES "#define LIBMAMBA_VERSION_(MAJOR|MINOR|PATCH) +([^ ]+)$")
        set(LIBMAMBA_VERSION_${CMAKE_MATCH_1} "${CMAKE_MATCH_2}" CACHE INTERNAL "")
    endif()
endforeach()
set(${PROJECT_NAME}_VERSION
    ${LIBMAMBA_VERSION_MAJOR}.${LIBMAMBA_VERSION_MINOR}.${LIBMAMBA_VERSION_PATCH})
message(STATUS "Building libmamba v${${PROJECT_NAME}_VERSION}")


# Binary version
# See the following URL for explanations about the binary versionning
# https://www.gnu.org/software/libtool/manual/html_node/Updating-version-info.html#Updating-version-info
file(STRINGS "${LIBMAMBA_INCLUDE_DIR}/mamba/version.hpp" libmamba_version_defines
     REGEX "#define LIBMAMBA_BINARY_(CURRENT|REVISION|AGE)")
foreach(ver ${libmamba_version_defines})
    if(ver MATCHES "#define LIBMAMBA_BINARY_(CURRENT|REVISION|AGE) +([^ ]+)$")
        set(LIBMAMBA_BINARY_${CMAKE_MATCH_1} "${CMAKE_MATCH_2}" CACHE INTERNAL "")
    endif()
endforeach()
set(LIBMAMBA_BINARY_VERSION
    ${LIBMAMBA_BINARY_CURRENT}.${LIBMAMBA_BINARY_REVISION}.${LIBMAMBA_BINARY_AGE})
message(STATUS "libmamba binary version: v${LIBMAMBA_BINARY_VERSION}")


# Build options
# =============
option(BUILD_TESTS "Build libmamba C++ tests" OFF)
option(BUILD_SHARED "Build shared libmamba library" OFF)
option(BUILD_STATIC "Build static libmamba library" OFF)

option(MICROMAMBA_STATIC_LINK "" OFF)

# Source files
# ============

set(LIBMAMBA_SOURCES
    ${LIBMAMBA_SOURCE_DIR}/version.cpp
    # Core API (low-level)
    ${LIBMAMBA_SOURCE_DIR}/core/activation.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/channel.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/context.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/environments_manager.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/fetch.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/transaction_context.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/link.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/history.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/match_spec.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/menuinst.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/url.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/output.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/package_handling.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/package_cache.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/pool.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/prefix_data.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/progress_bar.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/pinning.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/package_info.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/package_paths.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/query.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/repo.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/shell_init.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/solver.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/subdirdata.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/thread_utils.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/transaction.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/util.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/util_os.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/validate.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/virtual_packages.cpp
    # API (high-level)
    ${LIBMAMBA_SOURCE_DIR}/api/c_api.cpp
    ${LIBMAMBA_SOURCE_DIR}/api/clean.cpp
    ${LIBMAMBA_SOURCE_DIR}/api/config.cpp
    ${LIBMAMBA_SOURCE_DIR}/api/configuration.cpp
    ${LIBMAMBA_SOURCE_DIR}/api/create.cpp
    ${LIBMAMBA_SOURCE_DIR}/api/info.cpp
    ${LIBMAMBA_SOURCE_DIR}/api/install.cpp
    ${LIBMAMBA_SOURCE_DIR}/api/list.cpp
    ${LIBMAMBA_SOURCE_DIR}/api/remove.cpp
    ${LIBMAMBA_SOURCE_DIR}/api/shell.cpp
    ${LIBMAMBA_SOURCE_DIR}/api/update.cpp
)

set(LIBMAMBA_THIRD_PARTIES
    ${LIBMAMBA_SOURCE_DIR}/thirdparty/WinReg.hpp
)

set(LIBMAMBA_HEADERS
    ${LIBMAMBA_INCLUDE_DIR}/mamba/version.hpp
    # Core API (low-level)
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/activation.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/channel.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/context.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/environment.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/environments_manager.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/fetch.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/fsutil.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/graph_util.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/history.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/link.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/mamba_fs.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/match_spec.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/menuinst.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/output.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/package_cache.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/package_handling.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/package_info.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/package_paths.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/pool.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/prefix_data.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/progress_bar.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/pinning.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/query.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/repo.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/shell_init.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/solver.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/subdirdata.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/thread_utils.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/transaction.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/transaction_context.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/url.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/util.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/util_os.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/validate.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/virtual_packages.hpp
    # API (high-level)
    ${LIBMAMBA_INCLUDE_DIR}/mamba/api/c_api.h
    ${LIBMAMBA_INCLUDE_DIR}/mamba/api/clean.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/api/config.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/api/configuration.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/api/constants.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/api/create.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/api/info.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/api/install.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/api/list.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/api/remove.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/api/shell.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/api/update.hpp
)

# Targets and link
# ================

macro(libmamba_create_target target_name linkage output_name)
    string(TOUPPER "${linkage}" linkage_upper)

    if (NOT ${linkage_upper} MATCHES "^(SHARED|STATIC)$")
        message(FATAL_ERROR "Invalid library linkage: ${linkage}")
    endif ()

    # Output
    # ======

    add_library(${target_name} ${linkage_upper} ${LIBMAMBA_SOURCES} ${LIBMAMBA_HEADERS} ${LIBMAMBA_THIRD_PARTIES})
    if (NOT MICROMAMBA_STATIC_LINK)
        find_library(LIBSOLV_LIBRARIES NAMES solv)
        find_library(LIBSOLVEXT_LIBRARIES NAMES solvext)
        find_package(CURL REQUIRED)
        find_package(LibArchive REQUIRED)
        find_package(OpenSSL REQUIRED)
        find_package(yaml-cpp CONFIG REQUIRED)
        find_package(reproc++ CONFIG REQUIRED)

        set(LIBMAMBA_LIBRARIES_DEPS
            ${LIBSOLV_LIBRARIES}
            ${LIBSOLVEXT_LIBRARIES}
            ${LibArchive_LIBRARIES}
            ${CURL_LIBRARIES}
            ${OPENSSL_LIBRARIES}
            ${YAML_CPP_LIBRARIES}
            reproc++
        )

        target_link_libraries(${target_name} PUBLIC
                              ${LIBMAMBA_LIBRARIES_DEPS}
                              ${MAMBA_FORCE_DYNAMIC_LIBS})
    endif()
    set_property(TARGET ${target_name} PROPERTY CXX_STANDARD 17)

    target_include_directories(
        ${target_name}
        PUBLIC
            $<BUILD_INTERFACE:${LIBMAMBA_INCLUDE_DIR}>
            $<INSTALL_INTERFACE:include>
    )

    # TODO: package WinReg header
    target_include_directories(
        ${target_name}
        PRIVATE ${LIBMAMBA_SOURCE_DIR}
    )

    if (UNIX)
        math(EXPR LIBMAMBA_BINARY_COMPATIBLE "${LIBMAMBA_BINARY_CURRENT} - ${LIBMAMBA_BINARY_AGE}")
        set_target_properties(
            ${target_name}
            PROPERTIES
            PUBLIC_HEADER "${LIBMAMBA_HEADERS}"
            COMPILE_DEFINITIONS "LIBMAMBA_EXPORTS"
            PREFIX ""
            VERSION "${LIBMAMBA_BINARY_COMPATIBLE}.${LIBMAMBA_BINARY_REVISION}.${LIBMAMBA_BINARY_AGE}"
            SOVERSION ${LIBMAMBA_BINARY_COMPATIBLE}
            OUTPUT_NAME "${output_name}"
        )
    else()
        set_target_properties(
            ${target_name}
            PROPERTIES
            PUBLIC_HEADER "${LIBMAMBA_HEADERS}"
            COMPILE_DEFINITIONS "LIBMAMBA_EXPORTS"
            PREFIX ""
            VERSION ${LIBMAMBA_BINARY_VERSION}
            SOVERSION ${LIBMAMBA_BINARY_CURRENT}
            OUTPUT_NAME "${output_name}"
        )
        target_compile_definitions(${target_name} PUBLIC GHC_WIN_DISABLE_WSTRING_STORAGE_TYPE)
    endif()
endmacro()


set(libmamba_targets "")

if (BUILD_SHARED)
    message(STATUS "Building shared library libmamba")
    libmamba_create_target(libmamba SHARED libmamba)
    list(APPEND libmamba_targets libmamba)
endif ()

if (BUILD_STATIC)
    message(STATUS "Building static library libmamba")
    # On Windows, a static library should use a different output name
    # to avoid the conflict with the import library of a shared one.
    if (CMAKE_HOST_WIN32)
        libmamba_create_target(libmamba-static STATIC libmamba_static)
    else ()
        libmamba_create_target(libmamba-static STATIC libmamba)
    endif ()

    find_package(nlohmann_json CONFIG REQUIRED)
    find_package(Threads REQUIRED)

    target_link_libraries(libmamba-static PUBLIC
                          nlohmann_json::nlohmann_json
                          Threads::Threads)

    list(APPEND libmamba_targets libmamba-static)
endif ()

if (NOT (BUILD_SHARED OR BUILD_STATIC))
    message(FATAL_ERROR "Select at least a build variant (shared/static) for libmamba")
endif ()

# Tests
if (BUILD_TESTS)
    add_subdirectory(tests)
endif()

# Installation
# ============

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(LIBMAMBA_CMAKECONFIG_INSTALL_DIR "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}" CACHE STRING "install path for libmambaConfig.cmake")

install(TARGETS ${libmamba_targets}
        EXPORT ${PROJECT_NAME}-targets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install(DIRECTORY "${LIBMAMBA_INCLUDE_DIR}/"
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
        FILES_MATCHING
        PATTERN "*.hpp"
        PATTERN "*.h")

# Makes the project importable from the build directory
export(EXPORT ${PROJECT_NAME}-targets
        FILE "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Targets.cmake")

# Configure 'mambaConfig.cmake' for a build tree
set(MAMBA_CONFIG_CODE "####### Expanded from \@MAMBA_CONFIG_CODE\@ #######\n")
set(MAMBA_CONFIG_CODE "${MAMBA_CONFIG_CODE}set(CMAKE_MODULE_PATH \"${CMAKE_CURRENT_SOURCE_DIR}/cmake;\${CMAKE_MODULE_PATH}\")\n")
set(MAMBA_CONFIG_CODE "${MAMBA_CONFIG_CODE}##################################################")
configure_package_config_file(${PROJECT_NAME}Config.cmake.in
                                "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
                                INSTALL_DESTINATION ${PROJECT_BINARY_DIR})

# Configure 'mambaConfig.cmake' for an install tree
set(MAMBA_CONFIG_CODE "")
configure_package_config_file(${PROJECT_NAME}Config.cmake.in
                                "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/${PROJECT_NAME}Config.cmake"
                                INSTALL_DESTINATION ${LIBMAMBA_CMAKECONFIG_INSTALL_DIR})


write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake
                                    VERSION ${MAMBA_VERSION_MAJOR}.${MAMBA_VERSION_MINOR}.${MAMBA_VERSION_PATCH}
                                    COMPATIBILITY AnyNewerVersion)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/${PROJECT_NAME}Config.cmake
                ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake
                DESTINATION ${LIBMAMBA_CMAKECONFIG_INSTALL_DIR})
install(EXPORT ${PROJECT_NAME}-targets
        FILE ${PROJECT_NAME}Targets.cmake
        DESTINATION ${LIBMAMBA_CMAKECONFIG_INSTALL_DIR})
