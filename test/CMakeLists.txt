cmake_minimum_required(VERSION 3.1)

find_package(GTest REQUIRED)

set(TEST_SRCS
	test_cpp.cpp
	test_url.cpp
	test_shell_init.cpp
	test_activation.cpp
)

add_executable(test_mamba ${TEST_SRCS})
target_link_libraries(test_mamba PRIVATE ${GTEST_BOTH_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
target_link_libraries(test_mamba PUBLIC mamba_api)

add_custom_target(test COMMAND test_mamba DEPENDS test_mamba)