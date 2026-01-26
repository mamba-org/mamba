# Copyright (c) 2019, QuantStack and Mamba Contributors
#
# Distributed under the terms of the BSD 3-Clause License.
#
# The full license is in the file LICENSE, distributed with this software.

# Solver component - depends on libmamba-common and libsolv

# These variables should be set by the parent CMakeLists.txt
if(NOT DEFINED LIBMAMBA_SOURCE_DIR)
    set(LIBMAMBA_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../src)
endif()
if(NOT DEFINED LIBMAMBA_INCLUDE_DIR)
    set(LIBMAMBA_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../include)
endif()

set(
    LIBMAMBA_SOLVER_SOURCES
    # Solver generic interface
    ${LIBMAMBA_SOURCE_DIR}/solver/helpers.cpp
    ${LIBMAMBA_SOURCE_DIR}/solver/problems_graph.cpp
    # Solver libsolv implementation
    ${LIBMAMBA_SOURCE_DIR}/solver/libsolv/database.cpp
    ${LIBMAMBA_SOURCE_DIR}/solver/libsolv/helpers.cpp
    ${LIBMAMBA_SOURCE_DIR}/solver/libsolv/matcher.cpp
    ${LIBMAMBA_SOURCE_DIR}/solver/libsolv/parameters.cpp
    ${LIBMAMBA_SOURCE_DIR}/solver/libsolv/repo_info.cpp
    ${LIBMAMBA_SOURCE_DIR}/solver/libsolv/solver.cpp
    ${LIBMAMBA_SOURCE_DIR}/solver/libsolv/unsolvable.cpp
    # Core files that depend on solver
    ${LIBMAMBA_SOURCE_DIR}/core/package_database_loader.cpp
)

set(
    LIBMAMBA_SOLVER_PUBLIC_HEADERS
    # Solver generic interface
    ${LIBMAMBA_INCLUDE_DIR}/mamba/solver/problems_graph.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/solver/request.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/solver/solution.hpp
    # Solver libsolv implementation
    ${LIBMAMBA_INCLUDE_DIR}/mamba/solver/libsolv/database.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/solver/libsolv/parameters.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/solver/libsolv/repo_info.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/solver/libsolv/solver.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/solver/libsolv/unsolvable.hpp
    # Core files that depend on solver
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/package_database_loader.hpp
)

# Create the solver library target
macro(libmamba_solver_create_target target_name linkage output_name)
    string(TOUPPER "${linkage}" linkage_upper)
    if(NOT ${linkage_upper} MATCHES "^(SHARED|STATIC)$")
        message(FATAL_ERROR "Invalid library linkage: ${linkage}")
    endif()

    add_library(
        ${target_name}
        ${linkage_upper} ${LIBMAMBA_SOLVER_PUBLIC_HEADERS} ${LIBMAMBA_SOLVER_SOURCES}
    )

    target_include_directories(
        ${target_name}
        PUBLIC $<BUILD_INTERFACE:${LIBMAMBA_INCLUDE_DIR}> $<INSTALL_INTERFACE:include>
        PRIVATE ${LIBMAMBA_SOURCE_DIR}
    )

    target_compile_features(${target_name} PUBLIC cxx_std_20)
    set_target_properties(
        ${target_name}
        PROPERTIES
            CXX_STANDARD 20
            CXX_STANDARD_REQUIRED YES
            CXX_EXTENSIONS NO
    )

    mamba_target_add_compile_warnings(${target_name} WARNING_AS_ERROR ${MAMBA_WARNING_AS_ERROR})
    mamba_target_set_lto(${target_name} MODE ${MAMBA_LTO})

    # Depend on common component
    if(TARGET mamba::libmamba-common-dyn)
        target_link_libraries(${target_name} PUBLIC mamba::libmamba-common-dyn)
    elseif(TARGET mamba::libmamba-common-static)
        target_link_libraries(${target_name} PUBLIC mamba::libmamba-common-static)
    else()
        message(FATAL_ERROR "libmamba-common must be built before libmamba-solver")
    endif()

    # Depend on network component (package_database_loader needs subdir_index)
    if(TARGET mamba::libmamba-network-dyn)
        target_link_libraries(${target_name} PUBLIC mamba::libmamba-network-dyn)
    elseif(TARGET mamba::libmamba-network-static)
        target_link_libraries(${target_name} PUBLIC mamba::libmamba-network-static)
    else()
        message(FATAL_ERROR "libmamba-network must be built before libmamba-solver")
    endif()

    # solv-cpp is added in main CMakeLists.txt, just link to it

    if(${linkage_upper} STREQUAL "STATIC")
        target_link_libraries(
            ${target_name} PUBLIC solv::libsolv_static solv::libsolvext_static solv::cpp
        )
        # simdjson is needed for helpers.cpp (JSON parsing)
        target_link_libraries(${target_name} PUBLIC simdjson::simdjson_static)
    else()
        target_link_libraries(${target_name} PUBLIC solv::libsolv solv::libsolvext solv::cpp)
        # simdjson is needed for helpers.cpp (JSON parsing)
        target_link_libraries(${target_name} PUBLIC simdjson::simdjson)
    endif()

    if(UNIX)
        math(EXPR LIBMAMBA_BINARY_COMPATIBLE "${LIBMAMBA_BINARY_CURRENT} - ${LIBMAMBA_BINARY_AGE}")
        set_target_properties(
            ${target_name}
            PROPERTIES
                COMPILE_DEFINITIONS "LIBMAMBA_SOLVER_EXPORTS"
                PREFIX ""
                VERSION
                "${LIBMAMBA_BINARY_COMPATIBLE}.${LIBMAMBA_BINARY_REVISION}.${LIBMAMBA_BINARY_AGE}"
                SOVERSION ${LIBMAMBA_BINARY_COMPATIBLE}
                OUTPUT_NAME "${output_name}"
        )
    else()
        set_target_properties(
            ${target_name}
            PROPERTIES
                COMPILE_DEFINITIONS "LIBMAMBA_SOLVER_EXPORTS"
                PREFIX ""
                VERSION ${LIBMAMBA_BINARY_VERSION}
                SOVERSION ${LIBMAMBA_BINARY_CURRENT}
                OUTPUT_NAME "${output_name}"
        )
    endif()

    list(APPEND libmamba_solver_targets ${target_name})
    add_library(mamba::${target_name} ALIAS ${target_name})
endmacro()

set(libmamba_solver_targets "")
