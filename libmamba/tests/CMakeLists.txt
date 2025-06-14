cmake_minimum_required(VERSION 3.16)

add_executable(testing_libmamba_lock libmamba_lock/lock.cpp)

target_link_libraries(testing_libmamba_lock PUBLIC mamba::libmamba)

target_compile_features(testing_libmamba_lock PUBLIC cxx_std_20)
set_target_properties(
    testing_libmamba_lock
    PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
)

mamba_target_add_compile_warnings(testing_libmamba_lock WARNING_AS_ERROR ${MAMBA_WARNING_AS_ERROR})

set(
    LIBMAMBA_TEST_SRCS
    include/mambatests.hpp
    src/test_main.cpp
    # Catch utils
    src/catch-utils/conda_url.hpp
    src/catch-utils/msvc_catch_byte.cpp
    src/catch-utils/msvc_catch_string_view.cpp
    # Utility library
    src/util/test_cast.cpp
    src/util/test_charconv.cpp
    src/util/test_cryptography.cpp
    src/util/test_encoding.cpp
    src/util/test_environment.cpp
    src/util/test_flat_bool_expr_tree.cpp
    src/util/test_flat_set.cpp
    src/util/test_graph.cpp
    src/util/test_heap_optional.cpp
    src/util/test_iterator.cpp
    src/util/test_os_linux.cpp
    src/util/test_os_osx.cpp
    src/util/test_os_unix.cpp
    src/util/test_os_win.cpp
    src/util/test_parsers.cpp
    src/util/test_path_manip.cpp
    src/util/test_random.cpp
    src/util/test_synchronized_value.cpp
    src/util/test_string.cpp
    src/util/test_tuple_hash.cpp
    src/util/test_type_traits.cpp
    src/util/test_url_manip.cpp
    src/util/test_url.cpp
    src/util/test_weakening_map.cpp
    # Implementation of version and matching specs
    src/specs/test_archive.cpp
    src/specs/test_authentication_info.cpp
    src/specs/test_build_number_spec.cpp
    src/specs/test_channel.cpp
    src/specs/test_chimera_string_spec.cpp
    src/specs/test_conda_url.cpp
    src/specs/test_glob_spec.cpp
    src/specs/test_match_spec.cpp
    src/specs/test_package_info.cpp
    src/specs/test_platform.cpp
    src/specs/test_regex_spec.cpp
    src/specs/test_repo_data.cpp
    src/specs/test_unresolved_channel.cpp
    src/specs/test_version_spec.cpp
    src/specs/test_version.cpp
    # Solver tests
    src/solver/test_problems_graph.cpp
    src/solver/test_request.cpp
    src/solver/test_solution.cpp
    # Solver libsolv implementation tests
    src/solver/libsolv/test_database.cpp
    src/solver/libsolv/test_solver.cpp
    # Artifacts validation
    src/validation/test_tools.cpp
    src/validation/test_update_framework_v0_6.cpp
    src/validation/test_update_framework_v1.cpp
    # Implementation of downloaders and mirrors
    src/download/test_downloader.cpp
    src/download/test_mirror.cpp
    # Core tests
    ../longpath.manifest
    src/core/test_activation.cpp
    src/core/test_channel_context.cpp
    src/core/test_configuration.cpp
    src/core/test_cpp.cpp
    src/core/test_env_file_reading.cpp
    src/core/test_env_lockfile.cpp
    src/core/test_environments_manager.cpp
    src/core/test_execution.cpp
    src/core/test_filesystem.cpp
    src/core/test_history.cpp
    src/core/test_invoke.cpp
    src/core/test_lockfile.cpp
    src/core/test_output.cpp
    src/core/test_package_fetcher.cpp
    src/core/test_pinning.cpp
    src/core/test_progress_bar.cpp
    src/core/test_shell_init.cpp
    src/core/test_subdir_index.cpp
    src/core/test_tasksync.cpp
    src/core/test_thread_utils.cpp
    src/core/test_transaction_context.cpp
    src/core/test_util.cpp
    src/core/test_virtual_packages.cpp
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

find_package(Catch2 REQUIRED)
find_package(Threads REQUIRED)

target_link_libraries(
    test_libmamba
    PUBLIC mamba::libmamba reproc reproc++
    PRIVATE Catch2::Catch2WithMain Threads::Threads
)
set_target_properties(
    test_libmamba PROPERTIES COMPILE_DEFINITIONS CATCH_CONFIG_ENABLE_ALL_STRINGMAKERS
)

# Copy data directory into binary dir to avoid modifications
add_custom_target(
    test_libmamba_data
    COMMENT "Copying libmamba tests data"
    COMMAND "${CMAKE_COMMAND}" -E remove_directory "${CMAKE_CURRENT_BINARY_DIR}/data"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/data"
    COMMAND
        "${CMAKE_COMMAND}" -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/data"
        "${CMAKE_CURRENT_BINARY_DIR}/data"
)
add_dependencies(test_libmamba test_libmamba_data)

target_compile_definitions(
    test_libmamba
    PRIVATE
        MAMBA_TEST_DATA_DIR="${CMAKE_CURRENT_BINARY_DIR}/data"
        MAMBA_REPO_DIR="${CMAKE_SOURCE_DIR}"
        MAMBA_TEST_LOCK_EXE="$<TARGET_FILE:testing_libmamba_lock>"
)

target_compile_features(test_libmamba PUBLIC cxx_std_20)
set_target_properties(
    test_libmamba
    PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
)

add_custom_target(
    test
    COMMAND test_libmamba
    DEPENDS test_libmamba
)
