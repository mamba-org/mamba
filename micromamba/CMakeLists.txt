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

project(micromamba)

# Build options
# =============

set(MICROMAMBA_LINKAGE "DYNAMIC" CACHE STRING "micromamba linkage against libraries")

# Source files
# ============

set(MICROMAMBA_SRCS
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

set(MICROMAMBA_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/common_options.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/umamba.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/version.hpp
)

add_executable(micromamba ${MICROMAMBA_SRCS} ${MICROMAMBA_HEADERS})

if(NOT (TARGET libmamba OR TARGET libmamba-static OR TARGET libmamba-full-static))
    find_package(libmamba REQUIRED)
endif ()

string(TOUPPER "${MICROMAMBA_LINKAGE}" linkage_upper)
if (NOT ${linkage_upper} MATCHES "^(DYNAMIC|STATIC|FULL_STATIC)$")
    message(FATAL_ERROR "Invalid executable linkage: ${MICROMAMBA_LINKAGE}, should be one of { DYNAMIC, STATIC, FULL_STATIC }")
endif ()

message(STATUS "Micromamba linkage: ${MICROMAMBA_LINKAGE}")

find_package(Threads REQUIRED)
target_link_libraries(micromamba PRIVATE Threads::Threads)

# Check spdlog C header for external fmt configuration.
find_file(SPDLOG_TWEAK_H NAMES spdlog/tweakme.h)
if(SPDLOG_TWEAK_H)
    file(READ ${SPDLOG_TWEAK_H} SPDLOG_TWEAK_H_STRING)
    if("${SPDLOG_TWEAK_H_STRING}" MATCHES "\n *#define *SPDLOG_FMT_EXTERNAL *")
        find_package(fmt REQUIRED)
        target_link_libraries(micromamba PRIVATE fmt::fmt)
    endif()
endif()

if (${MICROMAMBA_LINKAGE} STREQUAL "FULL_STATIC")
    target_link_libraries(micromamba PRIVATE libmamba-full-static)
    if (WIN32)
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT")
    endif ()
elseif (${MICROMAMBA_LINKAGE} STREQUAL "STATIC")
    target_link_libraries(micromamba PRIVATE libmamba-static)
else ()
    target_link_libraries(micromamba PRIVATE libmamba)
endif ()

find_package(powerloader REQUIRED)
target_link_libraries(micromamba PRIVATE libpowerloader)

set_property(TARGET micromamba PROPERTY CXX_STANDARD 17)

install(TARGETS micromamba)
