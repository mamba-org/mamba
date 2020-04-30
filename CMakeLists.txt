cmake_minimum_required (VERSION 2.8.11)
project (mamba)

if (MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /std:c++17 /DNOMINMAX")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --std=c++17 -Og -g -pthread")
endif()
find_package(Threads REQUIRED)
find_library(LIBSOLV_LIBRARIES NAMES solv)
find_library(LIBSOLVEXT_LIBRARIES NAMES solvext)
find_package(CURL REQUIRED)
find_library(ARCHIVE_LIBRARIES NAMES archive)
find_package(OpenSSL REQUIRED)
find_package(nlohmann_json REQUIRED)

set(MAMBA_REQUIRED_LIBS
    ${LIBSOLV_LIBRARIES}
    ${LIBSOLVEXT_LIBRARIES}
    ${ARCHIVE_LIBRARIES}
    ${CURL_LIBRARIES}
    ${OPENSSL_CRYPTO_LIBRARY}
    nlohmann_json::nlohmann_json
)

message("Found libraries: ${MAMBA_REQUIRED_LIBS}")

option(ENABLE_TESTS "Enable C++ tests for mamba" OFF)

find_package(pybind11 REQUIRED)

include_directories(
    include
    include/thirdparty
)

set(MAMBA_SOURCES
    ${CMAKE_SOURCE_DIR}/src/common_write.c
    ${CMAKE_SOURCE_DIR}/src/context.cpp
    ${CMAKE_SOURCE_DIR}/src/fetch.cpp
    # ${CMAKE_SOURCE_DIR}/src/link.cpp
    ${CMAKE_SOURCE_DIR}/src/history.cpp
    ${CMAKE_SOURCE_DIR}/src/match_spec.cpp
    ${CMAKE_SOURCE_DIR}/src/url.cpp
    ${CMAKE_SOURCE_DIR}/src/output.cpp
    ${CMAKE_SOURCE_DIR}/src/package_handling.cpp
    ${CMAKE_SOURCE_DIR}/src/pool.cpp
    ${CMAKE_SOURCE_DIR}/src/prefix_data.cpp
    ${CMAKE_SOURCE_DIR}/src/query.cpp
    ${CMAKE_SOURCE_DIR}/src/repo.cpp
    ${CMAKE_SOURCE_DIR}/src/solver.cpp
    ${CMAKE_SOURCE_DIR}/src/subdirdata.cpp
    ${CMAKE_SOURCE_DIR}/src/transaction.cpp
    ${CMAKE_SOURCE_DIR}/src/util.cpp
    ${CMAKE_SOURCE_DIR}/src/validate.cpp
)

add_library(mamba_api ${MAMBA_SOURCES})
target_link_libraries(mamba_api PUBLIC ${MAMBA_REQUIRED_LIBS} Threads::Threads)
set_property(TARGET mamba_api PROPERTY CXX_STANDARD 17)

pybind11_add_module(py_mamba_api
    src/py_interface.cpp
)
target_link_libraries(py_mamba_api PUBLIC pybind11::pybind11 mamba_api)
set_property(TARGET py_mamba_api PROPERTY CXX_STANDARD 17)

install(TARGETS py_mamba_api
        LIBRARY DESTINATION ${CMAKE_SOURCE_DIR}/mamba/)

if (ENABLE_TESTS)
    add_subdirectory(test)
endif()