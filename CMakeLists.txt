# Copyright (c) 2019, QuantStack and Mamba Contributors
#
# Distributed under the terms of the BSD 3-Clause License.
#
# The full license is in the file LICENSE, distributed with this software.

cmake_minimum_required (VERSION 2.8.11)
if(POLICY CMP0025)
  cmake_policy(SET CMP0025 NEW)
endif()
project(mamba)

set(MAMBA_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)

if (MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /DNOMINMAX")
    set(CMAKE_BUILD_TYPE Release)
    # add_definitions("-DUNICODE -D_UNICODE")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")
endif()

set(CMAKE_CXX_STANDARD 17)

option(BUILD_EXE OFF)
option(BUILD_BINDINGS ON)
option(LINK_STATIC OFF)
option(USE_CONDA_PYTHON ON)

if (USE_CONDA_PYTHON)
    message("Using conda prefix: $ENV{CONDA_PREFIX}")
    set(PYTHON_EXECUTABLE "$ENV{CONDA_PREFIX}/bin/python")
    set(PYTHON_LIBRARIES "$ENV{CONDA_PREFIX}/lib/")
endif ()

find_package(Threads REQUIRED)
find_library(LIBSOLV_LIBRARIES NAMES solv)
find_library(LIBSOLVEXT_LIBRARIES NAMES solvext)
find_package(CURL REQUIRED)
find_package(LibArchive REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(nlohmann_json REQUIRED)

set(MAMBA_REQUIRED_LIBS
    ${LIBSOLV_LIBRARIES}
    ${LIBSOLVEXT_LIBRARIES}
    ${LibArchive_LIBRARIES}
    ${CURL_LIBRARIES}
    ${OPENSSL_LIBRARIES}
    nlohmann_json::nlohmann_json
)

# Versionning
# ===========

# Getting the mamba version from python
file (READ mamba/_version.py VERSION_FILE)

STRING(REGEX REPLACE "\r?\n" ";" TMP_VERSION_FILE_LIST "${VERSION_FILE}")
LIST(GET TMP_VERSION_FILE_LIST 0 VERSION_LINE)
string(REGEX MATCHALL "version_info = \\(([0-9]*), ([0-9]*), ([0-9]*)\\)" MATCHES ${VERSION_LINE})

set(MAMBA_VERSION_MAJOR "${CMAKE_MATCH_1}")
set(MAMBA_VERSION_MINOR "${CMAKE_MATCH_2}")
set(MAMBA_VERSION_PATCH "${CMAKE_MATCH_3}")

message("--> mamba version: ${MAMBA_VERSION_MAJOR}.${MAMBA_VERSION_MINOR}.${MAMBA_VERSION_PATCH}")

# Binary version
# See the following URL for explanations about the binary versionning
# https://www.gnu.org/software/libtool/manual/html_node/Updating-version-info.html#Updating-version-info
file(STRINGS "${MAMBA_INCLUDE_DIR}/mamba/version.hpp.in" mamba_version_defines
    REGEX "#define MAMBA_BINARY_(CURRENT|REVISION|AGE)")
foreach(ver ${mamba_version_defines})
    if(ver MATCHES "#define MAMBA_BINARY_(CURRENT|REVISION|AGE) +([^ ]+)$")
        set(MAMBA_BINARY_${CMAKE_MATCH_1} "${CMAKE_MATCH_2}" CACHE INTERNAL "")
    endif()
endforeach()
set(MAMBA_BINARY_VERSION
    ${MAMBA_BINARY_CURRENT}.${MAMBA_BINARY_REVISION}.${MAMBA_BINARY_AGE})
message(STATUS "mamba binary version: v${MAMBA_BINARY_VERSION}")
configure_file(${MAMBA_INCLUDE_DIR}/mamba/version.hpp.in
    ${CMAKE_SOURCE_DIR}/include/mamba/version.hpp @ONLY)

if (LINK_STATIC)

    set(MAMBA_STATIC_LIBS
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
    )

    if (APPLE)
        set (MAMBA_FORCE_DYNAMIC_LIBS resolv)
        set(FINAL_LIBS "")
        foreach(LIB ${MAMBA_STATIC_LIBS})
            set(TMP_LIB "TMP_LIB-NOTFOUND")
            find_library(TMP_LIB "${LIB}")
            list(APPEND FINAL_LIBS "${TMP_LIB}")
        endforeach(LIB)
        set (MAMBA_STATIC_LIBS "${FINAL_LIBS}")
    elseif (UNIX)
        set(MAMBA_FORCE_DYNAMIC_LIBS rt dl resolv)
    endif()

    if (APPLE)
        add_link_options(-static-libstdc++)
    elseif (UNIX)
        add_link_options(-static-libstdc++ -static-libgcc)
    endif ()
endif()

if (LINK_STATIC)
    message("Found libraries: ${MAMBA_STATIC_LIBS}")
else ()
    message("Found libraries: ${MAMBA_REQUIRED_LIBS}")
endif ()

set(MAMBA_SOURCES
    ${CMAKE_SOURCE_DIR}/src/activation.cpp
    ${CMAKE_SOURCE_DIR}/src/channel.cpp
    ${CMAKE_SOURCE_DIR}/src/context.cpp
    ${CMAKE_SOURCE_DIR}/src/environments_manager.cpp
    ${CMAKE_SOURCE_DIR}/src/fetch.cpp
    ${CMAKE_SOURCE_DIR}/src/transaction_context.cpp
    ${CMAKE_SOURCE_DIR}/src/link.cpp
    ${CMAKE_SOURCE_DIR}/src/history.cpp
    ${CMAKE_SOURCE_DIR}/src/match_spec.cpp
    ${CMAKE_SOURCE_DIR}/src/url.cpp
    ${CMAKE_SOURCE_DIR}/src/output.cpp
    ${CMAKE_SOURCE_DIR}/src/package_handling.cpp
    ${CMAKE_SOURCE_DIR}/src/package_cache.cpp
    ${CMAKE_SOURCE_DIR}/src/pool.cpp
    ${CMAKE_SOURCE_DIR}/src/prefix_data.cpp
    ${CMAKE_SOURCE_DIR}/src/package_info.cpp
    ${CMAKE_SOURCE_DIR}/src/package_paths.cpp
    ${CMAKE_SOURCE_DIR}/src/query.cpp
    ${CMAKE_SOURCE_DIR}/src/repo.cpp
    ${CMAKE_SOURCE_DIR}/src/shell_init.cpp
    ${CMAKE_SOURCE_DIR}/src/solver.cpp
    ${CMAKE_SOURCE_DIR}/src/subdirdata.cpp
    ${CMAKE_SOURCE_DIR}/src/thread_utils.cpp
    ${CMAKE_SOURCE_DIR}/src/transaction.cpp
    ${CMAKE_SOURCE_DIR}/src/util.cpp
    ${CMAKE_SOURCE_DIR}/src/validate.cpp
    ${CMAKE_SOURCE_DIR}/src/version.cpp
    ${CMAKE_SOURCE_DIR}/src/thirdparty/WinReg.hpp
    ${CMAKE_SOURCE_DIR}/src/thirdparty/WinReg.hpp
    ${CMAKE_SOURCE_DIR}/src/thirdparty/WinReg.hpp
)

set(MAMBA_HEADERS
    ${MAMBA_INCLUDE_DIR}/mamba/activation.hpp
    ${MAMBA_INCLUDE_DIR}/mamba/channel.hpp
    ${MAMBA_INCLUDE_DIR}/mamba/context.hpp
    ${MAMBA_INCLUDE_DIR}/mamba/environments_manager.hpp
    ${MAMBA_INCLUDE_DIR}/mamba/fetch.hpp
    ${MAMBA_INCLUDE_DIR}/mamba/transaction_context.hpp
    ${MAMBA_INCLUDE_DIR}/mamba/link.hpp
    ${MAMBA_INCLUDE_DIR}/mamba/history.hpp
    ${MAMBA_INCLUDE_DIR}/mamba/match_spec.hpp
    ${MAMBA_INCLUDE_DIR}/mamba/url.hpp
    ${MAMBA_INCLUDE_DIR}/mamba/output.hpp
    ${MAMBA_INCLUDE_DIR}/mamba/package_handling.hpp
    ${MAMBA_INCLUDE_DIR}/mamba/package_cache.hpp
    ${MAMBA_INCLUDE_DIR}/mamba/pool.hpp
    ${MAMBA_INCLUDE_DIR}/mamba/prefix_data.hpp
    ${MAMBA_INCLUDE_DIR}/mamba/package_info.hpp
    ${MAMBA_INCLUDE_DIR}/mamba/package_paths.hpp
    ${MAMBA_INCLUDE_DIR}/mamba/query.hpp
    ${MAMBA_INCLUDE_DIR}/mamba/repo.hpp
    ${MAMBA_INCLUDE_DIR}/mamba/solver.hpp
    ${MAMBA_INCLUDE_DIR}/mamba/subdirdata.hpp
    ${MAMBA_INCLUDE_DIR}/mamba/thread_utils.hpp
    ${MAMBA_INCLUDE_DIR}/mamba/transaction.hpp
    ${MAMBA_INCLUDE_DIR}/mamba/util.hpp
    ${MAMBA_INCLUDE_DIR}/mamba/validate.hpp
    ${MAMBA_INCLUDE_DIR}/mamba/version.hpp
    ${MAMBA_INCLUDE_DIR}/mamba/thirdparty/filesystem.hpp
)

# Targets and link
# ================

set(mamba_targets "")

add_library(mamba STATIC ${MAMBA_SOURCES} ${MAMBA_HEADERS} ${MAMBA_THIRD_PARTIES})
if (LINK_STATIC)
    target_link_libraries(mamba PUBLIC "${MAMBA_STATIC_LIBS}" ${MAMBA_FORCE_DYNAMIC_LIBS} nlohmann_json::nlohmann_json Threads::Threads)
    list(APPEND mamba_targets mamba)
else()
    target_link_libraries(mamba PUBLIC ${MAMBA_REQUIRED_LIBS} Threads::Threads)
    list(APPEND mamba_targets mamba)
endif()

    # Output
    # ======

    target_include_directories(
        mamba
        PUBLIC $<BUILD_INTERFACE:${MAMBA_INCLUDE_DIR}>
        $<INSTALL_INTERFACE:include>
    )

    set_target_properties(
        mamba
        PROPERTIES
        COMPILE_DEFINITIONS "MAMBA_EXPORTS"
        PREFIX ""
        VERSION ${MAMBA_BINARY_VERSION}
        SOVERSION ${MAMBA_BINARY_CURRENT}
        OUTPUT_NAME "libmamba"
    )

target_link_libraries(mamba PUBLIC ${CMAKE_THREAD_LIBS_INIT})

if (BUILD_BINDINGS)
    find_package(pybind11 REQUIRED)

    pybind11_add_module(mamba_api
        src/py_interface.cpp
    )
    target_link_libraries(mamba_api PUBLIC pybind11::pybind11 mamba)
    set_property(TARGET mamba_api PROPERTY CXX_STANDARD 17)

    install(TARGETS mamba_api
            LIBRARY DESTINATION ${CMAKE_SOURCE_DIR}/mamba/)
endif()

if (BUILD_EXE)
    set(MAMBA_EXE
        ${CMAKE_SOURCE_DIR}/src/main.cpp
    )

    add_executable(micromamba ${MAMBA_EXE})
    target_link_libraries(micromamba PUBLIC mamba)

    install(TARGETS micromamba)
endif()

# Tests
# =====

option(ENABLE_TESTS "Enable C++ tests for mamba" OFF)

if (ENABLE_TESTS)
    add_subdirectory(test)
endif()

# Installation
# ============

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(MAMBA_CMAKECONFIG_INSTALL_DIR "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}" CACHE STRING "install path for mambaConfig.cmake")

install(TARGETS ${mamba_targets}
        EXPORT ${PROJECT_NAME}-targets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# Makes the project importable from the build directory
export(EXPORT ${PROJECT_NAME}-targets
       FILE "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Targets.cmake")

# Headers from third party dependency should not be in the public API
# Running install directory because PUBLIC_HEADER DESTINATION does not keep the directory structure (it flattens everything)
install(DIRECTORY ${MAMBA_INCLUDE_DIR} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

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
                              INSTALL_DESTINATION ${MAMBA_CMAKECONFIG_INSTALL_DIR})


write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake
                                 VERSION ${MAMBA_VERSION_MAJOR}.${MAMBA_VERSION_MINOR}.${MAMBA_VERSION_PATCH}
                                 COMPATIBILITY AnyNewerVersion)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/${PROJECT_NAME}Config.cmake
              ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake
              DESTINATION ${MAMBA_CMAKECONFIG_INSTALL_DIR})
install(EXPORT ${PROJECT_NAME}-targets
        FILE ${PROJECT_NAME}Targets.cmake
        DESTINATION ${MAMBA_CMAKECONFIG_INSTALL_DIR})
