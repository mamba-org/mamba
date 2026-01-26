# Copyright (c) 2019, QuantStack and Mamba Contributors
#
# Distributed under the terms of the BSD 3-Clause License.
#
# The full license is in the file LICENSE, distributed with this software.

# Network component - depends on libmamba-common and libcurl

# These variables should be set by the parent CMakeLists.txt
if(NOT DEFINED LIBMAMBA_SOURCE_DIR)
    set(LIBMAMBA_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../src)
endif()
if(NOT DEFINED LIBMAMBA_INCLUDE_DIR)
    set(LIBMAMBA_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../include)
endif()

set(
    LIBMAMBA_NETWORK_SOURCES
    # URL utilities that need curl
    ${LIBMAMBA_SOURCE_DIR}/util/url.cpp
    # Downloaders and mirrors
    ${LIBMAMBA_SOURCE_DIR}/download/compression.cpp
    ${LIBMAMBA_SOURCE_DIR}/download/curl.cpp
    ${LIBMAMBA_SOURCE_DIR}/download/downloader.cpp
    ${LIBMAMBA_SOURCE_DIR}/download/mirror_impl.cpp
    ${LIBMAMBA_SOURCE_DIR}/download/mirror_map.cpp
    ${LIBMAMBA_SOURCE_DIR}/download/mirror.cpp
    ${LIBMAMBA_SOURCE_DIR}/download/request.cpp
    # Core files that use download
    ${LIBMAMBA_SOURCE_DIR}/core/download_progress_bar.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/subdir_index.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/shard_types.cpp
    # Validation that needs download
    ${LIBMAMBA_SOURCE_DIR}/validation/repo_checker.cpp
    ${LIBMAMBA_SOURCE_DIR}/validation/update_framework_v0_6.cpp
)

set(
    LIBMAMBA_NETWORK_PUBLIC_HEADERS
    # URL utilities
    ${LIBMAMBA_INCLUDE_DIR}/mamba/util/url.hpp
    # Downloaders and mirrors
    ${LIBMAMBA_INCLUDE_DIR}/mamba/download/downloader.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/download/mirror_map.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/download/mirror.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/download/request.hpp
    # Core files
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/download_progress_bar.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/subdir_index.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/shard_types.hpp
    # Note: singletons.hpp is in common but singletons.cpp needs CURL For now, singletons.cpp is in
    # common but it will need CURL dependency Validation
    ${LIBMAMBA_INCLUDE_DIR}/mamba/validation/repo_checker.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/validation/update_framework_v0_6.hpp
)

# Create the network library target
macro(libmamba_network_create_target target_name linkage output_name)
    string(TOUPPER "${linkage}" linkage_upper)
    if(NOT ${linkage_upper} MATCHES "^(SHARED|STATIC)$")
        message(FATAL_ERROR "Invalid library linkage: ${linkage}")
    endif()

    add_library(
        ${target_name}
        ${linkage_upper} ${LIBMAMBA_NETWORK_PUBLIC_HEADERS} ${LIBMAMBA_NETWORK_SOURCES}
    )

    target_include_directories(
        ${target_name}
        PUBLIC $<BUILD_INTERFACE:${LIBMAMBA_INCLUDE_DIR}> $<INSTALL_INTERFACE:include>
        PRIVATE ${LIBMAMBA_SOURCE_DIR}
    )

    target_compile_features(${target_name} PUBLIC cxx_std_20)
    set_target_properties(
        ${target_name}
        PROPERTIES
            CXX_STANDARD 20
            CXX_STANDARD_REQUIRED YES
            CXX_EXTENSIONS NO
    )

    mamba_target_add_compile_warnings(${target_name} WARNING_AS_ERROR ${MAMBA_WARNING_AS_ERROR})
    mamba_target_set_lto(${target_name} MODE ${MAMBA_LTO})

    # Depend on common component
    if(TARGET mamba::libmamba-common-dyn)
        target_link_libraries(${target_name} PUBLIC mamba::libmamba-common-dyn)
    elseif(TARGET mamba::libmamba-common-static)
        target_link_libraries(${target_name} PUBLIC mamba::libmamba-common-static)
    else()
        message(FATAL_ERROR "libmamba-common must be built before libmamba-network")
    endif()
    # Note: Archive dependency will be added after archive is built (in main CMakeLists.txt)

    if(${linkage_upper} STREQUAL "STATIC")
        if(UNIX)
            set(
                REQUIRED_STATIC_DEPS
                libcurl.a
                libssh2.a
                libgssapi_krb5.a
                libkrb5.a
                libk5crypto.a
                libkrb5support.a
                libcom_err.a
                libssl.a
                libcrypto.a
                libnghttp2.a
            )
            if(APPLE)
                set(REQUIRED_STATIC_DEPS ${REQUIRED_STATIC_DEPS} libc++.a)
            endif()

            if(UNIX AND NOT APPLE)
                list(REMOVE_ITEM REQUIRED_STATIC_DEPS libiconv.a)
            endif()

            set(STATIC_DEPS "")
            foreach(LIB ${REQUIRED_STATIC_DEPS})
                set(TMP_LIB "${LIB}-NOTFOUND")
                find_library(TMP_LIB NAMES "${LIB}")
                if(NOT ${TMP_LIB} STREQUAL "TMP_LIB-NOTFOUND")
                    list(APPEND STATIC_DEPS "${TMP_LIB}")
                else()
                    list(APPEND STATIC_DEPS "${LIB}-NOTFOUND")
                endif()
            endforeach(LIB)

            if(APPLE)
                find_library(SECURITY_LIBRARY Security)
                find_library(SYSTEMCONFIGURATION_LIBRARY SystemConfiguration)
                find_library(COREFOUNDATION_LIBRARY CoreFoundation)
                list(
                    APPEND
                    STATIC_DEPS
                    ${COREFOUNDATION_LIBRARY}
                    ${SECURITY_LIBRARY}
                    ${SYSTEMCONFIGURATION_LIBRARY}
                )
            endif()

            target_link_libraries(${target_name} PUBLIC ${STATIC_DEPS})

            if(APPLE)
                set(MAMBA_FORCE_DYNAMIC_LIBS resolv c++abi)
                target_link_options(${target_name} PRIVATE -nostdlib++)
            elseif(UNIX)
                set(MAMBA_FORCE_DYNAMIC_LIBS rt dl resolv)
                target_link_options(${target_name} PUBLIC -static-libstdc++ -static-libgcc)
            endif()

            target_link_libraries(${target_name} PUBLIC ${MAMBA_FORCE_DYNAMIC_LIBS})

        elseif(WIN32)
            set(CMAKE_PREFIX_PATH "$ENV{VCPKG_ROOT}/installed/x64-windows-static-md/")
            find_package(CURL CONFIG REQUIRED)
            target_link_libraries(${target_name} PUBLIC CURL::libcurl)
            add_compile_definitions(CURL_STATICLIB)
        endif()
    else()
        find_package(CURL REQUIRED)
        find_package(OpenSSL REQUIRED)
        find_package(zstd REQUIRED)
        find_package(BZip2 REQUIRED)

        target_link_libraries(
            ${target_name}
            PUBLIC ${CURL_LIBRARIES} ${OPENSSL_LIBRARIES} BZip2::BZip2 zstd::libzstd_shared
        )
    endif()

    if(UNIX)
        math(EXPR LIBMAMBA_BINARY_COMPATIBLE "${LIBMAMBA_BINARY_CURRENT} - ${LIBMAMBA_BINARY_AGE}")
        set_target_properties(
            ${target_name}
            PROPERTIES
                COMPILE_DEFINITIONS "LIBMAMBA_NETWORK_EXPORTS"
                PREFIX ""
                VERSION
                "${LIBMAMBA_BINARY_COMPATIBLE}.${LIBMAMBA_BINARY_REVISION}.${LIBMAMBA_BINARY_AGE}"
                SOVERSION ${LIBMAMBA_BINARY_COMPATIBLE}
                OUTPUT_NAME "${output_name}"
        )
    else()
        set_target_properties(
            ${target_name}
            PROPERTIES
                COMPILE_DEFINITIONS "LIBMAMBA_NETWORK_EXPORTS"
                PREFIX ""
                VERSION ${LIBMAMBA_BINARY_VERSION}
                SOVERSION ${LIBMAMBA_BINARY_CURRENT}
                OUTPUT_NAME "${output_name}"
        )
    endif()

    list(APPEND libmamba_network_targets ${target_name})
    add_library(mamba::${target_name} ALIAS ${target_name})
endmacro()

set(libmamba_network_targets "")
