#include "solver.hpp"

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

PYBIND11_MODULE(mamba_api, m) {
    m.def("solve", &solve);

    m.attr("SOLVER_ERASE")   = SOLVER_ERASE;
    m.attr("SOLVER_INSTALL") = SOLVER_INSTALL;
    m.attr("SOLVER_UPDATE")  = SOLVER_UPDATE;
}