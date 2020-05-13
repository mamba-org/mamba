cmake_minimum_required (VERSION 2.8.11)
project (mamba)

if (MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /DNOMINMAX")
endif()

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
    ${CMAKE_SOURCE_DIR}/src/query.cpp
    ${CMAKE_SOURCE_DIR}/src/repo.cpp
    ${CMAKE_SOURCE_DIR}/src/solver.cpp
    ${CMAKE_SOURCE_DIR}/src/subdirdata.cpp
    ${CMAKE_SOURCE_DIR}/src/transaction.cpp
    ${CMAKE_SOURCE_DIR}/src/util.cpp
    ${CMAKE_SOURCE_DIR}/src/validate.cpp
)

add_library(mamba_lib ${MAMBA_SOURCES})
target_link_libraries(mamba_lib PUBLIC ${MAMBA_REQUIRED_LIBS} Threads::Threads)
set_property(TARGET mamba_lib PROPERTY CXX_STANDARD 17)

pybind11_add_module(mamba_api
    src/py_interface.cpp
)
target_link_libraries(mamba_api PUBLIC pybind11::pybind11 mamba_lib)
set_property(TARGET mamba_api PROPERTY CXX_STANDARD 17)

option(BUILD_EXE OFF)

if (BUILD_EXE)
    set(MAMBA_EXE
        ${CMAKE_SOURCE_DIR}/src/main.cpp
    )

    add_executable(micromamba ${MAMBA_EXE})
    target_link_libraries(micromamba PUBLIC mamba_lib)
endif()

install(TARGETS mamba_api
        LIBRARY DESTINATION ${CMAKE_SOURCE_DIR}/mamba/)

if (ENABLE_TESTS)
    add_subdirectory(test)
endif()
