# Copyright (c) 2019, QuantStack and Mamba Contributors
#
# Distributed under the terms of the BSD 3-Clause License.
#
# The full license is in the file LICENSE, distributed with this software.
cmake_minimum_required(VERSION 3.18.2)

include("../cmake/CompilerWarnings.cmake")

project(libmambapy)

if(NOT TARGET mamba::libmamba)
    find_package(libmamba CONFIG REQUIRED)
    set(libmamba_target mamba::libmamba-dyn)
else()
    set(libmamba_target mamba::libmamba)
endif()

find_package(Python COMPONENTS Interpreter Development.Module)
find_package(pybind11 REQUIRED)

pybind11_add_module(
    bindings
    bindings/longpath.manifest
    # Entry point to all submodules
    bindings/bindings.cpp
    # All bindings used to live in a global module
    bindings/legacy.cpp
    # Submodules
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

target_link_libraries(bindings PRIVATE pybind11::pybind11 ${libmamba_target})
target_compile_features(bindings PUBLIC cxx_std_20)
set_target_properties(
    bindings
    PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
)

# Installation

if(SKBUILD)
    install(TARGETS bindings DESTINATION libmambapy)
else()
    # WARNING: this default should probably not be used for installation but only for local
    # development and testing. Proper installation should be controlled externally by a Python
    # packager tool

    # Build bindings in a self-contain libmambapy/ folder inside the build tree
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

    set(
        MAMBA_INSTALL_PYTHON_EXT_LIBDIR
        "lib"
        CACHE PATH "Installation directory for Python extension"
    )

    install(
        TARGETS bindings
        EXCLUDE_FROM_ALL
        COMPONENT Mamba_Python_Extension
        DESTINATION ${MAMBA_INSTALL_PYTHON_EXT_LIBDIR}
    )
endif()
