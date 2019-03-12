/*
<%
cfg['compiler_args'] = ['-std=c++17', '-march=native']
cfg['libraries'] = ['solv']
cfg['include_dirs'] = ['/home/wolfv/miniconda3/include/']
setup_pybind11(cfg)
%>
*/

#include "include/api.cpp"

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

PYBIND11_MODULE(interface, m) {
    m.def("solve", &solve);
}