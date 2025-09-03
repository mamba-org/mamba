# Copyright (c) 2019, QuantStack and Mamba Contributors
#
# Distributed under the terms of the BSD 3-Clause License.
#
# The full license is in the file LICENSE, distributed with this software.

cmake_minimum_required(VERSION 3.16...4.1)
cmake_policy(SET CMP0025 NEW) # Introduced in cmake 3.0
cmake_policy(SET CMP0077 NEW) # Introduced in cmake 3.13
cmake_policy(SET CMP0192 NEW) # Introduced in cmake 4.1
project(micromamba)

include(GNUInstallDirs)

# Source files
# ============

set(
    MICROMAMBA_SRCS
    longpath.manifest
    ${CMAKE_CURRENT_SOURCE_DIR}/src/activate.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/clean.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/common_options.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/completer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/config.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/constructor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/create.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/env.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/info.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/install.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/list.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/login.cpp
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
    ${CMAKE_CURRENT_SOURCE_DIR}/src/constructor.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/umamba.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/version.hpp
)

# Targets and link
# ================

find_package(Threads REQUIRED)
find_package(reproc REQUIRED)
find_package(reproc++ REQUIRED)

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
    target_compile_features(${target_name} PUBLIC cxx_std_20)
    set_target_properties(
        ${target_name}
        PROPERTIES
            CXX_STANDARD 20
            CXX_STANDARD_REQUIRED YES
            CXX_EXTENSIONS NO
    )

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
    target_compile_definitions(micromamba PRIVATE BUILDING_MICROMAMBA)
endif()

# Installation
# ============

# This profile script is only needed for mamba on Unix systems.
if(BUILD_SHARED AND UNIX)
    message(STATUS "Generating profile script for mamba (i.e. `etc/profile.d/mamba.sh`)")
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/etc/profile.d/mamba.sh.in"
        "${CMAKE_CURRENT_BINARY_DIR}/etc/profile.d/mamba.sh"
    )

    install(
        FILES ${CMAKE_CURRENT_BINARY_DIR}/etc/profile.d/mamba.sh
        DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/profile.d/
    )
endif()

install(
    TARGETS ${mambaexe_targets}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
