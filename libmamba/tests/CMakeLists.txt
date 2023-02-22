cmake_minimum_required(VERSION 3.1)


add_executable(testing_libmamba_lock testing/lock.cpp)

target_link_libraries(testing_libmamba_lock PUBLIC libmamba)

target_compile_features(testing_libmamba_lock PUBLIC cxx_std_17)


find_package(GTest)
find_package(Threads REQUIRED)

set(LIBMAMBA_TEST_SRCS
    ../longpath.manifest
    test_activation.cpp
    test_channel.cpp
    test_configuration.cpp
    test_cpp.cpp
    test_env_file_reading.cpp
    test_environments_manager.cpp
    history_test/test_history.cpp
    test_lockfile.cpp
    test_pinning.cpp
    test_output.cpp
    test_progress_bar.cpp
    test_shell_init.cpp
    test_thread_utils.cpp
    test_transfer.cpp
    test_url.cpp
    test_validate.cpp
    test_virtual_packages.cpp
    test_util.cpp
    test_util_cast.cpp
    test_util_compare.cpp
    test_util_string.cpp
    test_util_graph.cpp
    test_env_lockfile.cpp
    test_execution.cpp
    test_invoke.cpp
    test_tasksync.cpp
    test_filesystem.cpp
    test_satisfiability_error.cpp
)

message(STATUS "Building libmamba C++ tests")

add_executable(test_libmamba ${LIBMAMBA_TEST_SRCS})
mamba_target_add_compile_warnings(test_libmamba WARNING_AS_ERROR ${MAMBA_WARNING_AS_ERROR})

target_include_directories(
    test_libmamba
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(
    test_libmamba
    PUBLIC libmamba
    PRIVATE GTest::GTest GTest::Main Threads::Threads
)

target_compile_definitions(
    test_libmamba
    PRIVATE
        MAMBA_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
        MAMBA_TEST_LOCK_EXE="${CMAKE_CURRENT_BINARY_DIR}/testing_libmamba_lock"
)

target_compile_features(test_libmamba PUBLIC cxx_std_17)

add_custom_target(test COMMAND test_libmamba DEPENDS test_libmamba)
