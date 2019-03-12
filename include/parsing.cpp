/*
<%
cfg['compiler_args'] = ['-O3', '-std=c++17', '-march=native']
cfg['libraries'] = ['solv']
setup_pybind11(cfg)
%>
*/

#include "parsing.hpp"

#include <pybind11/pybind11.h>

static Pool* gb_pool;

PYBIND11_MODULE(parsing, m) {
    m.def("init", []() {
        Pool* pool = pool_create();
        gb_pool = pool;
    });

    // m.def("normalize_version", (std::string (std::string_view, int)*) &normalize_version);
    // m.def("normalize_version", (std::string (std::string_view, int, std::string_view)*) &normalize_version);
    m.def("parse_to_relation", [](const std::string_view& dep) { parse_to_relation(dep, gb_pool); });
    m.def("test_parse", [](const std::string t) {
        auto id = pool_str2id(gb_pool, "python", 1);
        auto rel_id = parse_version_relation(id, t, gb_pool);
        std::cout << pool_dep2str(gb_pool, rel_id) << std::endl;
        return parse_version_relation(id, t, gb_pool);
    });
    m.def("test_fparse", [](const std::string t) {
        auto rel_id = parse_to_relation(t, gb_pool);
        std::cout << pool_dep2str(gb_pool, rel_id) << std::endl;
    });
}