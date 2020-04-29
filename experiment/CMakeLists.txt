cmake_minimum_required (VERSION 2.8.11)
project (mamba_experiments)

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

include_directories(
	../include
	../include/thirdparty
	${pybind11_INCLUDE_DIRS}
	/home/mariana/.conda/envs/mamba/include
	/home/mariana/.conda/envs/mamba/include/python3.8
)

file(GLOB MAMBA_SOURCES ../src/*.cpp)

add_library(mamba_lib ${MAMBA_SOURCES})
target_link_libraries(mamba_lib PUBLIC ${MAMBA_REQUIRED_LIBS})

add_executable(test experiment.cpp)
target_link_libraries(test mamba_lib)