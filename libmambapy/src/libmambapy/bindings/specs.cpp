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
#include "mamba/specs/conda_url.hpp"
#include "mamba/specs/error.hpp"
#include "mamba/specs/glob_spec.hpp"
#include "mamba/specs/match_spec.hpp"
#include "mamba/specs/package_info.hpp"
#include "mamba/specs/platform.hpp"
#include "mamba/specs/unresolved_channel.hpp"
#include "mamba/specs/version.hpp"
#include "mamba/specs/version_spec.hpp"

#include "bindings.hpp"
#include "expected_caster.hpp"
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

        py::register_local_exception<ParseError>(m, "ParseError", PyExc_ValueError);

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

        py::enum_<KnownPlatform>(m, "KnownPlatform")
            .value("noarch", KnownPlatform::noarch)
            .value("linux_32", KnownPlatform::linux_32)
            .value("linux_64", KnownPlatform::linux_64)
            .value("linux_armv6l", KnownPlatform::linux_armv6l)
            .value("linux_armv7l", KnownPlatform::linux_armv7l)
            .value("linux_aarch64", KnownPlatform::linux_aarch64)
            .value("linux_ppc64le", KnownPlatform::linux_ppc64le)
            .value("linux_ppc64", KnownPlatform::linux_ppc64)
            .value("linux_s390x", KnownPlatform::linux_s390x)
            .value("linux_riscv32", KnownPlatform::linux_riscv32)
            .value("linux_riscv64", KnownPlatform::linux_riscv64)
            .value("osx_64", KnownPlatform::osx_64)
            .value("osx_arm64", KnownPlatform::osx_arm64)
            .value("win_32", KnownPlatform::win_32)
            .value("win_64", KnownPlatform::win_64)
            .value("win_arm64", KnownPlatform::win_arm64)
            .value("zos_z", KnownPlatform::zos_z)
            .def(py::init(&enum_from_str<KnownPlatform>))
            .def_static("parse", &platform_parse)
            .def_static("count", &known_platforms_count)
            .def_static("build_platform", &build_platform);
        py::implicitly_convertible<py::str, KnownPlatform>();

        py::enum_<NoArchType>(m, "NoArchType")
            .value("No", NoArchType::No)
            .value("Generic", NoArchType::Generic)
            .value("Python", NoArchType::Python)
            .def(py::init(&enum_from_str<NoArchType>))
            .def_static("parse", &noarch_parse)
            .def_static("count", &known_noarch_count);
        py::implicitly_convertible<py::str, NoArchType>();

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
            .def("__deepcopy__", &deepcopy<CondaURL>, py::arg("memo"))
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
            .def("set_platform", [](CondaURL& self, KnownPlatform plat) { self.set_platform(plat); })
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

        auto py_unresolved_channel = py::class_<UnresolvedChannel>(m, "UnresolvedChannel");

        py::enum_<UnresolvedChannel::Type>(py_unresolved_channel, "Type")
            .value("URL", UnresolvedChannel::Type::URL)
            .value("PackageURL", UnresolvedChannel::Type::PackageURL)
            .value("Path", UnresolvedChannel::Type::Path)
            .value("PackagePath", UnresolvedChannel::Type::PackagePath)
            .value("Name", UnresolvedChannel::Type::Name)
            .value("Unknown", UnresolvedChannel::Type::Unknown)
            .def(py::init(&enum_from_str<UnresolvedChannel::Type>));
        py::implicitly_convertible<py::str, UnresolvedChannel::Type>();

        py_unresolved_channel  //
            .def_static("parse", UnresolvedChannel::parse)
            .def(
                py::init<std::string, UnresolvedChannel::platform_set, UnresolvedChannel::Type>(),
                py::arg("location"),
                py::arg("platform_filters"),
                py::arg("type") = UnresolvedChannel::Type::Unknown
            )
            .def("__str__", &UnresolvedChannel::str)
            .def("__copy__", &copy<UnresolvedChannel>)
            .def("__deepcopy__", &deepcopy<UnresolvedChannel>, py::arg("memo"))
            .def_property_readonly("type", &UnresolvedChannel::type)
            .def_property_readonly("location", &UnresolvedChannel::location)
            .def_property_readonly("platform_filters", &UnresolvedChannel::platform_filters);

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

        py::enum_<Channel::Match>(py_channel, "Match")
            .value("No", Channel::Match::No)
            .value("InOtherPlatform", Channel::Match::InOtherPlatform)
            .value("Full", Channel::Match::Full)
            .def(py::init(&enum_from_str<Channel::Match>));
        py::implicitly_convertible<py::str, Channel::Match>();

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
                py::overload_cast<UnresolvedChannel, const ChannelResolveParams&>(&Channel::resolve),
                py::arg("what"),
                py::arg("params")
            )
            .def_static(
                "resolve",
                [](const UnresolvedChannel& what,
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
                        what,
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
                py::arg("what"),
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
            .def("contains_package", &Channel::contains_package)
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
            .def("__str__", [](const Version& v) { return v.str(); })
            .def("str", [](const Version& v) { return v.str(); })
            .def(
                "str",
                [](const Version& v, std::size_t level) { return v.str(level); },
                py::arg("level")
            )
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
            .def_readonly_static("prefered_free_str", &VersionSpec::prefered_free_str)
            .def_readonly_static("all_free_strs", &VersionSpec::all_free_strs)
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
            .def("__str__", &VersionSpec::str)
            .def("__copy__", &copy<VersionSpec>)
            .def("__deepcopy__", &deepcopy<VersionSpec>, py::arg("memo"));

        py::class_<PackageInfo>(m, "PackageInfo")
            .def_static("from_url", PackageInfo::from_url)
            .def(
                py::init(
                    [](decltype(PackageInfo::name) name,
                       decltype(PackageInfo::version) version,
                       decltype(PackageInfo::build_string) build_string,
                       decltype(PackageInfo::build_number) build_number,
                       decltype(PackageInfo::channel) channel,
                       decltype(PackageInfo::package_url) package_url,
                       decltype(PackageInfo::platform) platform,
                       decltype(PackageInfo::filename) filename,
                       decltype(PackageInfo::license) license,
                       decltype(PackageInfo::md5) md5,
                       decltype(PackageInfo::sha256) sha256,
                       decltype(PackageInfo::signatures) signatures,
                       decltype(PackageInfo::track_features) track_features,
                       decltype(PackageInfo::dependencies) depends,
                       decltype(PackageInfo::constrains) constrains,
                       decltype(PackageInfo::defaulted_keys) defaulted_keys,
                       decltype(PackageInfo::noarch) noarch,
                       decltype(PackageInfo::size) size,
                       decltype(PackageInfo::timestamp) timestamp) -> PackageInfo
                    {
                        auto pkg = PackageInfo();
                        pkg.name = std::move(name);
                        pkg.version = std::move(version);
                        pkg.build_string = std::move(build_string);
                        pkg.build_number = std::move(build_number);
                        pkg.channel = std::move(channel);
                        pkg.package_url = std::move(package_url);
                        pkg.platform = std::move(platform);
                        pkg.filename = std::move(filename);
                        pkg.license = std::move(license);
                        pkg.md5 = std::move(md5);
                        pkg.sha256 = std::move(sha256);
                        pkg.signatures = std::move(signatures);
                        pkg.track_features = std::move(track_features);
                        pkg.dependencies = std::move(depends);
                        pkg.constrains = std::move(constrains);
                        pkg.defaulted_keys = std::move(defaulted_keys);
                        pkg.noarch = std::move(noarch);
                        pkg.size = std::move(size);
                        pkg.timestamp = std::move(timestamp);
                        return pkg;
                    }
                ),
                py::arg("name") = decltype(PackageInfo::name)(),
                py::arg("version") = decltype(PackageInfo::version)(),
                py::arg("build_string") = decltype(PackageInfo::build_string)(),
                py::arg("build_number") = decltype(PackageInfo::build_number)(),
                py::arg("channel") = decltype(PackageInfo::channel)(),
                py::arg("package_url") = decltype(PackageInfo::package_url)(),
                py::arg("platform") = decltype(PackageInfo::platform)(),
                py::arg("filename") = decltype(PackageInfo::filename)(),
                py::arg("license") = decltype(PackageInfo::license)(),
                py::arg("md5") = decltype(PackageInfo::md5)(),
                py::arg("sha256") = decltype(PackageInfo::sha256)(),
                py::arg("signatures") = decltype(PackageInfo::signatures)(),
                py::arg("track_features") = decltype(PackageInfo::track_features)(),
                py::arg("depends") = decltype(PackageInfo::dependencies)(),
                py::arg("constrains") = decltype(PackageInfo::constrains)(),
                py::arg("defaulted_keys") = decltype(PackageInfo::defaulted_keys)(),
                py::arg("noarch") = decltype(PackageInfo::noarch)(),
                py::arg("size") = decltype(PackageInfo::size)(),
                py::arg("timestamp") = decltype(PackageInfo::timestamp)()
            )
            .def_readwrite("name", &PackageInfo::name)
            .def_readwrite("version", &PackageInfo::version)
            .def_readwrite("build_string", &PackageInfo::build_string)
            .def_readwrite("build_number", &PackageInfo::build_number)
            .def_readwrite("noarch", &PackageInfo::noarch)
            .def_readwrite("channel", &PackageInfo::channel)
            .def_readwrite("package_url", &PackageInfo::package_url)
            .def_property(
                // V2 migration helper
                "url",
                [](py::handle) { throw std::runtime_error("'url' has been renamed 'package_url'"); },
                [](py::handle, py::handle)
                { throw std::runtime_error("'url' has been renamed 'package_url'"); }
            )
            .def_readwrite("platform", &PackageInfo::platform)
            .def_readwrite("filename", &PackageInfo::filename)
            .def_property(
                // V2 migration helper
                "fn",
                [](py::handle) { throw std::runtime_error("'fn' has been renamed 'filename'"); },
                [](py::handle, py::handle)
                { throw std::runtime_error("'fn' has been renamed 'filename'"); }
            )
            .def_readwrite("license", &PackageInfo::license)
            .def_readwrite("size", &PackageInfo::size)
            .def_readwrite("timestamp", &PackageInfo::timestamp)
            .def_readwrite("md5", &PackageInfo::md5)
            .def_readwrite("sha256", &PackageInfo::sha256)
            .def_readwrite("track_features", &PackageInfo::track_features)
            .def_readwrite("dependencies", &PackageInfo::dependencies)
            .def_readwrite("constrains", &PackageInfo::constrains)
            .def_readwrite("signatures", &PackageInfo::signatures)
            .def_readwrite("defaulted_keys", &PackageInfo::defaulted_keys)
            .def(py::self == py::self)
            .def(py::self != py::self)
            .def("__str__", &PackageInfo::str)
            .def("__copy__", &copy<PackageInfo>)
            .def("__deepcopy__", &deepcopy<PackageInfo>, py::arg("memo"));

        py::class_<GlobSpec>(m, "GlobSpec")
            .def_readonly_static("free_pattern", &GlobSpec::free_pattern)
            .def_readonly_static("glob_pattern", &GlobSpec::glob_pattern)
            .def(py::init<>())
            .def(py::init<std::string>(), py::arg("spec"))
            .def("contains", &GlobSpec::contains)
            .def("is_free", &GlobSpec::is_free)
            .def("is_exact", &GlobSpec::is_exact)
            .def("__str__", &GlobSpec::str)
            .def("__copy__", &copy<GlobSpec>)
            .def("__deepcopy__", &deepcopy<GlobSpec>, py::arg("memo"));

        py::class_<MatchSpec>(m, "MatchSpec")
            .def_property_readonly_static("NameSpec", &py::type::of<GlobSpec>)
            .def_property_readonly_static("BuildStringSpec", &py::type::of<GlobSpec>)
            .def_readonly_static("url_md5_sep", &MatchSpec::url_md5_sep)
            .def_readonly_static("prefered_list_open", &MatchSpec::prefered_list_open)
            .def_readonly_static("prefered_list_close", &MatchSpec::prefered_list_close)
            .def_readonly_static("alt_list_open", &MatchSpec::alt_list_open)
            .def_readonly_static("alt_list_close", &MatchSpec::alt_list_close)
            .def_readonly_static("prefered_quote", &MatchSpec::prefered_quote)
            .def_readonly_static("alt_quote", &MatchSpec::alt_quote)
            .def_readonly_static("channel_namespace_spec_sep", &MatchSpec::channel_namespace_spec_sep)
            .def_readonly_static("attribute_sep", &MatchSpec::attribute_sep)
            .def_readonly_static("attribute_assign", &MatchSpec::attribute_assign)
            .def_readonly_static("package_version_sep", &MatchSpec::package_version_sep)
            .def_static("parse", &MatchSpec::parse)
            .def_static("parse_url", &MatchSpec::parse_url)
            .def(
                // V2 Migation: Hard deperecation since errors would be hard to track.
                py::init(
                    [](std::string_view) -> MatchSpec {
                        throw std::invalid_argument(
                            "Use 'MatchSpec.parse' to create a new object from a string"
                        );
                    }
                ),
                py::arg("spec")
            )
            .def_property("channel", &MatchSpec::channel, &MatchSpec::set_channel)
            .def_property("filename", &MatchSpec::filename, &MatchSpec::set_filename)
            .def_property("platforms", &MatchSpec::platforms, &MatchSpec::set_platforms)
            .def_property("name_space", &MatchSpec::name_space, &MatchSpec::set_name_space)
            .def_property("name", &MatchSpec::name, &MatchSpec::set_name)
            .def_property("version", &MatchSpec::version, &MatchSpec::set_version)
            .def_property("build_number", &MatchSpec::build_number, &MatchSpec::set_build_number)
            .def_property("build_string", &MatchSpec::build_string, &MatchSpec::set_build_string)
            .def_property("md5", &MatchSpec::md5, &MatchSpec::set_md5)
            .def_property("sha256", &MatchSpec::sha256, &MatchSpec::set_sha256)
            .def_property("license", &MatchSpec::license, &MatchSpec::set_license)
            .def_property("license_family", &MatchSpec::license_family, &MatchSpec::set_license_family)
            .def_property("features", &MatchSpec::features, &MatchSpec::set_features)
            .def_property("track_features", &MatchSpec::track_features, &MatchSpec::set_track_features)
            .def_property("optional", &MatchSpec::optional, &MatchSpec::set_optional)
            .def(
                "contains_except_channel",
                [](const MatchSpec& ms, const PackageInfo& pkg)
                { return ms.contains_except_channel(pkg); }
            )
            .def(
                "contains_except_channel",
                [](const MatchSpec& ms,
                   std::string_view name,
                   const Version& version,
                   std::string_view build_string,
                   std::size_t build_number,
                   std::string_view md5,
                   std::string_view sha256,
                   std::string_view license,
                   std::string& platform,
                   MatchSpec::string_set track_features)
                {
                    struct Pkg
                    {
                        std::string_view name;
                        std::reference_wrapper<const Version> version;
                        std::string_view build_string;
                        std::size_t build_number;
                        std::string_view md5;
                        std::string_view sha256;
                        std::string_view license;
                        std::reference_wrapper<const std::string> platform;
                        const MatchSpec::string_set track_features;
                    };

                    return ms.contains_except_channel(Pkg{
                        /* .name= */ name,
                        /* .version= */ version,
                        /* .build_string= */ build_string,
                        /* .build_number= */ build_number,
                        /* .md5= */ md5,
                        /* .sha256= */ sha256,
                        /* .license= */ license,
                        /* .platform= */ platform,
                        /* .track_features= */ std::move(track_features),
                    });
                },
                py::arg("name") = "",
                py::arg("version") = Version(),
                py::arg("build_string") = "",
                py::arg("build_number") = 0,
                py::arg("md5") = "",
                py::arg("sha256") = "",
                py::arg("license") = "",
                py::arg("platform") = "",
                py::arg("track_features") = MatchSpec::string_set{}
            )
            .def("is_file", &MatchSpec::is_file)
            .def("is_simple", &MatchSpec::is_simple)
            .def("conda_build_form", &MatchSpec::conda_build_form)
            .def("__str__", &MatchSpec::str)
            .def("__copy__", &copy<MatchSpec>)
            .def("__deepcopy__", &deepcopy<MatchSpec>, py::arg("memo"));
    }
}
