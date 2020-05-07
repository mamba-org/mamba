cmake_minimum_required(VERSION 3.1)

find_package(GTest REQUIRED)
include_directories(${GTEST_INCLUDE_DIR})

set(TEST_SRCS
	test_cpp.cpp
	test_url.cpp
    history_test/test_history.cpp
	test_shell_init.cpp
	test_activation.cpp
	test_string_methods.cpp
	test_transfer.cpp
)

add_executable(test_mamba ${TEST_SRCS})

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/history_test/conda-meta/history
    ${CMAKE_CURRENT_BINARY_DIR}/history_test/conda-meta/history COPYONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/history_test/conda-meta/aux_file
    ${CMAKE_CURRENT_BINARY_DIR}/history_test/conda-meta/aux_file COPYONLY)

target_link_libraries(test_mamba PRIVATE ${GTEST_BOTH_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
target_link_libraries(test_mamba PUBLIC mamba_lib)
set_property(TARGET test_mamba PROPERTY CXX_STANDARD 17)

add_custom_target(test COMMAND test_mamba DEPENDS test_mamba)