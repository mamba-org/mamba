cmake_minimum_required(VERSION 3.16)

add_executable(testing_libmamba_lock libmamba_lock/lock.cpp)

target_link_libraries(testing_libmamba_lock PUBLIC mamba::libmamba)

target_compile_features(testing_libmamba_lock PUBLIC cxx_std_17)

mamba_target_add_compile_warnings(testing_libmamba_lock WARNING_AS_ERROR ${MAMBA_WARNING_AS_ERROR})

set(
    LIBMAMBA_TEST_SRCS
    include/mambatests.hpp
    src/test_main.cpp
    # C++ wrapping of libsolv
    src/solv-cpp/pool_data.cpp
    src/solv-cpp/test_pool.cpp
    src/solv-cpp/test_queue.cpp
    src/solv-cpp/test_repo.cpp
    src/solv-cpp/test_scenarios.cpp
    src/solv-cpp/test_solvable.cpp
    src/solv-cpp/test_solver.cpp
    src/solv-cpp/test_transaction.cpp
    # Utility library
    src/util/test_cast.cpp
    src/util/test_compare.cpp
    src/util/test_cryptography.cpp
    src/util/test_encoding.cpp
    src/util/test_environment.cpp
    src/util/test_flat_bool_expr_tree.cpp
    src/util/test_flat_set.cpp
    src/util/test_graph.cpp
    src/util/test_iterator.cpp
    src/util/test_os_win.cpp
    src/util/test_path_manip.cpp
    src/util/test_parsers.cpp
    src/util/test_string.cpp
    src/util/test_tuple_hash.cpp
    src/util/test_type_traits.cpp
    src/util/test_url.cpp
    src/util/test_url_manip.cpp
    src/util/test_weakening_map.cpp
    # Implementation of version and matching specs
    src/specs/test_archive.cpp
    src/specs/test_authentication_info.cpp
    src/specs/test_channel.cpp
    src/specs/test_channel_spec.cpp
    src/specs/test_conda_url.cpp
    src/specs/test_platform.cpp
    src/specs/test_repo_data.cpp
    src/specs/test_version.cpp
    src/specs/test_version_spec.cpp
    src/specs/test_match_spec.cpp
    # Artifacts validation
    src/validation/test_tools.cpp
    src/validation/test_update_framework_v0_6.cpp
    src/validation/test_update_framework_v1.cpp
    # The un-triaged rest
    ../longpath.manifest
    src/core/test_activation.cpp
    src/core/test_channel_context.cpp
    src/core/test_configuration.cpp
    src/core/test_cpp.cpp
    src/core/test_downloader.cpp
    src/core/test_env_file_reading.cpp
    src/core/test_environments_manager.cpp
    src/core/test_history.cpp
    src/core/test_lockfile.cpp
    src/core/test_mirror.cpp
    src/core/test_pinning.cpp
    src/core/test_output.cpp
    src/core/test_progress_bar.cpp
    src/core/test_shell_init.cpp
    src/core/test_thread_utils.cpp
    src/core/test_virtual_packages.cpp
    src/core/test_util.cpp
    src/core/test_env_lockfile.cpp
    src/core/test_execution.cpp
    src/core/test_invoke.cpp
    src/core/test_tasksync.cpp
    src/core/test_filesystem.cpp
    src/core/test_satisfiability_error.cpp
)

message(STATUS "Building libmamba C++ tests")

add_executable(test_libmamba ${LIBMAMBA_TEST_SRCS})
mamba_target_add_compile_warnings(test_libmamba WARNING_AS_ERROR ${MAMBA_WARNING_AS_ERROR})

target_include_directories(
    test_libmamba
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
        "${CMAKE_SOURCE_DIR}/libmamba/src"
)

find_package(doctest REQUIRED)
find_package(Threads REQUIRED)

target_link_libraries(
    test_libmamba
    PUBLIC mamba::libmamba reproc reproc++
    PRIVATE doctest::doctest Threads::Threads
)

# Copy data directory into binary dir to avoid modifications
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/data" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")

target_compile_definitions(
    test_libmamba
    PRIVATE
        DOCTEST_CONFIG_USE_STD_HEADERS
        MAMBA_TEST_DATA_DIR="${CMAKE_CURRENT_BINARY_DIR}/data"
        MAMBA_TEST_LOCK_EXE="$<TARGET_FILE:testing_libmamba_lock>"
)

target_compile_features(test_libmamba PUBLIC cxx_std_17)

add_custom_target(
    test
    COMMAND test_libmamba
    DEPENDS test_libmamba
)
