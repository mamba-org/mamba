# Copyright (c) 2019, QuantStack and Mamba Contributors
#
# Distributed under the terms of the BSD 3-Clause License.
#
# The full license is in the file LICENSE, distributed with this software.
cmake_minimum_required(VERSION 3.18.2)

include("../cmake/CompilerWarnings.cmake")

project(libmambapy)

# Find libmamba components
if(NOT TARGET mamba::libmamba-common-dyn AND NOT TARGET mamba::libmamba-common-static)
    find_package(libmamba CONFIG REQUIRED)
endif()

if(NOT TARGET mamba::libmamba-dyn-spdlog)
    find_package(libmamba-spdlog CONFIG REQUIRED)
endif()

find_package(Python COMPONENTS Interpreter Development.Module)
find_package(pybind11 REQUIRED)
find_package(msgpack-c REQUIRED)

# Set variables for component CMakeLists.txt files
set(LIBMAMBAPY_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/bindings)
set(LIBMAMBAPY_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR})

# Build component bindings
# ========================
# Build order: common → solver (mirrors libmamba component order)

# Include component CMakeLists.txt files
add_subdirectory(common)
add_subdirectory(solver)

# Determine linkage type from libmamba components
if(TARGET mamba::libmamba-common-dyn)
    set(BUILD_SHARED_BINDINGS ON)
    set(BUILD_STATIC_BINDINGS OFF)
elseif(TARGET mamba::libmamba-common-static)
    set(BUILD_SHARED_BINDINGS OFF)
    set(BUILD_STATIC_BINDINGS ON)
else()
    message(FATAL_ERROR "Neither libmamba-common-dyn nor libmamba-common-static found")
endif()

# Create component binding targets
if(BUILD_SHARED_BINDINGS)
    libmambapy_common_create_target(libmambapy-common SHARED)
    libmambapy_solver_create_target(libmambapy-solver SHARED)
endif()

if(BUILD_STATIC_BINDINGS)
    libmambapy_common_create_target(libmambapy-common-static STATIC)
    libmambapy_solver_create_target(libmambapy-solver-static STATIC)
endif()

# Create aggregated bindings module for backward compatibility
# ============================================================
# This module includes legacy.cpp and re-exports component modules

# Determine which libmamba target to use for aggregated module
if(BUILD_SHARED_BINDINGS)
    if(TARGET mamba::libmamba-dyn)
        set(libmamba_target mamba::libmamba-dyn)
    else()
        # Fall back to component targets
        set(
            libmamba_target
            mamba::libmamba-common-dyn
            mamba::libmamba-solver-dyn
            mamba::libmamba-network-dyn
            mamba::libmamba-archive-dyn
        )
    endif()
else()
    if(TARGET mamba::libmamba-static)
        set(libmamba_target mamba::libmamba-static)
    else()
        # Fall back to component targets
        set(
            libmamba_target
            mamba::libmamba-common-static
            mamba::libmamba-solver-static
            mamba::libmamba-network-static
            mamba::libmamba-archive-static
        )
    endif()
endif()

# Create aggregated bindings module for backward compatibility This module includes all bindings and
# maintains the same API as before
pybind11_add_module(
    bindings
    bindings/longpath.manifest
    # Entry point to all submodules
    bindings/bindings.cpp
    # All bindings used to live in a global module
    bindings/legacy.cpp
    # Component bindings (included for backward compatibility) Note: Component modules are separate
    # optional modules
    bindings/utils.cpp
    bindings/specs.cpp
    bindings/solver.cpp
    bindings/solver_libsolv.cpp
)

# TODO: remove when `SubdirData::cache_path()` is removed
if(
    CMAKE_CXX_COMPILER_ID STREQUAL "Clang"
    OR CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang"
    OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU"
)
    # This file uses capturing structured bindings, which was fixed in C++20
    set_source_files_properties(
        bindings/legacy.cpp PROPERTIES COMPILE_FLAGS -Wno-error=deprecated-declarations
    )
endif()

target_include_directories(bindings PRIVATE bindings)

mamba_target_add_compile_warnings(bindings WARNING_AS_ERROR ${MAMBA_WARNING_AS_ERROR})

target_link_libraries(
    bindings PRIVATE pybind11::pybind11 ${libmamba_target} mamba::libmamba-dyn-spdlog msgpack-c
)
# Note: Component modules (libmambapy-common, libmambapy-solver) are separate Python extension
# modules and cannot be linked to the aggregated bindings module. They are optional and can be
# imported independently. The aggregated bindings module includes all source files for backward
# compatibility.

target_compile_features(bindings PUBLIC cxx_std_20)
set_target_properties(
    bindings
    PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
)

# Installation
# ============

if(SKBUILD)
    # Install all component modules and aggregated module
    install(TARGETS bindings DESTINATION libmambapy)
    if(BUILD_SHARED_BINDINGS)
        if(TARGET libmambapy-common)
            install(TARGETS libmambapy-common DESTINATION libmambapy)
        endif()
        if(TARGET libmambapy-solver)
            install(TARGETS libmambapy-solver DESTINATION libmambapy)
        endif()
    else()
        if(TARGET libmambapy-common-static)
            install(TARGETS libmambapy-common-static DESTINATION libmambapy)
        endif()
        if(TARGET libmambapy-solver-static)
            install(TARGETS libmambapy-solver-static DESTINATION libmambapy)
        endif()
    endif()
else()
    # WARNING: this default should probably not be used for installation but only for local
    # development and testing. Proper installation should be controlled externally by a Python
    # packager tool

    # Build bindings in a self-contained libmambapy/ folder inside the build tree
    set_target_properties(
        bindings PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/libmambapy"
    )

    # Copy all source files in the same libmambapy folder inside the build tree. This creates a
    # valid, development-only folder that can be imported by Python (e.g. via PYTHONPATH).
    file(GLOB_RECURSE MAMBAPY_FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/libmambapy/*")
    add_custom_command(
        OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/libmambapy/__init__.py"
        DEPENDS ${MAMBAPY_FILES}
        COMMENT "Copying libmambapy/ to build directory"
        COMMAND
            "${CMAKE_COMMAND}" -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/src/libmambapy/"
            "${CMAKE_CURRENT_BINARY_DIR}/libmambapy"
    )
    add_custom_target(
        libmambapy_copy_files ALL DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/libmambapy/__init__.py"
    )
    add_dependencies(bindings libmambapy_copy_files)

    # Also copy component module directories (if they exist) These are included in the main
    # copy_directory command above, so no separate commands needed

    # Set output directories for component modules to be in the same location as bindings
    if(BUILD_SHARED_BINDINGS)
        if(TARGET libmambapy-common)
            set_target_properties(
                libmambapy-common
                PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/libmambapy"
            )
            add_dependencies(libmambapy-common libmambapy_copy_files)
        endif()
        if(TARGET libmambapy-solver)
            set_target_properties(
                libmambapy-solver
                PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/libmambapy"
            )
            add_dependencies(libmambapy-solver libmambapy_copy_files)
        endif()
    else()
        if(TARGET libmambapy-common-static)
            set_target_properties(
                libmambapy-common-static
                PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/libmambapy"
            )
            add_dependencies(libmambapy-common-static libmambapy_copy_files)
        endif()
        if(TARGET libmambapy-solver-static)
            set_target_properties(
                libmambapy-solver-static
                PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/libmambapy"
            )
            add_dependencies(libmambapy-solver-static libmambapy_copy_files)
        endif()
    endif()

    set(
        MAMBA_INSTALL_PYTHON_EXT_LIBDIR
        "lib"
        CACHE PATH "Installation directory for Python extension"
    )

    # Install all targets
    install(
        TARGETS bindings
        EXCLUDE_FROM_ALL
        COMPONENT Mamba_Python_Extension
        DESTINATION ${MAMBA_INSTALL_PYTHON_EXT_LIBDIR}
    )

    if(BUILD_SHARED_BINDINGS)
        if(TARGET libmambapy-common)
            install(
                TARGETS libmambapy-common
                EXCLUDE_FROM_ALL
                COMPONENT Mamba_Python_Extension
                DESTINATION ${MAMBA_INSTALL_PYTHON_EXT_LIBDIR}
            )
        endif()
        if(TARGET libmambapy-solver)
            install(
                TARGETS libmambapy-solver
                EXCLUDE_FROM_ALL
                COMPONENT Mamba_Python_Extension
                DESTINATION ${MAMBA_INSTALL_PYTHON_EXT_LIBDIR}
            )
        endif()
    else()
        if(TARGET libmambapy-common-static)
            install(
                TARGETS libmambapy-common-static
                EXCLUDE_FROM_ALL
                COMPONENT Mamba_Python_Extension
                DESTINATION ${MAMBA_INSTALL_PYTHON_EXT_LIBDIR}
            )
        endif()
        if(TARGET libmambapy-solver-static)
            install(
                TARGETS libmambapy-solver-static
                EXCLUDE_FROM_ALL
                COMPONENT Mamba_Python_Extension
                DESTINATION ${MAMBA_INSTALL_PYTHON_EXT_LIBDIR}
            )
        endif()
    endif()
endif()
