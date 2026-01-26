# Copyright (c) 2019, QuantStack and Mamba Contributors
#
# Distributed under the terms of the BSD 3-Clause License.
#
# The full license is in the file LICENSE, distributed with this software.

# Solver component bindings - solver and solver_libsolv These variables should be set by the parent
# CMakeLists.txt
if(NOT DEFINED LIBMAMBAPY_SOURCE_DIR)
    set(LIBMAMBAPY_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../bindings)
endif()
if(NOT DEFINED LIBMAMBAPY_BINARY_DIR)
    set(LIBMAMBAPY_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR})
endif()

# Capture the directory containing this CMakeLists.txt file Use CMAKE_CURRENT_LIST_DIR which is set
# when this file is processed
set(
    LIBMAMBAPY_SOLVER_DIR
    "${CMAKE_CURRENT_LIST_DIR}"
    CACHE INTERNAL "Directory containing solver CMakeLists.txt"
)

# Create the solver bindings target
macro(libmambapy_solver_create_target target_name linkage)
    string(TOUPPER "${linkage}" linkage_upper)
    if(NOT ${linkage_upper} MATCHES "^(SHARED|STATIC)$")
        message(FATAL_ERROR "Invalid library linkage: ${linkage}")
    endif()

    # Determine libmamba component target names
    if(${linkage_upper} STREQUAL "SHARED")
        set(common_target libmamba-common-dyn)
        set(solver_target libmamba-solver-dyn)
    else()
        set(common_target libmamba-common-static)
        set(solver_target libmamba-solver-static)
    endif()

    # Create pybind11 module for solver bindings This creates a separate Python extension module
    # (libmambapy_solver) that can be imported independently. The aggregated bindings module will
    # also include these bindings for backward compatibility.
    pybind11_add_module(
        ${target_name}
        ${LIBMAMBAPY_SOLVER_DIR}/bindings_solver.cpp
        ${LIBMAMBAPY_SOURCE_DIR}/solver.cpp
        ${LIBMAMBAPY_SOURCE_DIR}/solver_libsolv.cpp
    )

    target_include_directories(
        ${target_name} PRIVATE ${LIBMAMBAPY_SOURCE_DIR} ${LIBMAMBAPY_SOLVER_DIR}/..
    )

    mamba_target_add_compile_warnings(${target_name} WARNING_AS_ERROR ${MAMBA_WARNING_AS_ERROR})

    # Link to libmamba components (solver depends on common)
    if(TARGET mamba::${common_target} AND TARGET mamba::${solver_target})
        target_link_libraries(
            ${target_name} PRIVATE mamba::${common_target} mamba::${solver_target}
        )
    else()
        message(
            FATAL_ERROR "libmamba-common and libmamba-solver must be built before libmambapy-solver"
        )
    endif()

    target_link_libraries(${target_name} PRIVATE pybind11::pybind11)

    target_compile_features(${target_name} PUBLIC cxx_std_20)
    set_target_properties(
        ${target_name}
        PROPERTIES
            CXX_STANDARD 20
            CXX_STANDARD_REQUIRED YES
            CXX_EXTENSIONS NO
    )

    # Output directory will be set by parent CMakeLists.txt

    list(APPEND libmambapy_solver_targets ${target_name})
    add_library(mamba::${target_name} ALIAS ${target_name})
endmacro()

set(libmambapy_solver_targets "")
