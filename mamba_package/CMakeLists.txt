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

project(mamba-package)

# Build options
# =============

set(MAMBA_PACKAGE_LINKAGE "DYNAMIC" CACHE STRING "mamba-package linkage against libraries")

string(TOUPPER "${MAMBA_PACKAGE_LINKAGE}" linkage_upper)
if (NOT ${linkage_upper} MATCHES "^(DYNAMIC|STATIC|FULL_STATIC)$")
    message(FATAL_ERROR "Invalid executable linkage: ${MAMBA_PACKAGE_LINKAGE}, should be one of { DYNAMIC, STATIC, FULL_STATIC }")
endif ()

message(STATUS "'mamba-package' linkage: ${MAMBA_PACKAGE_LINKAGE}")

# Source files
# ============

set(MAMBA_PACKAGE_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/package.cpp
)

set(MAMBA_PACKAGE_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/package.hpp
)

# Dependencies
# ============

if(NOT (TARGET libmamba OR TARGET libmamba-static OR TARGET libmamba-full-static))
    find_package(libmamba REQUIRED)
endif ()

# Build definition
# ================

add_executable(mamba-package ${MAMBA_PACKAGE_SRCS} ${MAMBA_PACKAGE_HEADERS})

if (${MAMBA_PACKAGE_LINKAGE} STREQUAL "FULL_STATIC")
    target_link_libraries(mamba-package PRIVATE libmamba-full-static)
elseif (${MAMBA_PACKAGE_LINKAGE} STREQUAL "STATIC")
    target_link_libraries(mamba-package PRIVATE libmamba-static)
else ()
    target_link_libraries(mamba-package PRIVATE libmamba)
endif ()

set_property(TARGET mamba-package PROPERTY CXX_STANDARD 17)

install(TARGETS mamba-package)
