// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <pybind11/operators.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl_bind.h>

#include "mamba/specs/archive.hpp"
#include "mamba/specs/authentication_info.hpp"
#include "mamba/specs/channel.hpp"
#include "mamba/specs/channel_spec.hpp"
#include "mamba/specs/conda_url.hpp"
#include "mamba/specs/platform.hpp"
#include "mamba/specs/version.hpp"
#include "mamba/specs/version_spec.hpp"

#include "bindings.hpp"
#include "flat_set_caster.hpp"
#include "utils.hpp"
#include "weakening_map_bind.hpp"

PYBIND11_MAKE_OPAQUE(mamba::specs::VersionPart);
PYBIND11_MAKE_OPAQUE(mamba::specs::CommonVersion);

namespace mambapy
{
    void bind_submodule_specs(pybind11::module_ m)
    {
        namespace py = pybind11;
        using namespace mamba::specs;

        m.def("archive_extensions", []() { return ARCHIVE_EXTENSIONS; });

        m.def(
            "has_archive_extension",
            [](std::string_view str) { return has_archive_extension(str); }
        );
        m.def(
            "has_archive_extension",
            [](const mamba::fs::u8path& p) { return has_archive_extension(p); }
        );

        m.def(
            "strip_archive_extension",
            [](std::string_view str) { return strip_archive_extension(str); }
        );
        m.def(
            "strip_archive_extension",
            [](const mamba::fs::u8path& p) { return strip_archive_extension(p); }
        );

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
            .def(py::self / std::string_view("other"))
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
            .def("__deepcopy__", &deepcopy<ChannelSpec>, py::arg("memo"))
            .def_property_readonly("type", &ChannelSpec::type)
            .def_property_readonly("location", &ChannelSpec::location)
            .def_property_readonly("platform_filters", &ChannelSpec::platform_filters);

        py::class_<BasicHTTPAuthentication>(m, "BasicHTTPAuthentication")
            .def(
                py::init(
                    [](std::string_view user, std::string_view password) -> BasicHTTPAuthentication
                    {
                        return {
                            /* .user= */ std::string(user),
                            /* .password= */ std::string(password),
                        };
                    }
                ),
                py::arg("user"),
                py::arg("password")
            )
            .def_readwrite("user", &BasicHTTPAuthentication::user)
            .def_readwrite("password", &BasicHTTPAuthentication::password)
            .def(py::self == py::self)
            .def(py::self != py::self)
            .def("__copy__", &copy<BasicHTTPAuthentication>)
            .def("__deepcopy__", &deepcopy<BasicHTTPAuthentication>, py::arg("memo"))
            .def("__hash__", &hash<BasicHTTPAuthentication>);

        py::class_<BearerToken>(m, "BearerToken")
            .def(
                py::init([](std::string_view token) -> BearerToken { return { std::string(token) }; }),
                py::arg("token")
            )
            .def_readwrite("token", &BearerToken::token)
            .def(py::self == py::self)
            .def(py::self != py::self)
            .def("__copy__", &copy<BearerToken>)
            .def("__deepcopy__", &deepcopy<BearerToken>, py::arg("memo"))
            .def("__hash__", &hash<BearerToken>);

        py::class_<CondaToken>(m, "CondaToken")
            .def(
                py::init([](std::string_view token) -> CondaToken { return { std::string(token) }; }),
                py::arg("token")
            )
            .def_readwrite("token", &CondaToken::token)
            .def(py::self == py::self)
            .def(py::self != py::self)
            .def("__copy__", &copy<CondaToken>)
            .def("__deepcopy__", &deepcopy<CondaToken>, py::arg("memo"))
            .def("__hash__", &hash<CondaToken>);

        bind_weakening_map<AuthenticationDataBase>(m, "AuthenticationDataBase");

        auto py_resolve_params = py::class_<ChannelResolveParams>(m, "ChannelResolveParams");
        auto py_channel = py::class_<Channel>(m, "Channel");

        bind_weakening_map<ChannelResolveParams::channel_map>(py_resolve_params, "ChannelMap");
        bind_weakening_map<ChannelResolveParams::multichannel_map>(py_resolve_params, "MultiChannelMap");

        py_resolve_params  //
            .def(
                py::init(
                    [](ChannelResolveParams::platform_list platforms,
                       CondaURL channel_alias,
                       ChannelResolveParams::channel_map custom_channels,
                       ChannelResolveParams::multichannel_map custom_multichannels,
                       AuthenticationDataBase authentication_db,
                       std::string home_dir,
                       std::string current_working_dir  //
                    ) -> ChannelResolveParams
                    {
                        return {
                            /* .platforms= */ std::move(platforms),
                            /* .channel_alias= */ std::move(channel_alias),
                            /* .custom_channels= */ std::move(custom_channels),
                            /* .custom_multichannels= */ std::move(custom_multichannels),
                            /* .authentication_db= */ std::move(authentication_db),
                            /* .home_dir= */ std::move(home_dir),
                            /* .current_working_dir= */ std::move(current_working_dir),
                        };
                    }
                ),
                // Not really meant to provide sensible defaults, the ChannelContext does that.
                py::arg("platforms") = ChannelResolveParams::platform_list{},
                py::arg("channel_alias") = CondaURL{},
                py::arg("custom_channels") = ChannelResolveParams::channel_map{},
                py::arg("custom_multichannels") = ChannelResolveParams::multichannel_map{},
                py::arg("authentication_db") = AuthenticationDataBase{},
                py::arg("home_dir") = std::string{},
                py::arg("current_working_dir") = std::string{}
            )
            .def_readwrite("platforms", &ChannelResolveParams::platforms)
            .def_readwrite("channel_alias", &ChannelResolveParams::channel_alias)
            .def_readwrite("custom_channels", &ChannelResolveParams::custom_channels)
            .def_readwrite("custom_multichannels", &ChannelResolveParams::custom_multichannels)
            .def_readwrite("authentication_db", &ChannelResolveParams::authentication_db)
            .def_readwrite("home_dir", &ChannelResolveParams::home_dir)
            .def_readwrite("current_working_dir", &ChannelResolveParams::current_working_dir)
            .def("__copy__", &copy<BasicHTTPAuthentication>)
            .def("__deepcopy__", &deepcopy<BasicHTTPAuthentication>, py::arg("memo"));

        py_channel  //
            .def_property_readonly_static(
                "ChannelMap",
                [](py::handle) { return py::type::of<ChannelResolveParams::channel_map>(); }
            )
            .def_property_readonly_static(
                "MultiChannelMap",
                [](py::handle) { return py::type::of<ChannelResolveParams::multichannel_map>(); }
            )
            .def(
                py::init<CondaURL, std::string, Channel::platform_list>(),
                py::arg("url"),
                py::arg("display_name"),
                py::arg("platforms")
            )
            .def_static(
                "resolve",
                py::overload_cast<ChannelSpec, const ChannelResolveParams&>(&Channel::resolve),
                py::arg("spec"),
                py::arg("params")
            )
            .def_static(
                "resolve",
                [](const ChannelSpec& spec,
                   const ChannelResolveParams::platform_list& platforms,
                   const CondaURL& channel_alias,
                   const ChannelResolveParams::channel_map& custom_channels,
                   const ChannelResolveParams::multichannel_map& custom_multichannels,
                   const AuthenticationDataBase& authentication_db,
                   std::string_view home_dir,
                   std::string_view current_working_dir  //
                )
                {
                    return Channel::resolve(
                        spec,
                        ChannelResolveParamsView{
                            /* .platforms= */ platforms,
                            /* .channel_alias= */ channel_alias,
                            /* .custom_channels= */ custom_channels,
                            /* .custom_multichannels= */ custom_multichannels,
                            /* .authentication_db= */ authentication_db,
                            /* .home_dir= */ home_dir,
                            /* .current_working_dir= */ current_working_dir,
                        }
                    );
                },
                // Not really meant to provide sensible defaults, the ChannelContext does that.
                py::arg("spec"),
                py::arg("platforms") = ChannelResolveParams::platform_list{},
                py::arg("channel_alias") = CondaURL{},
                py::arg("custom_channels") = ChannelResolveParams::channel_map{},
                py::arg("custom_multichannels") = ChannelResolveParams::multichannel_map{},
                py::arg("authentication_db") = AuthenticationDataBase{},
                py::arg("home_dir") = std::string{},
                py::arg("current_working_dir") = std::string{}
            )
            .def("is_package", &Channel::is_package)
            .def_property("url", &Channel::url, &Channel::set_url)
            .def_property("platforms", &Channel::platforms, &Channel::set_platforms)
            .def_property("display_name", &Channel::display_name, &Channel::set_display_name)
            .def("platform_url", &Channel::platform_url)
            .def("platform_urls", &Channel::platform_urls)
            .def("url_equivalent_with", &Channel::url_equivalent_with)
            .def("is_equivalent_to", &Channel::is_equivalent_to)
            .def("contains_equivalent", &Channel::contains_equivalent)
            .def(py::self == py::self)
            .def(py::self != py::self)
            .def("__hash__", &hash<Channel>)
            .def("__copy__", &copy<Channel>)
            .def("__deepcopy__", &deepcopy<Channel>, py::arg("memo"));

        py::class_<VersionPartAtom>(m, "VersionPartAtom")
            .def(py::init<>())
            .def(py::init<std::size_t, std::string_view>(), py::arg("numeral"), py::arg("literal") = "")
            .def_property_readonly("numeral", &VersionPartAtom::numeral)
            .def_property_readonly(
                "literal",
                [](const VersionPartAtom& atom) { return atom.literal(); }
            )
            .def("__str__", &VersionPartAtom::str)
            .def(py::self == py::self)
            .def(py::self != py::self)
            .def(py::self < py::self)
            .def(py::self <= py::self)
            .def(py::self > py::self)
            .def(py::self >= py::self)
            .def("__copy__", &copy<VersionPartAtom>)
            .def("__deepcopy__", &deepcopy<VersionPartAtom>, py::arg("memo"));

        // Type made opaque at the top of this file
        py::bind_vector<VersionPart>(m, "VersionPart");

        // Type made opaque at the top of this file
        py::bind_vector<CommonVersion>(m, "CommonVersion");

        py::class_<Version>(m, "Version")
            .def_readonly_static("epoch_delim", &Version::epoch_delim)
            .def_readonly_static("local_delim", &Version::local_delim)
            .def_readonly_static("part_delim", &Version::part_delim)
            .def_readonly_static("part_delim_alt", &Version::part_delim_alt)
            .def_readonly_static("part_delim_special", &Version::part_delim_special)
            .def_static("parse", &Version::parse, py::arg("str"))
            .def(
                py::init<std::size_t, CommonVersion, CommonVersion>(),
                py::arg("epoch") = 0,
                py::arg("version") = CommonVersion(),
                py::arg("local") = CommonVersion()
            )
            .def_property_readonly("epoch", &Version::epoch)
            .def_property_readonly("version", &Version::version)
            .def_property_readonly("local", &Version::local)
            .def("starts_with", &Version::starts_with, py::arg("prefix"))
            .def("compatible_with", &Version::compatible_with, py::arg("older"), py::arg("level"))
            .def("__str__", &Version::str)
            .def(py::self == py::self)
            .def(py::self != py::self)
            .def(py::self < py::self)
            .def(py::self <= py::self)
            .def(py::self > py::self)
            .def(py::self >= py::self)
            .def("__copy__", &copy<Version>)
            .def("__deepcopy__", &deepcopy<Version>, py::arg("memo"));

        // Bindings for VersionSpec currently ignores VersionPredicate and flat_bool_expr_tree
        // which would be tedious to bind, and even more to make extendable through Python

        py::class_<VersionSpec>(m, "VersionSpec")
            .def_readonly_static("and_token", &VersionSpec::and_token)
            .def_readonly_static("or_token", &VersionSpec::or_token)
            .def_readonly_static("left_parenthesis_token", &VersionSpec::left_parenthesis_token)
            .def_readonly_static("right_parenthesis_token", &VersionSpec::right_parenthesis_token)
            .def_readonly_static("starts_with_str", &VersionSpec::starts_with_str)
            .def_readonly_static("equal_str", &VersionSpec::equal_str)
            .def_readonly_static("not_equal_str", &VersionSpec::not_equal_str)
            .def_readonly_static("greater_str", &VersionSpec::greater_str)
            .def_readonly_static("greater_equal_str", &VersionSpec::greater_equal_str)
            .def_readonly_static("less_str", &VersionSpec::less_str)
            .def_readonly_static("less_equal_str", &VersionSpec::less_equal_str)
            .def_readonly_static("compatible_str", &VersionSpec::compatible_str)
            .def_readonly_static("glob_suffix_str", &VersionSpec::glob_suffix_str)
            .def_readonly_static("glob_suffix_token", &VersionSpec::glob_suffix_token)
            .def_static("parse", &VersionSpec::parse, py::arg("str"))
            .def("contains", &VersionSpec::contains, py::arg("point"))
            .def("__copy__", &copy<VersionSpec>)
            .def("__deepcopy__", &deepcopy<VersionSpec>, py::arg("memo"));
    }
}
