// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <memory>

#include <pybind11/operators.h>
#include <pybind11/pybind11.h>

#include "mamba/specs/channel_spec.hpp"
#include "mamba/specs/conda_url.hpp"
#include "mamba/specs/platform.hpp"

#include "bindings.hpp"
#include "flat_set_caster.hpp"


namespace mambapy
{

    template <typename Enum>
    auto enum_from_str(const pybind11::str& name)
    {
        auto pyenum = pybind11::type::of<Enum>();
        return pyenum.attr("__members__")[name].template cast<Enum>();
    }

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

    template <typename T>
    auto hash(const T& x) -> std::size_t
    {
        return std::hash<T>()(x);
    }

    void bind_submodule_specs(pybind11::module_ m)
    {
        namespace py = pybind11;
        using namespace mamba::specs;

        py::enum_<Platform>(m, "Platform")
            .value("noarch", Platform::noarch)
            .value("linux_32", Platform::linux_32)
            .value("linux_64", Platform::linux_64)
            .value("linux_armv6l", Platform::linux_armv6l)
            .value("linux_armv7l", Platform::linux_armv7l)
            .value("linux_aarch64", Platform::linux_aarch64)
            .value("linux_ppc64le", Platform::linux_ppc64le)
            .value("linux_ppc64", Platform::linux_ppc64)
            .value("linux_s390x", Platform::linux_s390x)
            .value("linux_riscv32", Platform::linux_riscv32)
            .value("linux_riscv64", Platform::linux_riscv64)
            .value("osx_64", Platform::osx_64)
            .value("osx_arm64", Platform::osx_arm64)
            .value("win_32", Platform::win_32)
            .value("win_64", Platform::win_64)
            .value("win_arm64", Platform::win_arm64)
            .value("zos_z", Platform::zos_z)
            .def(py::init(&enum_from_str<Platform>))
            .def_static("parse", &platform_parse)
            .def_static("count", &known_platforms_count)
            .def_static("build_platform", &build_platform);
        py::implicitly_convertible<py::str, Platform>();

        auto py_conda_url = py::class_<CondaURL>(m, "CondaURL");

        py::enum_<CondaURL::Credentials>(py_conda_url, "Credentials")
            .value("Hide", CondaURL::Credentials::Hide)
            .value("Show", CondaURL::Credentials::Show)
            .value("Remove", CondaURL::Credentials::Remove)
            .def(py::init(&enum_from_str<CondaURL::Credentials>));
        py::implicitly_convertible<py::str, CondaURL::Credentials>();

        py_conda_url  //
            .def_static("parse", &CondaURL::parse)
            .def(py::init())
            .def("__copy__", &copy<CondaURL>)
            .def("__deepcopy__", &deepcopy<CondaURL>)
            .def(py::self == py::self)
            .def(py::self != py::self)
            .def("__hash__", &hash<CondaURL>)
            .def("scheme", [](const CondaURL& self) { return self.scheme(); })
            .def(
                "set_scheme",
                [](CondaURL& self, std::string_view str) { return self.set_scheme(str); }
            )
            .def("clear_scheme", [](CondaURL& self) { return self.clear_scheme(); })
            .def(
                "scheme_is_defaulted",
                [](const CondaURL& self) { return self.scheme_is_defaulted(); }
            )
            .def(
                "user",
                [](const CondaURL& self, bool decode) {
                    return decode ? self.user(CondaURL::Decode::yes)
                                  : self.user(CondaURL::Decode::no);
                },
                py::arg("decode") = true
            )
            .def(
                "set_user",
                [](CondaURL& self, std::string_view str, bool encode)
                {
                    if (encode)
                    {
                        self.set_user(str, CondaURL::Encode::yes);
                    }
                    self.set_user(std::string(str), CondaURL::Encode::no);
                },
                py::arg("user"),
                py::arg("encode") = true
            )
            .def("clear_user", [](CondaURL& self) { return self.clear_user(); })
            .def("has_user", [](const CondaURL& self) { return self.has_user(); })
            .def(
                "password",
                [](const CondaURL& self, bool decode) {
                    return decode ? self.password(CondaURL::Decode::yes)
                                  : self.password(CondaURL::Decode::no);
                },
                py::arg("decode") = true
            )
            .def(
                "set_password",
                [](CondaURL& self, std::string_view str, bool encode)
                {
                    if (encode)
                    {
                        self.set_password(str, CondaURL::Encode::yes);
                    }
                    self.set_password(std::string(str), CondaURL::Encode::no);
                },
                py::arg("password"),
                py::arg("encode") = true
            )
            .def("clear_password", [](CondaURL& self) { return self.clear_password(); })
            .def("has_password", [](const CondaURL& self) { return self.has_password(); })
            .def("authentication", [](const CondaURL& self) { return self.authentication(); })
            .def(
                "host",
                [](const CondaURL& self, bool decode)
                {
                    return decode ? self.host(CondaURL::Decode::yes)
                                  : std::string(self.host(CondaURL::Decode::no));
                },
                py::arg("decode") = true
            )
            .def(
                "set_host",
                [](CondaURL& self, std::string_view str, bool encode)
                {
                    if (encode)
                    {
                        return self.set_host(str, CondaURL::Encode::yes);
                    }
                    return self.set_host(std::string(str), CondaURL::Encode::no);
                },
                py::arg("host"),
                py::arg("encode") = true
            )
            .def("clear_host", [](CondaURL& self) { return self.clear_host(); })
            .def("host_is_defaulted", [](const CondaURL& self) { return self.host_is_defaulted(); })
            .def("port", [](const CondaURL& self) { return self.port(); })
            .def("set_port", [](CondaURL& self, std::string_view str) { self.set_port(str); })
            .def("clear_port", [](CondaURL& self) { return self.clear_port(); })
            .def("authority", [](const CondaURL& self) { return self.authority(); })
            .def(
                "path",
                [](const CondaURL& self, bool decode) {
                    return decode ? self.path(CondaURL::Decode::yes)
                                  : self.path(CondaURL::Decode::no);
                },
                py::arg("decode") = true
            )
            .def(
                "set_path",
                [](CondaURL& self, std::string_view str, bool encode)
                {
                    if (encode)
                    {
                        return self.set_path(str, CondaURL::Encode::yes);
                    }
                    return self.set_path(std::string(str), CondaURL::Encode::no);
                },
                py::arg("path"),
                py::arg("encode") = true
            )
            .def("clear_path", [](CondaURL& self) { return self.clear_path(); })
            .def(
                "append_path",
                [](CondaURL& self, std::string_view str, bool encode)
                {
                    if (encode)
                    {
                        return self.append_path(str, CondaURL::Encode::yes);
                    }
                    return self.append_path(std::string(str), CondaURL::Encode::no);
                },
                py::arg("path"),
                py::arg("encode") = true
            )
            .def("pretty_path", [](const CondaURL& self) { return self.pretty_path(); })
            .def("token", &CondaURL::token)
            .def("set_token", &CondaURL::set_token)
            .def("clear_token", &CondaURL::clear_token)
            .def("has_token", &CondaURL::has_token)
            .def(
                "path_without_token",
                [](const CondaURL& self, bool decode)
                {
                    return decode ? self.path_without_token(CondaURL::Decode::yes)
                                  : std::string(self.path_without_token(CondaURL::Decode::no));
                },
                py::arg("decode") = true
            )
            .def(
                "set_path_without_token",
                [](CondaURL& self, std::string_view str, bool encode)
                {
                    if (encode)
                    {
                        return self.set_path_without_token(str, CondaURL::Encode::yes);
                    }
                    return self.set_path_without_token(std::string(str), CondaURL::Encode::no);
                },
                py::arg("path_without_token"),
                py::arg("encode") = true
            )
            .def("clear_path_without_token", &CondaURL::clear_path_without_token)
            .def("platform", &CondaURL::platform)
            .def("set_platform", [](CondaURL& self, Platform plat) { self.set_platform(plat); })
            .def("clear_platform", &CondaURL::clear_platform)
            .def(
                "package",
                [](const CondaURL& self, bool decode)
                {
                    return decode ? self.package(CondaURL::Decode::yes)
                                  : std::string(self.package(CondaURL::Decode::no));
                },
                py::arg("decode") = true
            )
            .def(
                "set_package",
                [](CondaURL& self, std::string_view str, bool encode)
                {
                    if (encode)
                    {
                        return self.set_package(str, CondaURL::Encode::yes);
                    }
                    return self.set_package(std::string(str), CondaURL::Encode::no);
                },
                py::arg("package"),
                py::arg("encode") = true
            )
            .def("clear_package", &CondaURL::clear_package)
            .def("str", &CondaURL::str, py::arg("credentials") = CondaURL::Credentials::Hide)
            .def(
                "__repr__",
                [](const CondaURL& self) { return self.str(CondaURL::Credentials::Hide); }
            )
            .def(
                "pretty_str",
                [](const CondaURL& self,
                   bool strip_scheme,
                   std::string_view rstrip_path,
                   CondaURL::Credentials credentials)
                {
                    auto const scheme = strip_scheme ? CondaURL::StripScheme::yes
                                                     : CondaURL::StripScheme::no;
                    const char rstrip = rstrip_path.empty() ? '\0' : rstrip_path.front();
                    return self.pretty_str(scheme, rstrip, credentials);
                },
                py::arg("strip_scheme") = false,
                py::arg("rstrip_path") = "/",
                py::arg("credentials") = CondaURL::Credentials::Hide
            );

        auto py_channel_spec = py::class_<ChannelSpec>(m, "ChannelSpec");

        py::enum_<ChannelSpec::Type>(py_channel_spec, "Type")
            .value("URL", ChannelSpec::Type::URL)
            .value("PackageURL", ChannelSpec::Type::PackageURL)
            .value("Path", ChannelSpec::Type::Path)
            .value("PackagePath", ChannelSpec::Type::PackagePath)
            .value("Name", ChannelSpec::Type::Name)
            .value("Unknown", ChannelSpec::Type::Unknown)
            .def(py::init(&enum_from_str<ChannelSpec::Type>));
        py::implicitly_convertible<py::str, ChannelSpec::Type>();

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
