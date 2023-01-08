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

option(BUILD_LIBMAMBA "Build libmamba library" OFF)
option(BUILD_LIBMAMBAPY "Build libmamba Python bindings" OFF)
option(BUILD_LIBMAMBA_TESTS "Build libmamba C++ tests" OFF)
option(BUILD_MICROMAMBA "Build micromamba" OFF)
option(BUILD_MAMBA_PACKAGE "Build mamba package utility" OFF)

if (MSVC)
    # NOMINMAX : prevent tons of code to be included when having to `#include <windows.h>`
    # /EHsc : enable C++ exceptions (otherwise exceptions do not work)
    # /Zc:__cplusplus : makes sure `__cplusplus` is set to the current C++ version language. Otherwise it is always set to an incorrect wrong value.
    # /MP : enable multi-process build with MSBuild (it should be on by default but CMake generates the project files without the right params).
    # /W4 : warnings level 4 (default in visual studio projects and recommended minimum level).
    # /external:I $ENV{CONDA_PREFIX}: consider the conda env prefix libraries headers as "external" to this project.
    # /external:W0 : set the warning level to 1 for external libraries headers (severe warnings, default when unspecified) to only see important warnings coming from dependencies headers.
    string(REGEX REPLACE "/W[0-9]" "/W4" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /D_CRT_SECURE_NO_WARNINGS /DNOMINMAX /EHsc /Zc:__cplusplus /MP /experimental:external /external:I $ENV{CONDA_PREFIX} /external:W1")
    # Force release mode to avoid debug libraries to be linked
    set(CMAKE_BUILD_TYPE Release)
    # add_definitions("-DUNICODE -D_UNICODE")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")
endif()

# Variants
# ========

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
    add_subdirectory(micromamba)
endif()

# mamba package tarball utility
if (BUILD_MAMBA_PACKAGE)
    add_subdirectory(mamba_package)
endif()
