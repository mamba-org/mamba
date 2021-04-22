cmake_minimum_required(VERSION 3.1)

if(WIN32)
    find_package(GTest)
else()
    add_library(GTest::GTest INTERFACE IMPORTED)
    target_link_libraries(GTest::GTest INTERFACE gtest)
    add_library(GTest::Main INTERFACE IMPORTED)
    target_link_libraries(GTest::Main INTERFACE gtest_main)
endif()

include_directories(${GTEST_INCLUDE_DIRS} SYSTEM)

set(TEST_SRCS
    test_channel.cpp
    test_configuration.cpp
    test_cpp.cpp
    test_url.cpp
    history_test/test_history.cpp
    test_shell_init.cpp
    test_activation.cpp
    test_string_methods.cpp
    test_environments_manager.cpp
    test_transfer.cpp
    test_thread_utils.cpp
    test_graph.cpp
    test_pinning.cpp
    test_virtual_packages.cpp
    test_env_file_reading.cpp
)

add_executable(test_mamba ${TEST_SRCS})

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/history_test/conda-meta/history
    ${CMAKE_CURRENT_BINARY_DIR}/history_test/conda-meta/history COPYONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/history_test/conda-meta/aux_file
    ${CMAKE_CURRENT_BINARY_DIR}/history_test/conda-meta/aux_file COPYONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config_test/.condarc
    ${CMAKE_CURRENT_BINARY_DIR}/config_test/.condarc COPYONLY)

target_link_libraries(test_mamba PRIVATE GTest::GTest GTest::Main ${CMAKE_THREAD_LIBS_INIT})
target_link_libraries(test_mamba PUBLIC mamba-static)
set_property(TARGET test_mamba PROPERTY CXX_STANDARD 17)

add_custom_target(test COMMAND test_mamba DEPENDS test_mamba)
