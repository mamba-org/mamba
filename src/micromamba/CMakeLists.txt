set(MAMBA_EXE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/micromamba/clean.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/micromamba/config.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/micromamba/completer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/micromamba/constructor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/micromamba/create.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/micromamba/info.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/micromamba/install.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/micromamba/list.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/micromamba/common_options.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/micromamba/main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/micromamba/remove.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/micromamba/shell.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/micromamba/umamba.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/micromamba/update.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/micromamba/env.cpp
)

set(MAMBA_EXE_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/micromamba/common_options.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/micromamba/umamba.hpp
)

add_executable(micromamba ${MAMBA_EXE} ${MAMBA_EXE_HEADERS})

target_include_directories(
    micromamba
    PRIVATE $<BUILD_INTERFACE:${MAMBA_INCLUDE_DIR}>
)

target_link_libraries(micromamba PRIVATE mamba-static)

message("Using static dependencies for micromamba: ${STATIC_DEPENDENCIES}")

if (STATIC_DEPENDENCIES AND UNIX)

    set(MAMBA_DEPENDENCIES_LIBS
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

    if (BUILD_CRYPTO_PACKAGE_VALIDATION)
        list(APPEND MAMBA_DEPENDENCIES_LIBS "libsodium.a")
    endif()

    set(FINAL_LIBS "")
    foreach(LIB ${MAMBA_DEPENDENCIES_LIBS})
        set(TMP_LIB "TMP_LIB-NOTFOUND")
        find_library(TMP_LIB
            NAMES "${LIB}"
        )
        list(APPEND FINAL_LIBS "${TMP_LIB}")
    endforeach(LIB)

    if (APPLE)
        find_library(SECURITY_LIBRARY Security)
        find_library(COREFOUNDATION_LIBRARY CoreFoundation)
        message("Found library: ${SECURITY_LIBRARY}\n${COREFOUNDATION_LIBRARY}")
        list(APPEND FINAL_LIBS ${COREFOUNDATION_LIBRARY} ${SECURITY_LIBRARY})
    endif()

    message("  Found static dependencies:")
    foreach(LIB ${FINAL_LIBS})
        message("   - ${LIB}")
    endforeach(LIB)

    if (APPLE)
        set (MAMBA_FORCE_DYNAMIC_LIBS resolv)
        target_link_options(micromamba PRIVATE -static-libstdc++)
    elseif (UNIX)
        set(MAMBA_FORCE_DYNAMIC_LIBS rt dl resolv)
        target_link_options(micromamba PRIVATE -static-libstdc++ -static-libgcc)
    endif()

    target_link_libraries(micromamba PRIVATE ${FINAL_LIBS} ${MAMBA_FORCE_DYNAMIC_LIBS})

elseif (STATIC_DEPENDENCIES AND WIN32)

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

    find_library(LIBSOLV_STATIC_LIBRARIES NAMES solv_static)
    find_library(LIBSOLVEXT_STATIC_LIBRARIES NAMES solvext_static)
    find_package(reproc++ CONFIG REQUIRED)

    if (BUILD_CRYPTO_PACKAGE_VALIDATION)
        find_package(Sodium REQUIRED)
    endif()

    find_library(LIBXML2_LIBRARY NAMES libxml2)
    find_library(ICONV_LIBRARY NAMES libiconv)
    find_library(CHARSET_LIBRARY NAMES libcharset)
    message("Found: ${LIBXML2_LIBRARY} ${ICONV_LIBRARY} ${CHARSET_LIBRARY}")

    target_link_libraries(micromamba PRIVATE
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
        ${LIBSOLV_STATIC_LIBRARIES}
        ${LIBSOLVEXT_STATIC_LIBRARIES}
        ${sodium_LIBRARY_RELEASE}
        reproc++
    )

    add_definitions("-DLIBARCHIVE_STATIC -DCURL_STATICLIB -DSOLV_STATIC_LIB")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT")
    include_directories($ENV{CONDA_PREFIX}/Library/include/)
    include_directories($ENV{VCPKG_ROOT}/installed/x64-windows-static/include/)
endif()

set_property(TARGET micromamba PROPERTY CXX_STANDARD 17)

install(TARGETS micromamba)
