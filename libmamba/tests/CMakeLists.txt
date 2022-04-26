cmake_minimum_required(VERSION 3.1)

find_package(GTest)
find_package(Threads REQUIRED)

include_directories(${GTEST_INCLUDE_DIRS} SYSTEM)

set(TEST_SRCS
    test_activation.cpp
    test_channel.cpp
    test_configuration.cpp
    test_cpp.cpp
    test_env_file_reading.cpp
    test_environments_manager.cpp
    test_graph.cpp
    history_test/test_history.cpp
    test_lockfile.cpp
    test_pinning.cpp
    test_output.cpp
    test_progress_bar.cpp
    test_shell_init.cpp
    test_string_methods.cpp
    test_thread_utils.cpp
    test_transfer.cpp
    test_url.cpp
    test_validate.cpp
    test_virtual_packages.cpp
    test_util.cpp
    test_env_lockfile.cpp
)

message(STATUS "Building libmamba C++ tests")
add_executable(test_libmamba ${TEST_SRCS})

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/history_test/parse/conda-meta/history
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/history_test/parse/conda-meta/)
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/history_test/parse/conda-meta/aux_file
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/history_test/parse/conda-meta/)
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/history_test/parse_segfault/conda-meta/history
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/history_test/parse_segfault/conda-meta/)
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/config_test/.condarc
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/config_test/)
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/config_test/.condarc
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/config_test/)
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/env_file_test
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/)
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/validation_data
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/)
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/repodata_json_cache
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/)
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/env_lockfile_test
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/)

target_link_libraries(test_libmamba PRIVATE GTest::GTest GTest::Main Threads::Threads)
target_link_libraries(test_libmamba PUBLIC libmamba)
set_property(TARGET test_libmamba PROPERTY CXX_STANDARD 17)

add_custom_target(test COMMAND test_libmamba DEPENDS test_libmamba)

add_executable(testing_libmamba_lock testing/lock.cpp)
target_link_libraries(testing_libmamba_lock PUBLIC libmamba)
set_property(TARGET testing_libmamba_lock PROPERTY CXX_STANDARD 17)
