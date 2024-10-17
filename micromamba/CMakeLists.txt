# Copyright (c) 2019, QuantStack and Mamba Contributors
#
# Distributed under the terms of the BSD 3-Clause License.
#
# The full license is in the file LICENSE, distributed with this software.

cmake_minimum_required(VERSION 3.16)
cmake_policy(SET CMP0025 NEW) # Introduced in cmake 3.0
cmake_policy(SET CMP0077 NEW) # Introduced in cmake 3.13
project(micromamba)

# Source files
# ============

set(
    MICROMAMBA_SRCS
    longpath.manifest
    ${CMAKE_CURRENT_SOURCE_DIR}/src/activate.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/clean.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/config.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/completer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/constructor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/create.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/env.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/info.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/install.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/list.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/login.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/common_options.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/package.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/remove.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/repoquery.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/run.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/shell.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/umamba.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/update.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/version.cpp
)

set(
    MICROMAMBA_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/common_options.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/umamba.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/version.hpp
)

if(UNIX AND BUILD_MICROMAMBA_SERVER)
    list(APPEND MICROMAMBA_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/src/server.cpp)
    add_definitions(-DMICROMAMBA_SERVER)
endif()

# Targets and link
# ================

find_package(Threads REQUIRED)
find_package(reproc REQUIRED)
find_package(reproc++ REQUIRED)

# Conda's binary relocation can result in string changing which can result in errors like:
#
# "warning:  command substitution: ignored null byte in input"
#
# See: https://github.com/mamba-org/mamba/issues/1517
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fno-merge-constants")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-merge-constants")

if(APPLE)
    # Needed for cross-compilation See:
    # https://conda-forge.org/docs/maintainer/knowledge_base/#newer-c-features-with-old-sdk
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_LIBCPP_DISABLE_AVAILABILITY=1")

    # Dependency of libsolv-static missing in CMakeLists.txt
    set(
        frameworks_flags
        "-framework CoreFoundation -framework CoreServices -framework Security -framework Kerberos"
    )
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${frameworks_flags}")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${frameworks_flags}")
endif()

macro(mambaexe_create_target target_name linkage output_name)
    string(TOUPPER "${linkage}" linkage_upper)
    if(NOT ${linkage_upper} MATCHES "^(SHARED|STATIC)$")
        message(FATAL_ERROR "Invalid library linkage: ${linkage}")
    endif()

    # Output
    # ======
    add_executable(${target_name} ${MICROMAMBA_SRCS} ${MICROMAMBA_HEADERS})
    mamba_target_add_compile_warnings(${target_name} WARNING_AS_ERROR ${MAMBA_WARNING_AS_ERROR})
    mamba_target_set_lto(${target_name} MODE ${MAMBA_LTO})
    set_property(TARGET ${target_name} PROPERTY CXX_STANDARD 17)

    target_link_libraries(${target_name} PRIVATE Threads::Threads reproc reproc++)

    # Static build
    # ============
    if(${linkage_upper} STREQUAL "STATIC")
        if(NOT (TARGET mamba::libmamba-static))
            find_package(libmamba REQUIRED)
        endif()
        target_link_libraries(${target_name} PRIVATE mamba::libmamba-static)
        if(APPLE)
            target_link_options(${target_name} PRIVATE -nostdlib++)
        endif()
        # Dynamic build
        # =============
    else()
        if(NOT (TARGET mamba::libmamba-dyn))
            find_package(libmamba REQUIRED)
        endif()
        target_link_libraries(${target_name} PRIVATE mamba::libmamba-dyn)
    endif()

    list(APPEND mambaexe_targets ${target_name})
endmacro()

set(mambaexe_targets "")

if(BUILD_SHARED)
    message(STATUS "Adding executable mamba")
    mambaexe_create_target(mamba SHARED mamba)
endif()

if(BUILD_STATIC)
    message(STATUS "Adding executable micromamba")
    mambaexe_create_target(micromamba STATIC micromamba)
endif()

# Installation
# ============

install(TARGETS ${mambaexe_targets})
