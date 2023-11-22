// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <memory>

#include <pybind11/pybind11.h>

#include "mamba/specs/channel_spec.hpp"

#include "bindings.hpp"
#include "flat_set_caster.hpp"


namespace mambapy
{
    template <typename T>
    auto copy(const T& x) -> std::unique_ptr<T>
    {
        return std::make_unique<T>(x);
    }

    template <typename T>
    auto deepcopy(const T& x, const pybind11::dict& /* memo */) -> std::unique_ptr<T>
    {
        return std::make_unique<T>(x);
    }

    void bind_submodule_specs(pybind11::module_ m)
    {
        namespace py = pybind11;
        using namespace mamba::specs;

        auto py_channel_spec = py::class_<ChannelSpec>(m, "ChannelSpec");

        py::enum_<ChannelSpec::Type>(py_channel_spec, "Type")
            .value("URL", ChannelSpec::Type::URL)
            .value("PackageURL", ChannelSpec::Type::PackageURL)
            .value("Path", ChannelSpec::Type::Path)
            .value("PackagePath", ChannelSpec::Type::PackagePath)
            .value("Name", ChannelSpec::Type::Name)
            .value("Unknown", ChannelSpec::Type::Unknown);

        py_channel_spec  //
            .def_static("parse", ChannelSpec::parse)
            .def(
                py::init<std::string, ChannelSpec::dynamic_platform_set, ChannelSpec::Type>(),
                py::arg("location"),
                py::arg("platform_filters"),
                py::arg("type") = ChannelSpec::Type::Unknown
            )
            .def("__copy__", &copy<ChannelSpec>)
            .def("__deepcopy__", &deepcopy<ChannelSpec>, pybind11::arg("memo"))
            .def_property("type", &ChannelSpec::type, &ChannelSpec::set_type)
            .def_property("location", &ChannelSpec::location, &ChannelSpec::set_location)
            .def_property(
                "platform_filters",
                &ChannelSpec::platform_filters,
                &ChannelSpec::set_platform_filters
            );
    }
}
