cmake_minimum_required (VERSION 2.8.11)
project (mamba)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --std=c++17 -Og -g -pthread")

find_package(Threads REQUIRED)
find_library(LIBSOLV_LIBRARIES NAMES solv)
find_library(LIBSOLVEXT_LIBRARIES NAMES solvext)
find_package(CURL REQUIRED)
find_library(ARCHIVE_LIBRARIES NAMES archive)
find_package(OpenSSL REQUIRED)

set(MAMBA_REQUIRED_LIBS
	${LIBSOLV_LIBRARIES}
	${LIBSOLVEXT_LIBRARIES}
	${ARCHIVE_LIBRARIES}
	${CURL_LIBRARIES}
	${OPENSSL_CRYPTO_LIBRARY}
)

find_package(pybind11 REQUIRED)

include_directories(
	include
	include/thirdparty
	${pybind11_INCLUDE_DIRS}
)

file(GLOB MAMBA_SOURCES src/*.cpp)

pybind11_add_module(mamba_api
	src/py_interface.cpp
	${MAMBA_SOURCES}
)

target_link_libraries(mamba_api PUBLIC ${MAMBA_REQUIRED_LIBS} ${PYTHON_LIBRARY} Threads::Threads)
set_property(TARGET mamba_api PROPERTY CXX_STANDARD 17)

install(TARGETS mamba_api
		LIBRARY DESTINATION ${CMAKE_SOURCE_DIR}/mamba/)