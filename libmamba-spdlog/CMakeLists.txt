# Copyright (c) 2025, QuantStack and Mamba Contributors
#
# Distributed under the terms of the BSD 3-Clause License.
#
# The full license is in the file LICENSE, distributed with this software.
cmake_minimum_required(VERSION 3.18.2)

include("../cmake/CompilerWarnings.cmake")

project(libmamba-spdlog)

if(NOT TARGET mamba::libmamba-dyn AND NOT TARGET mamba::libmamba-static)
    find_package(libmamba)
endif()

find_package(spdlog CONFIG REQUIRED)
find_package(Threads REQUIRED)

# For some reasons, using target_compile_definitions does not set the definitions properly
add_compile_definitions(SPDLOG_FMT_EXTERNAL "SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_${BUILD_LOG_LEVEL}")

macro(libmamba_spdlog_create_target target_name linkage libmamba_target)
    string(TOUPPER "${linkage}" linkage_upper)
    if(NOT ${linkage_upper} MATCHES "^(SHARED|STATIC)$")
        message(FATAL_ERROR "Invalid library linkage: ${linkage}")
    endif()

    add_library(${target_name} INTERFACE)
    target_include_directories(
        ${target_name}
        INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
                  $<INSTALL_INTERFACE:include>
    )

    target_link_libraries(
        ${target_name}
        INTERFACE
            ${libmamba_target}
            # Since conda-forge spdlog is built with a bundled version of fmt we use the header only
            # version to avoid chasing after the correct fmt version matching the one used in the
            # bundle.
            spdlog::spdlog_header_only
            # We need the threading facilities available whatever the user use. This should have
            # been part of depending on spdlog but for some reason it's not made available
            # automatically.
            Threads::Threads
    )

    mamba_target_add_compile_warnings(${target_name} WARNING_AS_ERROR ${MAMBA_WARNING_AS_ERROR})

    target_compile_features(${target_name} INTERFACE cxx_std_20)
    set_target_properties(
        ${target_name}
        PROPERTIES
            CXX_STANDARD 20
            CXX_STANDARD_REQUIRED YES
            CXX_EXTENSIONS NO
    )

    list(APPEND libmamba_spdlog_targets ${target_name})
    add_library(mamba::${target_name} ALIAS ${target_name})
endmacro()

set(libmamba_spdlog_targets "")

if(BUILD_SHARED)
    message(STATUS "Adding target libmamba-spdlog with shared libmamba")
    libmamba_spdlog_create_target(libmamba-spdlog-dyn SHARED libmamba-dyn)
endif()

if(BUILD_STATIC)
    message(STATUS "Adding target libmamba-spdlog with static libmamba")
    libmamba_spdlog_create_target(libmamba-spdlog-static STATIC libmamba-static)
endif()

if(BUILD_SHARED)
    add_library(mamba::libmamba-spdlog ALIAS libmamba-spdlog-dyn)
elseif(BUILD_STATIC)
    add_library(mamba::libmamba-spdlog ALIAS libmamba-spdlog-static)
else()
    message(FATAL_ERROR "Select at least a build variant for libmamba")
endif()

include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "${PROJECT_BINARY_DIR}/${PROJECT_NAME}Version.cmake"
    VERSION 1.0
    COMPATIBILITY AnyNewerVersion
)
install(
    TARGETS ${libmamba_spdlog_targets}
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

install(
    EXPORT ${PROJECT_NAME}Targets
    NAMESPACE mamba::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
)

install(
    FILES "${PROJECT_BINARY_DIR}/${PROJECT_NAME}Version.cmake"
          "${PROJECT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
)

install(DIRECTORY ${PROJECT_SOURCE_DIR}/include/ DESTINATION include)

if(BUILD_LIBMAMBA_SPDLOG_TESTS)
    add_subdirectory(tests/)
endif()
