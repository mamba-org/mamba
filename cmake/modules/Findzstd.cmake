# First, try the package's own config file
find_package(zstd CONFIG QUIET)

if(TARGET zstd::libzstd_shared)
    set(zstd_FOUND TRUE)
    return()
endif()

if(TARGET zstd::libzstd_static)
    set(zstd_FOUND TRUE)
    return()
endif()

# Fallback for systems without zstdConfig.cmake
find_path(ZSTD_INCLUDE_DIR zstd.h)
find_library(ZSTD_LIBRARY NAMES zstd)

if(ZSTD_INCLUDE_DIR AND ZSTD_LIBRARY)
    set(zstd_FOUND TRUE)
    if(NOT TARGET zstd::libzstd_shared)
        add_library(zstd::libzstd_shared UNKNOWN IMPORTED)
        set_target_properties(
            zstd::libzstd_shared
            PROPERTIES IMPORTED_LOCATION \"${ZSTD_LIBRARY}\"
                       INTERFACE_INCLUDE_DIRECTORIES \"${ZSTD_INCLUDE_DIR}\"
        )
    endif()
    if(NOT TARGET zstd::libzstd_static)
        add_library(zstd::libzstd_static UNKNOWN IMPORTED)
        set_target_properties(
            zstd::libzstd_static
            PROPERTIES IMPORTED_LOCATION \"${ZSTD_LIBRARY}\"
                       INTERFACE_INCLUDE_DIRECTORIES \"${ZSTD_INCLUDE_DIR}\"
        )
    endif()
endif()
