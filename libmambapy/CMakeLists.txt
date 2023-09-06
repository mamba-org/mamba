# Copyright (c) 2019, QuantStack and Mamba Contributors
#
# Distributed under the terms of the BSD 3-Clause License.
#
# The full license is in the file LICENSE, distributed with this software.
cmake_minimum_required(VERSION 3.18.2)

cmake_policy(SET CMP0025 NEW)
cmake_policy(SET CMP0077 NEW)
cmake_policy(SET CMP0057 NEW)

project(libmambapy)

if(NOT TARGET libmamba)
    find_package(libmamba REQUIRED)
endif ()

find_package(Python COMPONENTS Interpreter Development)
find_package(pybind11 REQUIRED)

pybind11_add_module(bindings
    src/main.cpp
    longpath.manifest
)

mamba_target_add_compile_warnings(bindings WARNING_AS_ERROR ${MAMBA_WARNING_AS_ERROR})

target_link_libraries(bindings PUBLIC pybind11::pybind11 libmamba)
target_compile_features(bindings PUBLIC cxx_std_17)

set(
    PY_FILES
    "libmambapy/__init__.py"
    "libmambapy/_version.py"
)
set(PY_SOURCE_FILES ${PY_FILES})
list(TRANSFORM PY_SOURCE_FILES PREPEND "${CMAKE_CURRENT_SOURCE_DIR}/")
set(PY_BINARY_FILES ${PY_FILES})
list(TRANSFORM PY_BINARY_FILES PREPEND "${CMAKE_CURRENT_BINARY_DIR}/")

add_custom_command(
    TARGET bindings
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${PY_SOURCE_FILES} "${CMAKE_CURRENT_BINARY_DIR}/"
    BYPRODUCTS ${PY_BINARY_FILES}
)


get_filename_component(STUBGEN_RUN_DIR "${CMAKE_CURRENT_BINARY_DIR}" DIRECTORY)  # Parent dir
add_custom_target(
    stubgen
    COMMAND Python::Interpreter -m pybind11_stubgen
        -o "${STUBGEN_RUN_DIR}"
        --exit-code
        libmambapy.bindings
    WORKING_DIRECTORY "${STUBGEN_RUN_DIR}"
    COMMENT "Generating PyBind11 Stubs"
    BYPRODUCTS "${CMAKE_CURRENT_BINARY_DIR}/bindings.pyi"
)
add_dependencies(stubgen bindings)

install(TARGETS bindings LIBRARY DESTINATION ${CMAKE_CURRENT_SOURCE_DIR}/libmambapy/)
install(
    FILES "${CMAKE_CURRENT_BINARY_DIR}/bindings.pyi"
    DESTINATION ${CMAKE_CURRENT_SOURCE_DIR}/libmambapy/
    COMPONENT PythonBindingsStubgen
    EXCLUDE_FROM_ALL
)
