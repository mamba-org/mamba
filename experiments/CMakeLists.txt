cmake_minimum_required(VERSION 3.2)

project(testxxx)

set(CMAKE_CXX_FLAGS -std=c++17)

set(CMAKE_BUILD_TYPE "Debug")

find_package(PkgConfig REQUIRED)
find_package(OpenSSL REQUIRED)
# find_package(skyr-url REQUIRED)
pkg_search_module(GLIB REQUIRED glib-2.0 HINTS )

find_library(REPO_LIBRARIES 
	NAMES repo)

# set(CMAKE_SKIP_BUILD_RPATH FALSE)
# set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
# set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")
# set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
# set(CMAKE_MACOSX_RPATH OFF)

add_executable(subi subdirdata.cpp)
set_target_properties(subi 
  PROPERTIES
	MACOSX_RPATH ON)
target_include_directories(subi PRIVATE ${GLIB_INCLUDE_DIRS})
target_link_libraries(subi PUBLIC ${GLIB_LINK_LIBRARIES})

target_link_libraries(subi PUBLIC ${REPO_LIBRARIES} ${OPENSSL_LIBRARIES} /home/wolfv/miniconda3/lib/libskyr-url.so)