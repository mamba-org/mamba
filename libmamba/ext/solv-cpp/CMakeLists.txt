# Copyright (c) 2024, QuantStack and Mamba Contributors
#
# Distributed under the terms of the BSD 3-Clause License.
#
# The full license is in the file LICENSE, distributed with this software.

cmake_minimum_required(VERSION 3.16)

add_library(
    solv-cpp OBJECT
    src/pool.cpp
    src/queue.cpp
    src/repo.cpp
    src/solvable.cpp
    src/solver.cpp
    src/transaction.cpp
    src/dependency.cpp
)
target_include_directories(
    solv-cpp
    PUBLIC $<INSTALL_INTERFACE:include> $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
# Avoids `libsolv`'s usage of `requires` which became a keyword in C++20. See:
# https://github.com/openSUSE/libsolv/blob/master/src/solvable.h#L38
target_compile_definitions(solv-cpp PUBLIC LIBSOLV_SOLVABLE_PREPEND_DEP)

find_package(tl-expected REQUIRED)
find_package(Libsolv REQUIRED)

if(BUILD_SHARED)
    set(LIBSOLV_DEPS solv::libsolv solv::libsolvext)
    set_target_properties(solv-cpp PROPERTIES POSITION_INDEPENDENT_CODE ON)
elseif(PPC32)
    set(LIBSOLV_DEPS libsolv libsolvext)
else()
    set(LIBSOLV_DEPS solv::libsolv_static solv::libsolvext_static)
endif()

target_link_libraries(solv-cpp PUBLIC tl::expected ${LIBSOLV_DEPS})
target_compile_features(solv-cpp PUBLIC cxx_std_20)
set_target_properties(
    solv-cpp
    PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
)

mamba_target_add_compile_warnings(solv-cpp WARNING_AS_ERROR ${MAMBA_WARNING_AS_ERROR})
mamba_target_set_lto(solv-cpp MODE ${MAMBA_LTO})

add_library(solv::cpp ALIAS solv-cpp)

if(BUILD_LIBMAMBA_TESTS)
    add_subdirectory(tests)
endif()

# Object libraries are installed as an interface library (in libmambaTargets) but do not install any
# objects (.o files) or headers without the ``OBJECTS DESTINATION`` property.
install(
    TARGETS solv-cpp
    EXPORT ${PROJECT_NAME}Targets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
