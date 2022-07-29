# Copyright (c) 2019, QuantStack and Mamba Contributors
#
# Distributed under the terms of the BSD 3-Clause License.
#
# The full license is in the file LICENSE, distributed with this software.

cmake_minimum_required(VERSION 3.18.2)
cmake_policy(SET CMP0025 NEW)
cmake_policy(SET CMP0077 NEW)

project(libmamba)

set(LIBMAMBA_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)
set(LIBMAMBA_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(LIBMAMBA_DATA_DIR ${CMAKE_CURRENT_SOURCE_DIR}/data)

# Versionning
# ===========
file(STRINGS "${LIBMAMBA_INCLUDE_DIR}/mamba/version.hpp" libmamba_version_defines
     REGEX "#define LIBMAMBA_VERSION_(MAJOR|MINOR|PATCH)")
foreach(ver ${libmamba_version_defines})
    if(ver MATCHES "#define LIBMAMBA_VERSION_(MAJOR|MINOR|PATCH) +([^ ]+)$")
        set(LIBMAMBA_VERSION_${CMAKE_MATCH_1} "${CMAKE_MATCH_2}" CACHE INTERNAL "")
    endif()
endforeach()
set(${PROJECT_NAME}_VERSION
    ${LIBMAMBA_VERSION_MAJOR}.${LIBMAMBA_VERSION_MINOR}.${LIBMAMBA_VERSION_PATCH})
message(STATUS "Building libmamba v${${PROJECT_NAME}_VERSION}")


# Binary version
# See the following URL for explanations about the binary versionning
# https://www.gnu.org/software/libtool/manual/html_node/Updating-version-info.html#Updating-version-info
file(STRINGS "${LIBMAMBA_INCLUDE_DIR}/mamba/version.hpp" libmamba_version_defines
     REGEX "#define LIBMAMBA_BINARY_(CURRENT|REVISION|AGE)")
foreach(ver ${libmamba_version_defines})
    if(ver MATCHES "#define LIBMAMBA_BINARY_(CURRENT|REVISION|AGE) +([^ ]+)$")
        set(LIBMAMBA_BINARY_${CMAKE_MATCH_1} "${CMAKE_MATCH_2}" CACHE INTERNAL "")
    endif()
endforeach()
set(LIBMAMBA_BINARY_VERSION
    ${LIBMAMBA_BINARY_CURRENT}.${LIBMAMBA_BINARY_REVISION}.${LIBMAMBA_BINARY_AGE})
message(STATUS "libmamba binary version: v${LIBMAMBA_BINARY_VERSION}")


# Build options
# =============

option(BUILD_TESTS "Build libmamba C++ tests" OFF)
option(BUILD_SHARED "Build shared libmamba library" OFF)
option(BUILD_STATIC "Build static libmamba library" OFF)
option(BUILD_STATIC_DEPS "Build static libmamba library with static linkage to its dependencies" OFF)
set(BUILD_LOG_LEVEL "TRACE" CACHE STRING "Logger active level at compile time")

if (NOT ${BUILD_LOG_LEVEL} MATCHES "^(TRACE|DEBUG|INFO|WARN|ERROR|CRITICAL|OFF)$")
    message(FATAL_ERROR "Invalid log level: ${BUILD_LOG_LEVEL}, should be one of { TRACE, DEBUG, INFO, WARN, ERROR, CRITICAL, OFF }")
endif ()

if (BUILD_STATIC_DEPS)
    add_definitions("-DSPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_${BUILD_LOG_LEVEL}")
endif ()

if (BUILD_STATIC_DEPS)
    add_definitions(-DLIBMAMBA_STATIC_DEPS)
endif ()

if (WIN32)
    set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif ()

option(ENABLE_ASAN "Enable Address-Sanitizer (currently only supported on GCC and Clang)" OFF)

if(ENABLE_ASAN)
    message(WARNING "Address-Sanitizer instrumentation will be injected into binaries - do not release these binaries")
    add_compile_options(-fno-omit-frame-pointer  -fsanitize=address)
    add_link_options(-fno-omit-frame-pointer  -fsanitize=address)
endif()

if(ENABLE_TSAN)
    message(WARNING "Thread-Sanitizer instrumentation will be injected into binaries - do not release these binaries")
    add_compile_options(-fno-omit-frame-pointer  -fsanitize=thread)
    add_link_options(-fno-omit-frame-pointer  -fsanitize=thread)
endif()

if(ENABLE_USAN)
    message(WARNING "Undefined-Sanitizer instrumentation will be injected into binaries - do not release these binaries")
    add_compile_options(-fno-omit-frame-pointer  -fsanitize=undefined)
    add_link_options(-fno-omit-frame-pointer  -fsanitize=undefined)
endif()


# Source files
# ============

find_package (Python3 COMPONENTS Interpreter)

set(SHELL_SCRIPTS
    micromamba.sh
    micromamba.csh
    micromamba.bat
    activate.bat
    _mamba_activate.bat
    mamba_hook.bat
    mamba_hook.ps1
    Mamba.psm1
    mamba.xsh
    mamba.fish
    compile_pyc.py
    mamba_completion.posix)

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/shell_scripts)
foreach(script ${SHELL_SCRIPTS})
    string(REPLACE "." "_" script_var ${script})
    add_custom_command(OUTPUT shell_scripts/${script}.cpp
                       DEPENDS data/${script}
                       COMMAND ${Python3_EXECUTABLE} ${LIBMAMBA_DATA_DIR}/bin2header.py
                        --extern
                        -v data_${script_var}
                        -i ${CMAKE_CURRENT_SOURCE_DIR}/data/${script}
                        -o ${CMAKE_CURRENT_BINARY_DIR}/shell_scripts/${script}.cpp)
endforeach()

set(LIBMAMBA_SOURCES
    longpath.manifest
    ${LIBMAMBA_SOURCE_DIR}/version.cpp
    # Core API (low-level)
    ${LIBMAMBA_SOURCE_DIR}/core/singletons.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/activation.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/channel.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/context.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/environment.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/environments_manager.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/error_handling.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/fetch.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/transaction_context.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/link.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/history.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/match_spec.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/menuinst.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/url.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/output.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/package_handling.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/package_cache.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/pool.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/prefix_data.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/satisfiability_error.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/progress_bar.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/progress_bar_impl.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/pinning.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/package_info.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/package_paths.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/query.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/repo.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/shell_init.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/solver.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/subdirdata.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/thread_utils.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/transaction.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/util.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/util_os.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/validate.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/virtual_packages.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/env_lockfile.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/execution.cpp
    # API (high-level)
    ${LIBMAMBA_SOURCE_DIR}/api/c_api.cpp
    ${LIBMAMBA_SOURCE_DIR}/api/channel_loader.cpp
    ${LIBMAMBA_SOURCE_DIR}/api/clean.cpp
    ${LIBMAMBA_SOURCE_DIR}/api/config.cpp
    ${LIBMAMBA_SOURCE_DIR}/api/configuration.cpp
    ${LIBMAMBA_SOURCE_DIR}/api/create.cpp
    ${LIBMAMBA_SOURCE_DIR}/api/info.cpp
    ${LIBMAMBA_SOURCE_DIR}/api/install.cpp
    ${LIBMAMBA_SOURCE_DIR}/api/list.cpp
    ${LIBMAMBA_SOURCE_DIR}/api/remove.cpp
    ${LIBMAMBA_SOURCE_DIR}/api/repoquery.cpp
    ${LIBMAMBA_SOURCE_DIR}/api/shell.cpp
    ${LIBMAMBA_SOURCE_DIR}/api/update.cpp
)

foreach(script ${SHELL_SCRIPTS})
    list(APPEND LIBMAMBA_SOURCES shell_scripts/${script}.cpp)
endforeach()

set(LIBMAMBA_HEADERS
    ${LIBMAMBA_INCLUDE_DIR}/mamba/version.hpp
    # Core API (low-level)
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/activation.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/channel.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/channel_builder.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/context.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/environment.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/environments_manager.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/error_handling.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/fetch.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/fsutil.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/graph_util.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/satisfiability_error.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/history.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/link.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/mamba_fs.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/match_spec.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/menuinst.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/output.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/package_cache.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/package_handling.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/package_info.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/package_paths.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/pool.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/prefix_data.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/progress_bar.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/pinning.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/query.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/queue.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/repo.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/shell_init.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/solver.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/subdirdata.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/thread_utils.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/transaction.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/transaction_context.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/url.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/util.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/util_os.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/util_random.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/util_scope.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/util_string.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/validate.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/virtual_packages.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/env_lockfile.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/tasksync.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/invoke.hpp
    # API (high-level)
    ${LIBMAMBA_INCLUDE_DIR}/mamba/api/c_api.h
    ${LIBMAMBA_INCLUDE_DIR}/mamba/api/channel_loader.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/api/clean.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/api/config.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/api/configuration.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/api/configuration_impl.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/api/constants.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/api/create.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/api/info.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/api/install.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/api/list.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/api/remove.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/api/repoquery.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/api/shell.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/api/update.hpp
)

# Targets and link
# ================

macro(libmamba_create_target target_name linkage deps_linkage output_name)
    string(TOUPPER "${linkage}" linkage_upper)
    if (NOT ${linkage_upper} MATCHES "^(SHARED|STATIC)$")
        message(FATAL_ERROR "Invalid library linkage: ${linkage}")
    endif ()

    string(TOUPPER "${deps_linkage}" deps_linkage_upper)
    if (NOT ${deps_linkage_upper} MATCHES "^(SHARED|STATIC)$")
        message(FATAL_ERROR "Invalid library dependencies linkage: ${deps_linkage_upper}")
    endif ()

    # Output
    # ======
    add_library(${target_name} ${linkage_upper} ${LIBMAMBA_SOURCES} ${LIBMAMBA_HEADERS})

    if (${deps_linkage_upper} STREQUAL "STATIC")
        message("   -> Statically linking against libmamba (static) dependencies")
        if (UNIX)

            set(REQUIRED_STATIC_DEPS
                libcurl.a
                libssh2.a

                libgssapi_krb5.a
                libkrb5.a
                libk5crypto.a
                libkrb5support.a
                libcom_err.a

                libssl.a
                libcrypto.a

                libarchive.a

                libiconv.a
                libbz2.a
                liblz4.a
                libzstd.a
                libz.a
                liblzma.a
                liblzo2.a

                libsolv.a
                libsolvext.a

                libnghttp2.a

                libyaml-cpp.a

                libreproc++.a
                libreproc.a
            )

            set(STATIC_DEPS "")
            foreach(LIB ${REQUIRED_STATIC_DEPS})
                set(TMP_LIB "${LIB}-NOTFOUND")
                find_library(TMP_LIB
                    NAMES "${LIB}"
                )
                list(APPEND STATIC_DEPS "${TMP_LIB}")
            endforeach(LIB)

            if (APPLE)
                find_library(SECURITY_LIBRARY Security)
                find_library(COREFOUNDATION_LIBRARY CoreFoundation)
                message("Found library: ${SECURITY_LIBRARY}\n${COREFOUNDATION_LIBRARY}")
                list(APPEND STATIC_DEPS ${COREFOUNDATION_LIBRARY} ${SECURITY_LIBRARY})
            endif()

            message("   -> Found static dependencies:")
            foreach(LIB ${STATIC_DEPS})
                message("      - ${LIB}")
            endforeach(LIB)

            if (APPLE)
                set (MAMBA_FORCE_DYNAMIC_LIBS resolv)
                target_link_options(libmamba-full-static PRIVATE -static-libstdc++)
            elseif (UNIX)
                set(MAMBA_FORCE_DYNAMIC_LIBS rt dl resolv)
                target_link_options(libmamba-full-static PUBLIC -static-libstdc++ -static-libgcc)
            endif()

            target_link_libraries(libmamba-full-static PUBLIC ${STATIC_DEPS} ${MAMBA_FORCE_DYNAMIC_LIBS})

        elseif (WIN32)

            set(CMAKE_PREFIX_PATH "$ENV{VCPKG_ROOT}/installed/x64-windows-static/")

            # For Windows we have a vcpkg based build system right now.
            find_package(LibArchive REQUIRED)
            find_package(CURL CONFIG REQUIRED)
            find_package(yaml-cpp CONFIG REQUIRED)
            find_package(nlohmann_json CONFIG REQUIRED)
            find_package(LibLZMA CONFIG REQUIRED)
            find_library(LZ4_LIBRARY NAMES lz4)
            find_library(LZO2_LIBRARY NAMES lzo2)
            find_package(zstd CONFIG REQUIRED)
            find_library(BZIP2_LIBRARIES NAMES bz2)
            find_library(CRYPTO_LIBRARIES NAMES libcrypto)

            find_library(LIBSOLV_BUILD_STATICRARIES NAMES solv_static)
            find_library(LIBSOLVEXT_BUILD_STATICRARIES NAMES solvext_static)
            find_package(reproc++ CONFIG REQUIRED)
            find_package(tl-expected REQUIRED)

            find_library(LIBXML2_LIBRARY NAMES libxml2)
            find_library(ICONV_LIBRARY NAMES libiconv)
            find_library(CHARSET_LIBRARY NAMES libcharset)
            message("Found: ${LIBXML2_LIBRARY} ${ICONV_LIBRARY} ${CHARSET_LIBRARY}")

            target_link_libraries(${target_name} PUBLIC
                ${CRYPTO_LIBRARIES}
                ${LibArchive_LIBRARY}
                ${LIBXML2_LIBRARY}
                ${ICONV_LIBRARY}
                ${CHARSET_LIBRARY}
                libzstd
                ${LZ4_LIBRARY}
                ${LZO2_LIBRARY}
                ${BZIP2_LIBRARIES}
                ${LIBLZMA_LIBRARIES}
                CURL::libcurl
                nlohmann_json::nlohmann_json
                ${YAML_CPP_LIBRARIES}
                ${LIBSOLV_BUILD_STATICRARIES}
                ${LIBSOLVEXT_BUILD_STATICRARIES}
                ${sodium_LIBRARY_RELEASE}
                reproc++
                reproc
                tl::expected
            )

            add_definitions("-DLIBARCHIVE_STATIC -DCURL_STATICLIB -DSOLV_BUILD_STATIC")
            set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT")
            include_directories($ENV{CONDA_PREFIX}/Library/include/)
            include_directories($ENV{VCPKG_ROOT}/installed/x64-windows-static/include/)
        endif()
    else ()
        message("   -> Dynamically linking against libmamba (shared) dependencies")

        find_library(LIBSOLV_LIBRARIES NAMES solv)
        find_library(LIBSOLVEXT_LIBRARIES NAMES solvext)
        find_package(CURL REQUIRED)
        find_package(LibArchive REQUIRED)
        find_package(OpenSSL REQUIRED)
        find_package(yaml-cpp CONFIG REQUIRED)
        find_package(reproc++ CONFIG REQUIRED)
        find_package(tl-expected REQUIRED)

        set(LIBMAMBA_LIBRARIES_DEPS
            ${LIBSOLV_LIBRARIES}
            ${LIBSOLVEXT_LIBRARIES}
            ${LibArchive_LIBRARIES}
            ${CURL_LIBRARIES}
            ${OPENSSL_LIBRARIES}
            yaml-cpp
            reproc++
            reproc
            tl::expected
            powerloader
            zck
            spdlog
        )

        target_link_libraries(${target_name} PUBLIC
                              ${LIBMAMBA_LIBRARIES_DEPS}
                              ${MAMBA_FORCE_DYNAMIC_LIBS})
    endif ()

    set_property(TARGET ${target_name} PROPERTY CXX_STANDARD 17)

    target_include_directories(
        ${target_name}
        PUBLIC
            $<BUILD_INTERFACE:${LIBMAMBA_INCLUDE_DIR}>
            $<INSTALL_INTERFACE:include>
        PRIVATE
            $ENV{CONDA_PREFIX}/include
    )

    if (WIN32)
        find_path(WINREG_INCLUDE_DIR NAMES WinReg.hpp)
    endif()

    target_include_directories(
        ${target_name}
        PRIVATE
            ${LIBMAMBA_SOURCE_DIR}
            ${WINREG_INCLUDE_DIR}
    )

    if (UNIX)
        math(EXPR LIBMAMBA_BINARY_COMPATIBLE "${LIBMAMBA_BINARY_CURRENT} - ${LIBMAMBA_BINARY_AGE}")
        set_target_properties(
            ${target_name}
            PROPERTIES
            PUBLIC_HEADER "${LIBMAMBA_HEADERS}"
            COMPILE_DEFINITIONS "LIBMAMBA_EXPORTS"
            PREFIX ""
            VERSION "${LIBMAMBA_BINARY_COMPATIBLE}.${LIBMAMBA_BINARY_REVISION}.${LIBMAMBA_BINARY_AGE}"
            SOVERSION ${LIBMAMBA_BINARY_COMPATIBLE}
            OUTPUT_NAME "${output_name}"
        )
    else()
        set_target_properties(
            ${target_name}
            PROPERTIES
            PUBLIC_HEADER "${LIBMAMBA_HEADERS}"
            COMPILE_DEFINITIONS "LIBMAMBA_EXPORTS"
            PREFIX ""
            VERSION ${LIBMAMBA_BINARY_VERSION}
            SOVERSION ${LIBMAMBA_BINARY_CURRENT}
            OUTPUT_NAME "${output_name}"
        )
        target_compile_definitions(${target_name} PUBLIC GHC_WIN_DISABLE_WSTRING_STORAGE_TYPE)
    endif()

    if (${linkage_upper} STREQUAL "STATIC")
        find_package(nlohmann_json CONFIG REQUIRED)
        find_package(Threads REQUIRED)

        target_link_libraries(${target_name} PUBLIC
                              nlohmann_json::nlohmann_json
                              Threads::Threads)
    endif ()

    list(APPEND libmamba_targets ${target_name})
endmacro()


set(libmamba_targets "")

if (BUILD_SHARED)
    message(STATUS "Adding shared libmamba target")
    libmamba_create_target(libmamba SHARED SHARED libmamba)
endif ()

if (BUILD_STATIC)
    message(STATUS "Adding static libmamba target")

    # On Windows, a static library should use a different output name
    # to avoid the conflict with the import library of a shared one.
    if (CMAKE_HOST_WIN32)
        libmamba_create_target(libmamba-static STATIC SHARED libmamba_static)
    else ()
        libmamba_create_target(libmamba-static STATIC SHARED libmamba)
    endif ()
endif ()

if (BUILD_STATIC_DEPS)
    message(STATUS "Adding full-static libmamba target")

    if (CMAKE_HOST_WIN32)
        libmamba_create_target(libmamba-full-static STATIC STATIC libmamba_full_static)
    else ()
        libmamba_create_target(libmamba-full-static STATIC STATIC libmamba_full_static)
    endif ()
endif ()

if (NOT (BUILD_SHARED OR BUILD_STATIC OR BUILD_STATIC_DEPS))
    message(FATAL_ERROR "Select at least a build variant for libmamba")
endif ()

# Tests
if (BUILD_TESTS)
    add_subdirectory(tests)
endif()

# Installation
# ============

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(LIBMAMBA_CMAKECONFIG_INSTALL_DIR "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}" CACHE STRING "install path for libmambaConfig.cmake")

install(TARGETS ${libmamba_targets}
        EXPORT ${PROJECT_NAME}-targets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install(DIRECTORY "${LIBMAMBA_INCLUDE_DIR}/"
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
        FILES_MATCHING
        PATTERN "*.hpp"
        PATTERN "*.h")

# Makes the project importable from the build directory
export(EXPORT ${PROJECT_NAME}-targets
        FILE "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Targets.cmake")

# Configure 'mambaConfig.cmake' for a build tree
set(MAMBA_CONFIG_CODE "####### Expanded from \@MAMBA_CONFIG_CODE\@ #######\n")
set(MAMBA_CONFIG_CODE "${MAMBA_CONFIG_CODE}set(CMAKE_MODULE_PATH \"${CMAKE_CURRENT_SOURCE_DIR}/cmake;\${CMAKE_MODULE_PATH}\")\n")
set(MAMBA_CONFIG_CODE "${MAMBA_CONFIG_CODE}##################################################")
configure_package_config_file(${PROJECT_NAME}Config.cmake.in
                                "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
                                INSTALL_DESTINATION ${PROJECT_BINARY_DIR})

# Configure 'mambaConfig.cmake' for an install tree
set(MAMBA_CONFIG_CODE "")
configure_package_config_file(${PROJECT_NAME}Config.cmake.in
                                "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/${PROJECT_NAME}Config.cmake"
                                INSTALL_DESTINATION ${LIBMAMBA_CMAKECONFIG_INSTALL_DIR})


write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake
                                    VERSION ${MAMBA_VERSION_MAJOR}.${MAMBA_VERSION_MINOR}.${MAMBA_VERSION_PATCH}
                                    COMPATIBILITY AnyNewerVersion)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/${PROJECT_NAME}Config.cmake
                ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake
                DESTINATION ${LIBMAMBA_CMAKECONFIG_INSTALL_DIR})
install(EXPORT ${PROJECT_NAME}-targets
        FILE ${PROJECT_NAME}Targets.cmake
        DESTINATION ${LIBMAMBA_CMAKECONFIG_INSTALL_DIR})
