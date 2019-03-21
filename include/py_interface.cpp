#include "solver.hpp"

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

PYBIND11_MODULE(mamba_api, m) {
    m.def("solve", &solve);
}