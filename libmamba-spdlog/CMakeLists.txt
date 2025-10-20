# Copyright (c) 2025, QuantStack and Mamba Contributors
#
# Distributed under the terms of the BSD 3-Clause License.
#
# The full license is in the file LICENSE, distributed with this software.
cmake_minimum_required(VERSION 3.18.2)

include("../cmake/CompilerWarnings.cmake")

project(libmamba-spdlog)

if(NOT TARGET mamba::libmamba)
    find_package(libmamba CONFIG REQUIRED)
    set(libmamba_target mamba::libmamba-dyn)
else()
    set(libmamba_target mamba::libmamba)
endif()

find_package(spdlog CONFIG REQUIRED)

# For some reasons, using target_compile_definitions does not set the definitions properly
add_compile_definitions(
    SPDLOG_FMT_EXTERNAL "SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_${BUILD_LOG_LEVEL}"
)

add_library(libmamba-spdlog INTERFACE)
target_sources(libmamba-spdlog
    INTERFACE
        include/mamba/logging_spdlog.hpp
        include/mamba/logging_spdlog_impl.hpp
)
add_library(mamba::libmamba-spdlog ALIAS libmamba-spdlog)

target_include_directories(libmamba-spdlog INTERFACE include/)

target_link_libraries(libmamba-spdlog 
    INTERFACE 
        ${libmamba_target}
        # Since conda-forge spdlog is built with a bundled version of fmt we use the header
        # only version to avoid chasing after the correct fmt version matching the one used
        # in the bundle
        spdlog::spdlog_header_only
)

mamba_target_add_compile_warnings(libmamba-spdlog WARNING_AS_ERROR ${MAMBA_WARNING_AS_ERROR})

target_compile_features(libmamba-spdlog INTERFACE cxx_std_20)
set_target_properties(
    libmamba-spdlog
    PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
)


# install(
#     EXPORT ${PROJECT_NAME}Targets
#     NAMESPACE mamba::
#     DESTINATION ${LIBMAMBA_CMAKECONFIG_INSTALL_DIR}
#     COMPONENT Mamba_Development
# )
# export(EXPORT ${PROJECT_NAME}Targets NAMESPACE mamba::)


add_subdirectory(tests/)