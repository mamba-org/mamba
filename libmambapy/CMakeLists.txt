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
    src/libmambapy/bindings/longpath.manifest
    # Entry point to all submodules
    src/libmambapy/bindings/bindings.cpp
    # All bindings used to live in a global module
    src/libmambapy/bindings/legacy.cpp
    # Submodules
    src/libmambapy/bindings/utils.cpp
    src/libmambapy/bindings/specs.cpp
    src/libmambapy/bindings/solver.cpp
    src/libmambapy/bindings/solver_libsolv.cpp
)
# TODO: remove when `SubdirData::cache_path()` is removed
if(
    CMAKE_CXX_COMPILER_ID STREQUAL "Clang"
    OR CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang"
    OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU"
)
    # This file uses capturing structured bindings, which was fixed in C++20
    set_source_files_properties(
        src/libmambapy/bindings/legacy.cpp
        PROPERTIES COMPILE_FLAGS -Wno-error=deprecated-declarations
    )
endif()

target_include_directories(bindings PRIVATE src/libmambapy/bindings)

mamba_target_add_compile_warnings(bindings WARNING_AS_ERROR ${MAMBA_WARNING_AS_ERROR})

target_link_libraries(bindings PRIVATE pybind11::pybind11 ${libmamba_target})
target_compile_features(bindings PRIVATE cxx_std_17)

# Installation

if(SKBUILD)
    install(TARGETS bindings DESTINATION ${MAMBA_INSTALL_PYTHON_EXT_LIBDIR})
else()
    # WARNING: this default should probably not be used as it is but set externally by a proper
    # Python packager tool
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
