find_package(PkgConfig)
pkg_check_modules(PC_Libsolv QUIET libsolv)

find_path(Libsolv_INCLUDE_DIR
    NAMES solv/pool.h solvversion.h
    PATHS ${PC_Libsolv_INCLUDE_DIRS}
)

find_path(Libsolvext_INCLUDE_DIR
    NAMES solv/repo_conda.h repo_conda.h
    PATHS ${PC_Libsolv_INCLUDE_DIRS}
)

find_library(Libsolv_LIBRARY
    NAMES libsolv.so libsolv.dylib solv.lib
    PATHS ${PC_Libsolv_LIBRARY_DIRS}
)
find_library(Libsolv_static_LIBRARY
    NAMES libsolv.a solv_static
    PATHS ${PC_Libsolv_LIBRARY_DIRS}
)

find_library(Libsolvext_LIBRARY
    NAMES libsolvext.so libsolvext.dylib solvext.lib
    PATHS ${PC_Libsolv_LIBRARY_DIRS}
)
find_library(Libsolvext_static_LIBRARY
    NAMES libsolvext.a solvext_static
    PATHS ${PC_Libsolv_LIBRARY_DIRS}
)

set(Libsolv_VERSION ${PC_Libsolv_VERSION})

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(
    Libsolv
    FOUND_VAR Libsolv_FOUND
    REQUIRED_VARS Libsolv_INCLUDE_DIR
    VERSION_VAR Libsolv_VERSION
)

if(Libsolv_FOUND)
    if(NOT TARGET solv::libsolv_static AND Libsolv_static_LIBRARY)
        add_library(solv::libsolv_static STATIC IMPORTED)
        set_target_properties(
            solv::libsolv_static PROPERTIES
            IMPORTED_LOCATION "${Libsolv_static_LIBRARY}"
            INTERFACE_COMPILE_OPTIONS "${PC_Libsolv_CFLAGS_OTHER}"
            INTERFACE_INCLUDE_DIRECTORIES "${Libsolv_INCLUDE_DIR}"
        )
    endif()
    if(NOT TARGET solv::libsolv AND Libsolv_LIBRARY)
        if(UNIX)
            add_library(solv::libsolv SHARED IMPORTED)
        else()
            add_library(solv::libsolv UNKNOWN IMPORTED)
        endif()
        set_target_properties(
            solv::libsolv PROPERTIES
            IMPORTED_LOCATION "${Libsolv_LIBRARY}"
            INTERFACE_COMPILE_OPTIONS "${PC_Libsolv_CFLAGS_OTHER}"
            INTERFACE_INCLUDE_DIRECTORIES "${Libsolv_INCLUDE_DIR}"
        )
    endif()

    if(NOT TARGET solv::libsolvext_static AND Libsolvext_static_LIBRARY)
        add_library(solv::libsolvext_static STATIC IMPORTED)
        set_target_properties(
            solv::libsolvext_static PROPERTIES
            IMPORTED_LOCATION "${Libsolvext_static_LIBRARY}"
            INTERFACE_COMPILE_OPTIONS "${PC_Libsolv_CFLAGS_OTHER}"
            INTERFACE_INCLUDE_DIRECTORIES "${Libsolvext_INCLUDE_DIR}"
        )
    endif()
    if(NOT TARGET solv::libsolvext AND Libsolvext_LIBRARY)
        if(UNIX)
            add_library(solv::libsolvext SHARED IMPORTED)
        else()
            add_library(solv::libsolvext UNKNOWN IMPORTED)
        endif()
        set_target_properties(
            solv::libsolvext PROPERTIES
            IMPORTED_LOCATION "${Libsolvext_LIBRARY}"
            INTERFACE_COMPILE_OPTIONS "${PC_Libsolv_CFLAGS_OTHER}"
            INTERFACE_INCLUDE_DIRECTORIES "${Libsolvext_INCLUDE_DIR}"
        )
    endif()
endif(

    )
