# Copyright (c) 2024, QuantStack and Mamba Contributors
#
# Distributed under the terms of the BSD 3-Clause License.
#
# The full license is in the file LICENSE, distributed with this software.

cmake_minimum_required(VERSION 3.16)

add_executable(
    test_solv_cpp
    src/main.cpp
    src/msvc_string_maker.cpp
    src/pool_data.cpp
    src/test_pool.cpp
    src/test_queue.cpp
    src/test_repo.cpp
    src/test_scenarios.cpp
    src/test_solvable.cpp
    src/test_solver.cpp
    src/test_transaction.cpp
)
target_include_directories(test_solv_cpp PRIVATE src/)
find_package(Catch2 REQUIRED)
target_link_libraries(test_solv_cpp PRIVATE Catch2::Catch2WithMain solv::cpp)
set_target_properties(
    test_solv_cpp PROPERTIES COMPILE_DEFINITIONS CATCH_CONFIG_ENABLE_ALL_STRINGMAKERS
)
