// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <pybind11/operators.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl_bind.h>

#include "mamba/solver/request.hpp"

#include "bindings.hpp"
#include "utils.hpp"

namespace mamba::solver
{
    // Fix Pybind11 py::bind_vector<Request::item_list> has trouble detecting the abscence
    // of comparions operators, so we tell it explicitly.
    auto operator==(const Request::Item&, const Request::Item&) -> bool = delete;
    auto operator!=(const Request::Item&, const Request::Item&) -> bool = delete;
}

PYBIND11_MAKE_OPAQUE(mamba::solver::Request::item_list);

namespace mambapy
{
    void bind_submodule_solver(pybind11::module_ m)
    {
        namespace py = pybind11;
        using namespace mamba;
        using namespace mamba::solver;

        auto py_request = py::class_<Request>(m, "Request");

        py::class_<Request::Install>(py_request, "Install")
            .def(
                py::init(
                    [](specs::MatchSpec spec) -> Request::Install { return { std::move(spec) }; }
                ),
                py::arg("spec")
            )
            .def_readwrite("spec", &Request::Install::spec)
            .def("__copy__", &copy<Request::Install>)
            .def("__deepcopy__", &deepcopy<Request::Install>, py::arg("memo"));

        py::class_<Request::Remove>(py_request, "Remove")
            .def(
                py::init(
                    [](specs::MatchSpec spec, bool clean) -> Request::Remove {
                        return { std::move(spec), clean };
                    }
                ),
                py::arg("spec"),
                py::arg("clean_dependencies") = true
            )
            .def_readwrite("spec", &Request::Remove::spec)
            .def_readwrite("clean_dependencies", &Request::Remove::clean_dependencies)
            .def("__copy__", &copy<Request::Remove>)
            .def("__deepcopy__", &deepcopy<Request::Remove>, py::arg("memo"));

        py::class_<Request::Update>(py_request, "Update")
            .def(
                py::init([](specs::MatchSpec spec) -> Request::Update { return { std::move(spec) }; }),
                py::arg("spec")
            )
            .def_readwrite("spec", &Request::Update::spec)
            .def("__copy__", &copy<Request::Update>)
            .def("__deepcopy__", &deepcopy<Request::Update>, py::arg("memo"));

        py::class_<Request::UpdateAll>(py_request, "UpdateAll")
            .def(
                py::init([](bool clean) -> Request::UpdateAll { return { clean }; }),
                py::arg("clean_dependencies") = true
            )
            .def_readwrite("clean_dependencies", &Request::UpdateAll::clean_dependencies)
            .def("__copy__", &copy<Request::UpdateAll>)
            .def("__deepcopy__", &deepcopy<Request::UpdateAll>, py::arg("memo"));

        py::class_<Request::Keep>(py_request, "Keep")
            .def(
                py::init([](specs::MatchSpec spec) -> Request::Keep { return { std::move(spec) }; }),
                py::arg("spec")
            )
            .def_readwrite("spec", &Request::Keep::spec)
            .def("__copy__", &copy<Request::Keep>)
            .def("__deepcopy__", &deepcopy<Request::Keep>, py::arg("memo"));

        py::class_<Request::Freeze>(py_request, "Freeze")
            .def(
                py::init([](specs::MatchSpec spec) -> Request::Freeze { return { std::move(spec) }; }),
                py::arg("spec")
            )
            .def_readwrite("spec", &Request::Freeze::spec)
            .def("__copy__", &copy<Request::Freeze>)
            .def("__deepcopy__", &deepcopy<Request::Freeze>, py::arg("memo"));

        py::class_<Request::Pin>(py_request, "Pin")
            .def(
                py::init([](specs::MatchSpec spec) -> Request::Pin { return { std::move(spec) }; }),
                py::arg("spec")
            )
            .def_readwrite("spec", &Request::Pin::spec)
            .def("__copy__", &copy<Request::Pin>)
            .def("__deepcopy__", &deepcopy<Request::Pin>, py::arg("memo"));

        // Type made opaque at the top of this file
        py::bind_vector<Request::item_list>(py_request, "ItemList");

        py::class_<Request::Flags>(py_request, "Flags")
            .def(
                py::init(
                    [](bool keep_dependencies,
                       bool keep_user_specs,
                       bool force_reinstall,
                       bool allow_downgrade,
                       bool allow_uninstall,
                       bool strict_repo_priority) -> Request::Flags
                    {
                        return {
                            /* .keep_dependencies= */ keep_dependencies,
                            /* .keep_user_specs= */ keep_user_specs,
                            /* .force_reinstall= */ force_reinstall,
                            /* .allow_downgrade= */ allow_downgrade,
                            /* .allow_uninstall= */ allow_uninstall,
                            /* .strict_repo_priority= */ strict_repo_priority,
                        };
                    }
                ),
                py::arg("keep_dependencies") = true,
                py::arg("keep_user_specs") = true,
                py::arg("force_reinstall") = false,
                py::arg("allow_downgrade") = true,
                py::arg("allow_uninstall") = true,
                py::arg("strict_repo_priority") = true
            )
            .def_readwrite("keep_dependencies", &Request::Flags::keep_dependencies)
            .def_readwrite("keep_user_specs", &Request::Flags::keep_user_specs)
            .def_readwrite("force_reinstall", &Request::Flags::force_reinstall)
            .def_readwrite("allow_downgrade", &Request::Flags::allow_downgrade)
            .def_readwrite("allow_uninstall", &Request::Flags::allow_uninstall)
            .def_readwrite("strict_repo_priority", &Request::Flags::strict_repo_priority)
            .def("__copy__", &copy<Request::Flags>)
            .def("__deepcopy__", &deepcopy<Request::Flags>, py::arg("memo"));

        py_request
            .def(
                // Big copy unfortunately
                py::init(
                    [](Request::Flags flags, Request::item_list items) -> Request {
                        return { std::move(flags), std::move(items) };
                    }
                )
            )
            .def(py::init(
                [](py::iterable items) -> Request
                {
                    auto request = Request();
                    request.items.reserve(py::len_hint(items));
                    for (py::handle itm : items)
                    {
                        request.items.push_back(py::cast<Request::Item>(itm));
                    }
                    return request;
                }
            ))
            .def_readwrite("flags", &Request::flags)
            .def_readwrite("items", &Request::items)
            .def("__copy__", &copy<Request>)
            .def("__deepcopy__", &deepcopy<Request>, py::arg("memo"));

        ;
    }
}
