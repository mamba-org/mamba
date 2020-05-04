cmake_minimum_required(VERSION 3.1)

find_package(GTest REQUIRED)

set(TEST_SRCS
	test_cpp.cpp
	test_url.cpp
    history_test/test_history.cpp
	test_shell_init.cpp
	test_activation.cpp
	test_string_methods.cpp
)

add_executable(test_mamba ${TEST_SRCS})

install(FILES original_history_data aux_history_data DESTINATION .)

target_link_libraries(test_mamba PRIVATE ${GTEST_BOTH_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
target_link_libraries(test_mamba PUBLIC mamba_lib)
set_property(TARGET test_mamba PROPERTY CXX_STANDARD 17)

add_custom_target(test COMMAND test_mamba DEPENDS test_mamba)