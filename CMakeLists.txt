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
#if(POLICY CMP0028)
#  cmake_policy(SET CMP0028 NEW)
#endif()

project(mamba)

# Build options
# =============

option(BUILD_MICROMAMBA "Build micromamba" OFF)
option(BUILD_LIBMAMBA "Build libmamba library" OFF)
option(BUILD_LIBMAMBAPY "Build libmamba Python bindings" OFF)
option(BUILD_LIBMAMBA_TESTS "Build libmamba C++ tests" OFF)
option(STATICALLY_LINK_DEPS "" OFF)

option(BUILD_SHARED "" OFF)
option(BUILD_STATIC "" OFF)

if (MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /DNOMINMAX")
    set(CMAKE_BUILD_TYPE Release)
    # add_definitions("-DUNICODE -D_UNICODE")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")
endif()

# Variants
# ========

if (BUILD_MICROMAMBA OR BUILD_LIBMAMBA_TESTS OR BUILD_LIBMAMBAPY)
    set(BUILD_LIBMAMBA ON)
    set(BUILD_STATIC ON)
endif()

# libmamba library and tests
if (BUILD_LIBMAMBA)
    if (BUILD_LIBMAMBA_TESTS)
        set(BUILD_TESTS ON)
    endif()

    add_subdirectory(libmamba)
endif()

# Python bindings of libmamba
if (BUILD_LIBMAMBAPY)
    add_subdirectory(libmambapy)
endif()

# micromamba
if (BUILD_MICROMAMBA)
    add_subdirectory(umamba)
endif()
