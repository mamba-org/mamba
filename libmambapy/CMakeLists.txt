find_package(pybind11 REQUIRED)

include(GNUInstallDirs)

pybind11_add_module(bindings
    src/main.cpp
)
target_link_libraries(bindings PUBLIC pybind11::pybind11 libmamba-static)
set_property(TARGET bindings PROPERTY CXX_STANDARD 17)

install(TARGETS bindings
        LIBRARY DESTINATION ${CMAKE_CURRENT_SOURCE_DIR}/libmambapy/)
