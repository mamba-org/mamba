cmake_minimum_required(VERSION 2.8)

project(mamba)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release" 
      CACHE STRING "Build configuration 'Release' or 'Debug'."
      FORCE)
endif()


set(CONDA $ENV{CONDA_PREFIX})

if (CONDA)
    set(CMAKE_INSTALL_LIBDIR lib)
    set(CMAKE_INSTALL_PREFIX ${CONDA})
    set(CMAKE_MODULE_PATH ${CONDA}/share/cmake/Modules)

    # if (WIN32)
    #     set (LS_INSTALL_SUFFIX "Library/")
    # else ()
    #     set (LS_INSTALL_SUFFIX "")
    # endif ()

    find_library(LibSolv_LIBRARIES
        NAMES solv
        HINTS ${CONDA}
        PATHS ${CONDA}
        PATH_SUFFIXES lib
        REQUIRED
    )

    find_path(LibSolv_INCLUDE_DIRS NAME solv/repo.h
        HINTS ${CONDA}
        PATHS ${CONDA}
        PATH_SUFFIXES include
    ) 

    message(STATUS "Found libsolv: ${LibSolv_LIBRARIES}")
    message(STATUS "Found libsolv include: ${LibSolv_INCLUDE_DIRS}")
else ()
    find_package(LibSolv REQUIRED)
endif ()


find_package(pybind11 REQUIRED)

include_directories(thirdparty)
include_directories(${CMAKE_INSTALL_PREFIX}/include)

pybind11_add_module(mamba_api py_interface.cpp solver.cpp parsing.cpp thirdparty/simdjson/simdjson.cpp)

target_link_libraries(mamba_api PRIVATE ${LibSolv_LIBRARIES})

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "GNU" OR (CMAKE_CXX_COMPILER_ID MATCHES "Intel" AND NOT WIN32))
    target_compile_options(mamba_api PUBLIC -std=c++17 -march=core-avx2 -O3)
else ()
    # TODO
    target_compile_options(mamba_api PUBLIC /std:c++17 /arch:AVX2 /Ox)
endif()

set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/../mamba/)