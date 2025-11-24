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
find_package(Threads REQUIRED)

# For some reasons, using target_compile_definitions does not set the definitions properly
add_compile_definitions(
    SPDLOG_FMT_EXTERNAL "SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_${BUILD_LOG_LEVEL}"
)

add_library(libmamba-spdlog INTERFACE)
# target_sources(libmamba-spdlog
#     INTERFACE
#         include/mamba/spdlog/logging_spdlog.hpp
#         include/mamba/spdlog/logging_spdlog_impl.hpp
# )
add_library(mamba::libmamba-spdlog ALIAS libmamba-spdlog)

target_include_directories(libmamba-spdlog INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)


target_link_libraries(libmamba-spdlog
    INTERFACE
        ${libmamba_target}

        # Since conda-forge spdlog is built with a bundled version of fmt we use the header
        # only version to avoid chasing after the correct fmt version matching the one used
        # in the bundle.
        spdlog::spdlog_header_only

        # We need the threading facilities available whatever the user use.
        # This should have been part of depending on spdlog but for some reason it's
        # not made available automatically.
        Threads::Threads
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

include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "${PROJECT_BINARY_DIR}/${PROJECT_NAME}Version.cmake"
    VERSION 1.0
    COMPATIBILITY AnyNewerVersion
)
install(TARGETS libmamba-spdlog
    EXPORT ${PROJECT_NAME}Targets
    LIBRARY DESTINATION lib COMPONENT Runtime
    ARCHIVE DESTINATION lib COMPONENT Development
    RUNTIME DESTINATION bin COMPONENT Runtime
    PUBLIC_HEADER DESTINATION include COMPONENT Development
    BUNDLE DESTINATION bin COMPONENT Runtime
)
configure_package_config_file(
    "${PROJECT_SOURCE_DIR}/cmake/${PROJECT_NAME}Config.cmake.in"
    "${PROJECT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
)

install(EXPORT ${PROJECT_NAME}Targets
    NAMESPACE mamba::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME})

install(FILES "${PROJECT_BINARY_DIR}/${PROJECT_NAME}Version.cmake"
              "${PROJECT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME})

install(DIRECTORY ${PROJECT_SOURCE_DIR}/include/ DESTINATION include)

if(BUILD_LIBMAMBA_SPDLOG_TESTS)
    add_subdirectory(tests/)
endif()

