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

project(mamba-archive)

# Build options
# =============

set(MAMBA_ARCHIVE_LINKAGE "DYNAMIC" CACHE STRING "mamba-archive linkage against libraries")

string(TOUPPER "${MAMBA_ARCHIVE_LINKAGE}" linkage_upper)
if (NOT ${linkage_upper} MATCHES "^(DYNAMIC|STATIC|FULL_STATIC)$")
    message(FATAL_ERROR "Invalid executable linkage: ${MAMBA_ARCHIVE_LINKAGE}, should be one of { DYNAMIC, STATIC, FULL_STATIC }")
endif ()

message(STATUS "'mamba-archive' linkage: ${MAMBA_ARCHIVE_LINKAGE}")

# Source files
# ============

set(MAMBA_ARCHIVE_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/archive.cpp
)

set(MAMBA_ARCHIVE_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/archive.hpp
)

# Dependencies
# ============

if(NOT (TARGET libmamba OR TARGET libmamba-static OR TARGET libmamba-full-static))
    find_package(libmamba REQUIRED)
endif ()

# Build definition
# ================

add_executable(mamba-archive ${MAMBA_ARCHIVE_SRCS} ${MAMBA_ARCHIVE_HEADERS})

if (${MAMBA_ARCHIVE_LINKAGE} STREQUAL "FULL_STATIC")
    target_link_libraries(mamba-archive PRIVATE libmamba-full-static)
elseif (${MAMBA_ARCHIVE_LINKAGE} STREQUAL "STATIC")
    target_link_libraries(mamba-archive PRIVATE libmamba-static)
else ()
    target_link_libraries(mamba-archive PRIVATE libmamba)
endif ()

set_property(TARGET mamba-archive PROPERTY CXX_STANDARD 17)

install(TARGETS mamba-archive)
