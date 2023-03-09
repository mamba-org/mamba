# Copyright (c) 2019, QuantStack and Mamba Contributors
#
# Distributed under the terms of the BSD 3-Clause License.
#
# The full license is in the file LICENSE, distributed with this software.

cmake_minimum_required (VERSION 3.16)
cmake_policy(SET CMP0025 NEW) # Introduced in cmake 3.0
cmake_policy(SET CMP0077 NEW) # Introduced in cmake 3.13
project(micromamba)

# Build options
# =============
option(BUILD_MICROMAMBA_SERVER "Build the micromamba server" OFF)

if (BUILD_STATIC AND NOT BUILD_SHARED)
    set(MICROMAMBA_LINKAGE "STATIC" CACHE STRING "micromamba linkage against libraries")
else ()
    set(MICROMAMBA_LINKAGE "DYNAMIC" CACHE STRING "micromamba linkage against libraries")
endif ()


option(ENABLE_ASAN "Enable Address-Sanitizer (currently only supported on GCC and Clang)" OFF)

if(ENABLE_ASAN)
    message(WARNING "Address-Sanitizer instrumentation will be injected into binaries - do not release these binaries")
    add_compile_options(-fno-omit-frame-pointer  -fsanitize=address)
    add_link_options(-fno-omit-frame-pointer  -fsanitize=address)
    if(BUILD_STATIC)
        # This is necessary because add_link_options() only pass flags to linker, not static cases
        set(STATIC_LIBRARY_FLAGS ${STATIC_LIBRARY_FLAGS} -fsanitize=address)
    endif()

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

set(MICROMAMBA_SRCS
    longpath.manifest
    ${CMAKE_CURRENT_SOURCE_DIR}/src/activate.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/clean.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/config.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/completer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/constructor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/create.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/env.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/info.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/install.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/list.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/login.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/common_options.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/package.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/remove.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/repoquery.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/run.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/shell.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/umamba.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/update.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/version.cpp
)

set(MICROMAMBA_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/common_options.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/umamba.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/version.hpp
)

if(UNIX AND BUILD_MICROMAMBA_SERVER)
    list(APPEND MICROMAMBA_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/src/server.cpp
    )
    add_definitions(-DMICROMAMBA_SERVER)
endif()

add_executable(micromamba ${MICROMAMBA_SRCS} ${MICROMAMBA_HEADERS})

mamba_target_add_compile_warnings(micromamba WARNING_AS_ERROR ${MAMBA_WARNING_AS_ERROR})

if(NOT (TARGET libmamba OR TARGET libmamba-static))
    find_package(libmamba REQUIRED)
endif ()

find_package(Threads REQUIRED)
target_link_libraries(micromamba PRIVATE Threads::Threads)

string(TOUPPER "${MICROMAMBA_LINKAGE}" linkage_upper)
message(STATUS "Micromamba linkage: ${MICROMAMBA_LINKAGE}")

if (${MICROMAMBA_LINKAGE} STREQUAL "STATIC")
    target_link_libraries(micromamba PRIVATE libmamba-static)
    if (WIN32)
        set_target_properties(micromamba PROPERTIES MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    endif ()
else ()
    target_link_libraries(micromamba PRIVATE libmamba)
endif ()

set_property(TARGET micromamba PROPERTY CXX_STANDARD 17)

install(TARGETS micromamba)
