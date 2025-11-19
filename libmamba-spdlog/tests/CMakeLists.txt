cmake_minimum_required(VERSION 3.16)

find_package(Catch2 REQUIRED)

add_executable( test_libmamba_logging_spdlog test_logging_spdlog.cpp )

target_link_libraries(test_libmamba_logging_spdlog
    PUBLIC
        mamba::libmamba
        mamba::libmamba-spdlog
        mamba::libtesting_mamba_logging_common
        Catch2::Catch2WithMain
)

target_compile_features(test_libmamba_logging_spdlog PUBLIC cxx_std_20)
set_target_properties(
    test_libmamba_logging_spdlog
    PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
)

mamba_target_add_compile_warnings(test_libmamba_logging_spdlog WARNING_AS_ERROR ${MAMBA_WARNING_AS_ERROR})

# ##################################################################################################

add_custom_target(
    test_logging_spdlog
    COMMAND test_libmamba_logging_spdlog
    DEPENDS test_libmamba_logging
)
