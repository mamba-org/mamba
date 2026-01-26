# Copyright (c) 2019, QuantStack and Mamba Contributors
#
# Distributed under the terms of the BSD 3-Clause License.
#
# The full license is in the file LICENSE, distributed with this software.

# Archive component - depends on libmamba-common and libarchive

# These variables should be set by the parent CMakeLists.txt
if(NOT DEFINED LIBMAMBA_SOURCE_DIR)
    set(LIBMAMBA_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../src)
endif()
if(NOT DEFINED LIBMAMBA_INCLUDE_DIR)
    set(LIBMAMBA_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../include)
endif()

set(
    LIBMAMBA_ARCHIVE_SOURCES
    # Archive extraction and package handling
    ${LIBMAMBA_SOURCE_DIR}/core/package_handling.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/package_fetcher.cpp
    ${LIBMAMBA_SOURCE_DIR}/core/package_cache.cpp
)

set(
    LIBMAMBA_ARCHIVE_PUBLIC_HEADERS
    # Archive extraction and package handling
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/package_handling.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/package_fetcher.hpp
    ${LIBMAMBA_INCLUDE_DIR}/mamba/core/package_cache.hpp
)

# Create the archive library target
macro(libmamba_archive_create_target target_name linkage output_name)
    string(TOUPPER "${linkage}" linkage_upper)
    if(NOT ${linkage_upper} MATCHES "^(SHARED|STATIC)$")
        message(FATAL_ERROR "Invalid library linkage: ${linkage}")
    endif()

    add_library(
        ${target_name}
        ${linkage_upper} ${LIBMAMBA_ARCHIVE_PUBLIC_HEADERS} ${LIBMAMBA_ARCHIVE_SOURCES}
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

    # For shared libraries, archive uses symbols from network but can't link to it due to circular
    # dependency (network also needs archive symbols). Allow undefined symbols - they'll be resolved
    # when the aggregated libmamba target links all components together.
    if(${linkage_upper} STREQUAL "SHARED")
        if(APPLE)
            target_link_options(${target_name} PRIVATE -Wl,-undefined,dynamic_lookup)
        elseif(UNIX)
            target_link_options(${target_name} PRIVATE -Wl,--allow-shlib-undefined)
        elseif(WIN32)
            # On Windows, use /FORCE:UNRESOLVED to allow unresolved symbols. The symbols will be
            # resolved at runtime when the DLLs are loaded. The aggregated libmamba target will
            # ensure all DLLs are available together.
            target_link_options(${target_name} PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/FORCE:UNRESOLVED>)
        endif()
    endif()

    # Depend on common component
    if(TARGET mamba::libmamba-common-dyn)
        target_link_libraries(${target_name} PUBLIC mamba::libmamba-common-dyn)
    elseif(TARGET mamba::libmamba-common-static)
        target_link_libraries(${target_name} PUBLIC mamba::libmamba-common-static)
    else()
        message(FATAL_ERROR "libmamba-common must be built before libmamba-archive")
    endif()

    # Depend on network component (package_fetcher uses download::Request, package_cache uses
    # util::URL) Note: Archive is built after network, so this link will be added in main
    # CMakeLists.txt

    if(${linkage_upper} STREQUAL "STATIC")
        if(UNIX)
            set(
                REQUIRED_STATIC_DEPS
                libarchive.a
                libiconv.a
                libbz2.a
                liblz4.a
                libzstd.a
                libz.a
                liblzma.a
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

            # reproc is needed for package_handling.cpp
            target_link_libraries(${target_name} PUBLIC reproc reproc++)

        elseif(WIN32)
            set(CMAKE_PREFIX_PATH "$ENV{VCPKG_ROOT}/installed/x64-windows-static-md/")
            set(SYSTEM_PROVIDED_LIBRARIES XmlLite.lib)
            set(ENABLE_WIN32_XMLLITE ON)

            find_package(LibArchive MODULE REQUIRED)
            find_library(LIBLZMA_LIBRARIES lzma REQUIRED)
            find_library(LZ4_LIBRARY NAMES lz4)
            find_library(LZO2_LIBRARY NAMES lzo2)
            find_package(zstd CONFIG REQUIRED)
            find_library(BZIP2_LIBRARIES NAMES bz2)
            find_library(CRYPTO_LIBRARIES NAMES libcrypto)
            find_library(LIBXML2_LIBRARY NAMES libxml2)
            find_library(ICONV_LIBRARY NAMES libiconv iconv)
            find_library(CHARSET_LIBRARY NAMES libcharset charset)

            target_link_libraries(
                ${target_name}
                PUBLIC
                    ${CRYPTO_LIBRARIES}
                    ${SYSTEM_PROVIDED_LIBRARIES}
                    ${LibArchive_LIBRARY}
                    ${LIBXML2_LIBRARY}
                    ${ICONV_LIBRARY}
                    ${CHARSET_LIBRARY}
                    zstd::libzstd_static
                    ${LZ4_LIBRARY}
                    ${LZO2_LIBRARY}
                    ${BZIP2_LIBRARIES}
                    ${LIBLZMA_LIBRARIES}
            )

            add_compile_definitions(LIBARCHIVE_STATIC)
            include_directories($ENV{CONDA_PREFIX}/Library/include/)
            include_directories($ENV{VCPKG_ROOT}/installed/x64-windows-static-md/include/)
        endif()
    else()
        find_package(LibArchive REQUIRED)
        find_package(zstd REQUIRED)
        find_package(BZip2 REQUIRED)
        find_package(OpenSSL REQUIRED)

        target_link_libraries(
            ${target_name}
            PUBLIC ${LibArchive_LIBRARIES} BZip2::BZip2 zstd::libzstd_shared ${OPENSSL_LIBRARIES}
        )
        # LibArchive include directories must be PUBLIC so that API sources (e.g., info.cpp) can
        # include <archive.h>
        target_include_directories(${target_name} PUBLIC "${LibArchive_INCLUDE_DIRS}")

        # reproc is needed for package_handling.cpp
        target_link_libraries(${target_name} PUBLIC reproc reproc++)
    endif()

    if(UNIX)
        math(EXPR LIBMAMBA_BINARY_COMPATIBLE "${LIBMAMBA_BINARY_CURRENT} - ${LIBMAMBA_BINARY_AGE}")
        set_target_properties(
            ${target_name}
            PROPERTIES
                COMPILE_DEFINITIONS "LIBMAMBA_ARCHIVE_EXPORTS"
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
                COMPILE_DEFINITIONS "LIBMAMBA_ARCHIVE_EXPORTS"
                PREFIX ""
                VERSION ${LIBMAMBA_BINARY_VERSION}
                SOVERSION ${LIBMAMBA_BINARY_CURRENT}
                OUTPUT_NAME "${output_name}"
        )
    endif()

    list(APPEND libmamba_archive_targets ${target_name})
    add_library(mamba::${target_name} ALIAS ${target_name})
endmacro()

set(libmamba_archive_targets "")
