// Copyright (c) 2026, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include <catch2/catch_all.hpp>

#include "mamba/core/sbom.hpp"
#include "mamba/specs/conda_url.hpp"
#include "mamba/specs/package_info.hpp"

#include "catch-utils/conda_url.hpp"

namespace
{
    using namespace mamba;

    const std::string zlib_sha256 = "f52f0c5f0f2f0f2f0f2f0f2f0f2f0f2f0f2f0f2f0f2f0f2f0f2f0f2f0f2f0f2f0f";

    auto conda_pkg(
        std::string name,
        std::string version,
        std::string build_string = "0",
        std::string sha256 = {},
        std::string channel = "conda-forge",
        std::string platform = "linux-64"
    ) -> specs::PackageInfo
    {
        specs::PackageInfo pkg;
        pkg.name = std::move(name);
        pkg.version = std::move(version);
        pkg.build_string = std::move(build_string);
        pkg.sha256 = std::move(sha256);
        pkg.channel = std::move(channel);
        pkg.platform = std::move(platform);
        pkg.filename = pkg.name + "-" + pkg.version + "-" + pkg.build_string + ".conda";
        pkg.package_url = "https://conda.anaconda.org/" + pkg.channel + "/" + pkg.platform + "/"
                          + pkg.filename;
        return pkg;
    }
}

TEST_CASE("sbom::detail::archive_type")
{
    SECTION(".conda filename")
    {
        const auto pkg = conda_pkg("zlib", "1.2.13", "hcfbbd0e_0");
        REQUIRE(sbom::detail::archive_type(pkg) == "conda");
    }

    SECTION(".tar.bz2 filename")
    {
        auto pkg = conda_pkg("openssl", "3.0.0", "h0b41bf4_0");
        pkg.filename = "openssl-3.0.0-h0b41bf4_0.tar.bz2";
        REQUIRE(sbom::detail::archive_type(pkg) == "tar.bz2");
    }

    SECTION("infers .tar.bz2 from package_url when filename is empty")
    {
        auto pkg = conda_pkg("openssl", "3.0.0", "h0b41bf4_0");
        pkg.filename = {};
        pkg.package_url = "https://conda.anaconda.org/conda-forge/linux-64/"
                          "openssl-3.0.0-h0b41bf4_0.tar.bz2";
        REQUIRE(sbom::detail::archive_type(pkg) == "tar.bz2");
    }

    SECTION("infers .conda from package_url when filename is empty")
    {
        auto pkg = conda_pkg("zlib", "1.2.13", "hcfbbd0e_0");
        pkg.filename = {};
        REQUIRE(sbom::detail::archive_type(pkg) == "conda");
    }

    SECTION("returns empty when the format is unknown")
    {
        auto pkg = conda_pkg("zlib", "1.2.13", "hcfbbd0e_0");
        pkg.filename = {};
        pkg.package_url = {};
        REQUIRE(sbom::detail::archive_type(pkg) == "");

        pkg.filename = "zlib-1.2.13-hcfbbd0e_0.zip";
        REQUIRE(sbom::detail::archive_type(pkg) == "");
    }
}

TEST_CASE("sbom::detail::channel_home_url")
{
    SECTION("strips subdir, package, query and fragment from package_url")
    {
        auto pkg = conda_pkg("zlib", "1.2.13", "hcfbbd0e_0");
        pkg.package_url += "?foo=bar#frag";
        const auto home = sbom::detail::channel_home_url(pkg);
        REQUIRE(home.has_value());
        REQUIRE(*home == specs::CondaURL::parse("https://conda.anaconda.org/conda-forge").value());
        REQUIRE(home->platform_name().empty());
        REQUIRE(home->package().empty());
        REQUIRE(home->query().empty());
        REQUIRE(home->fragment().empty());
    }

    SECTION("strips token from package_url")
    {
        auto pkg = conda_pkg("zlib", "1.2.13", "hcfbbd0e_0");
        pkg.package_url = "https://conda.anaconda.org/t/tok123/conda-forge/linux-64/zlib-1.2.13-hcfbbd0e_0.conda";
        const auto home = sbom::detail::channel_home_url(pkg);
        REQUIRE(home.has_value());
        REQUIRE_FALSE(home->has_token());
        REQUIRE(*home == specs::CondaURL::parse("https://conda.anaconda.org/conda-forge").value());
    }

    SECTION("strips credentials from package_url")
    {
        auto pkg = conda_pkg("zlib", "1.2.13", "hcfbbd0e_0");
        pkg.package_url = "https://user:password@conda.anaconda.org/conda-forge/linux-64/"
                          "zlib-1.2.13-hcfbbd0e_0.conda";
        const auto home = sbom::detail::channel_home_url(pkg);
        REQUIRE(home.has_value());
        REQUIRE_FALSE(home->has_user());
        REQUIRE_FALSE(home->has_password());
        REQUIRE(*home == specs::CondaURL::parse("https://conda.anaconda.org/conda-forge").value());
    }

    SECTION("falls back to channel when package_url is empty")
    {
        auto pkg = conda_pkg("zlib", "1.2.13", "hcfbbd0e_0");
        pkg.package_url = {};
        pkg.channel = "https://conda.anaconda.org/conda-forge";
        const auto home = sbom::detail::channel_home_url(pkg);
        REQUIRE(home.has_value());
        REQUIRE(*home == specs::CondaURL::parse("https://conda.anaconda.org/conda-forge").value());
    }

    SECTION("strips a trailing slash from the channel")
    {
        auto pkg = conda_pkg("zlib", "1.2.13", "hcfbbd0e_0");
        pkg.package_url = {};
        pkg.channel = "https://conda.anaconda.org/conda-forge/";
        const auto home = sbom::detail::channel_home_url(pkg);
        REQUIRE(home.has_value());
        REQUIRE(*home == specs::CondaURL::parse("https://conda.anaconda.org/conda-forge").value());
    }

    SECTION("keeps the root path slash")
    {
        auto pkg = conda_pkg("zlib", "1.2.13", "hcfbbd0e_0");
        pkg.package_url = {};
        pkg.channel = "https://conda.anaconda.org";
        const auto home = sbom::detail::channel_home_url(pkg);
        REQUIRE(home.has_value());
        REQUIRE(*home == specs::CondaURL::parse("https://conda.anaconda.org").value());
    }

    SECTION("returns nullopt when neither package_url nor channel has a scheme")
    {
        auto pkg = conda_pkg("zlib", "1.2.13", "hcfbbd0e_0");
        pkg.package_url = "conda-forge";
        pkg.channel = "conda-forge";
        REQUIRE_FALSE(sbom::detail::channel_home_url(pkg).has_value());
    }

    SECTION("returns nullopt when both are empty")
    {
        auto pkg = conda_pkg("zlib", "1.2.13", "hcfbbd0e_0");
        pkg.package_url = {};
        pkg.channel = {};
        REQUIRE_FALSE(sbom::detail::channel_home_url(pkg).has_value());
    }

    SECTION("accepts file:// channels")
    {
        auto pkg = conda_pkg("zlib", "1.2.13", "hcfbbd0e_0");
        pkg.package_url = {};
        pkg.channel = "file:///srv/mirror/mychan";
        const auto home = sbom::detail::channel_home_url(pkg);
        REQUIRE(home.has_value());
        REQUIRE(*home == specs::CondaURL::parse("file:///srv/mirror/mychan").value());
    }

    SECTION("returns nullopt when package_url has scheme but fails to parse and channel has no scheme")
    {
        auto pkg = conda_pkg("zlib", "1.2.13", "hcfbbd0e_0");
        pkg.package_url = "http://user@40email.com:test@localhost/linux-64/zlib-1.2.13-hcfbbd0e_0.conda";
        pkg.channel = "conda-forge";
        REQUIRE_FALSE(sbom::detail::channel_home_url(pkg).has_value());
    }

    SECTION("returns nullopt when both package_url and channel have scheme but fail to parse")
    {
        auto pkg = conda_pkg("zlib", "1.2.13", "hcfbbd0e_0");
        pkg.package_url = "http://user@40email.com:test@localhost/linux-64/zlib-1.2.13-hcfbbd0e_0.conda";
        pkg.channel = "http://user@40email.com:test@localhost";
        REQUIRE_FALSE(sbom::detail::channel_home_url(pkg).has_value());
    }

    SECTION("returns nullopt when channel has scheme but fails to parse and package_url is empty")
    {
        auto pkg = conda_pkg("zlib", "1.2.13", "hcfbbd0e_0");
        pkg.package_url = {};
        pkg.channel = "http://user@40email.com:test@localhost";
        REQUIRE_FALSE(sbom::detail::channel_home_url(pkg).has_value());
    }
}

TEST_CASE("sbom::detail::namespace_from_channel_home")
{
    const auto ns_of = [](std::string_view url) -> std::optional<std::string>
    { return sbom::detail::namespace_from_channel_home(specs::CondaURL::parse(url).value()); };

    SECTION("single segment on default host")
    {
        REQUIRE(ns_of("https://conda.anaconda.org/conda-forge") == "conda-forge");
    }

    SECTION("trailing slash is ignored")
    {
        REQUIRE(ns_of("https://conda.anaconda.org/conda-forge/") == "conda-forge");
    }

    SECTION("host and segment are lowercased")
    {
        REQUIRE(ns_of("https://CONDA.ANACONDA.ORG/Conda-Forge") == "conda-forge");
    }

    SECTION("empty path")
    {
        REQUIRE(ns_of("https://conda.anaconda.org") == std::nullopt);
    }

    SECTION("multi-segment path")
    {
        REQUIRE(ns_of("https://conda.anaconda.org/conda-forge/label/dev") == std::nullopt);
    }

    SECTION("banned namespace defaults")
    {
        REQUIRE(ns_of("https://conda.anaconda.org/defaults") == std::nullopt);
    }

    SECTION("banned namespace local")
    {
        REQUIRE(ns_of("https://conda.anaconda.org/local") == std::nullopt);
    }

    SECTION("non-default host")
    {
        REQUIRE(ns_of("https://prefix.dev/conda-forge") == std::nullopt);
    }

    SECTION("subdir names are not namespaceable")
    {
        REQUIRE(ns_of("https://conda.anaconda.org/linux-64") == std::nullopt);
        REQUIRE(ns_of("https://conda.anaconda.org/noarch") == std::nullopt);
    }
}

TEST_CASE("sbom::detail::channel_namespace")
{
    SECTION("full package_url on default host uses the channel as namespace")
    {
        const auto pkg = conda_pkg("zlib", "1.2.13", "hcfbbd0e_0");
        REQUIRE(sbom::detail::channel_namespace(pkg) == "conda-forge");
    }

    SECTION("credentials in package_url do not affect the namespace")
    {
        auto pkg = conda_pkg("zlib", "1.2.13", "hcfbbd0e_0");
        pkg.package_url = "https://user:password@conda.anaconda.org/conda-forge/linux-64/"
                          "zlib-1.2.13-hcfbbd0e_0.conda";
        REQUIRE(sbom::detail::channel_namespace(pkg) == "conda-forge");
    }

    SECTION("scheme-bearing channel on default host uses the channel as namespace")
    {
        auto pkg = conda_pkg("zlib", "1.2.13", "hcfbbd0e_0");
        pkg.package_url = {};
        pkg.channel = "https://conda.anaconda.org/conda-forge";
        REQUIRE(sbom::detail::channel_namespace(pkg) == "conda-forge");
    }

    SECTION("bare channel name falls back to namespace")
    {
        auto pkg = conda_pkg("zlib", "1.2.13", "hcfbbd0e_0");
        pkg.package_url = {};
        pkg.channel = "conda-forge";
        REQUIRE(sbom::detail::channel_namespace(pkg) == "conda-forge");
    }

    SECTION("bare channel name is lowercased")
    {
        auto pkg = conda_pkg("zlib", "1.2.13", "hcfbbd0e_0");
        pkg.package_url = {};
        pkg.channel = "ConDa-Forge";
        REQUIRE(sbom::detail::channel_namespace(pkg) == "conda-forge");
    }

    SECTION("bare defaults is not namespaceable")
    {
        auto pkg = conda_pkg("zlib", "1.2.13", "hcfbbd0e_0");
        pkg.package_url = {};
        pkg.channel = "defaults";
        REQUIRE_FALSE(sbom::detail::channel_namespace(pkg).has_value());
    }

    SECTION("bare local is not namespaceable")
    {
        auto pkg = conda_pkg("zlib", "1.2.13", "hcfbbd0e_0");
        pkg.package_url = {};
        pkg.channel = "local";
        REQUIRE_FALSE(sbom::detail::channel_namespace(pkg).has_value());
    }

    SECTION("bare multi-segment channel is not namespaceable")
    {
        auto pkg = conda_pkg("zlib", "1.2.13", "hcfbbd0e_0");
        pkg.package_url = {};
        pkg.channel = "conda-forge/label/dev";
        REQUIRE_FALSE(sbom::detail::channel_namespace(pkg).has_value());
    }

    SECTION("bare subdir-named channel is not namespaceable")
    {
        auto pkg = conda_pkg("zlib", "1.2.13", "hcfbbd0e_0");
        pkg.package_url = {};
        pkg.channel = "linux-64";
        REQUIRE_FALSE(sbom::detail::channel_namespace(pkg).has_value());
    }

    SECTION("scheme-bearing channel on non-default host is not namespaceable")
    {
        auto pkg = conda_pkg("zlib", "1.2.13", "hcfbbd0e_0");
        pkg.package_url = {};
        pkg.channel = "https://prefix.dev/conda-forge";
        REQUIRE_FALSE(sbom::detail::channel_namespace(pkg).has_value());
    }

    SECTION("empty channel and package_url")
    {
        auto pkg = conda_pkg("zlib", "1.2.13", "hcfbbd0e_0");
        pkg.package_url = {};
        pkg.channel = {};
        REQUIRE_FALSE(sbom::detail::channel_namespace(pkg).has_value());
    }
}

TEST_CASE("sbom::detail::channel_repository_url")
{
    SECTION("default host returns the channel base URL")
    {
        const auto pkg = conda_pkg("zlib", "1.2.13", "hcfbbd0e_0");
        REQUIRE(sbom::detail::channel_repository_url(pkg) == "https://conda.anaconda.org/conda-forge");
    }

    SECTION("subchannel returns the full channel base URL")
    {
        auto pkg = conda_pkg("libx", "1.0", "0");
        pkg.package_url = "https://conda.anaconda.org/conda-forge/label/dev/linux-64/libx-1.0-0.conda";
        REQUIRE(
            sbom::detail::channel_repository_url(pkg)
            == "https://conda.anaconda.org/conda-forge/label/dev"
        );
    }

    SECTION("custom host")
    {
        auto pkg = conda_pkg("numpy", "1.26.0", "py310h01a7c6b_0");
        pkg.package_url = "https://repo.anaconda.com/pkgs/main/linux-64/"
                          "numpy-1.26.0-py310h01a7c6b_0.conda";
        REQUIRE(sbom::detail::channel_repository_url(pkg) == "https://repo.anaconda.com/pkgs/main");
    }

    SECTION("token is stripped")
    {
        auto pkg = conda_pkg("zlib", "1.2.13", "hcfbbd0e_0");
        pkg.package_url = "https://conda.anaconda.org/t/tok123/conda-forge/linux-64/zlib-1.2.13-hcfbbd0e_0.conda";
        REQUIRE(sbom::detail::channel_repository_url(pkg) == "https://conda.anaconda.org/conda-forge");
    }

    SECTION("credentials are stripped")
    {
        auto pkg = conda_pkg("zlib", "1.2.13", "hcfbbd0e_0");
        pkg.package_url = "https://user:password@conda.anaconda.org/conda-forge/linux-64/"
                          "zlib-1.2.13-hcfbbd0e_0.conda";
        REQUIRE(sbom::detail::channel_repository_url(pkg) == "https://conda.anaconda.org/conda-forge");
    }

    SECTION("bare defaults resolves against the default repository")
    {
        auto pkg = conda_pkg("libx", "1.0", "0");
        pkg.package_url = {};
        pkg.channel = "defaults";
        REQUIRE(sbom::detail::channel_repository_url(pkg) == "https://conda.anaconda.org/defaults");
    }

    SECTION("bare local resolves against the default repository")
    {
        auto pkg = conda_pkg("libx", "1.0", "0");
        pkg.package_url = {};
        pkg.channel = "local";
        REQUIRE(sbom::detail::channel_repository_url(pkg) == "https://conda.anaconda.org/local");
    }

    SECTION("bare channel name resolves against the default channel alias")
    {
        auto pkg = conda_pkg("zlib", "1.2.13", "hcfbbd0e_0");
        pkg.package_url = {};
        pkg.channel = "conda-forge";
        REQUIRE(sbom::detail::channel_repository_url(pkg) == "https://conda.anaconda.org/conda-forge");
    }

    SECTION("empty channel returns the default repository")
    {
        auto pkg = conda_pkg("zlib", "1.2.13", "hcfbbd0e_0");
        pkg.package_url = {};
        pkg.channel = {};
        REQUIRE(sbom::detail::channel_repository_url(pkg) == "https://conda.anaconda.org");
    }

    SECTION("scheme-bearing channel with trailing slash is rstripped")
    {
        auto pkg = conda_pkg("zlib", "1.2.13", "hcfbbd0e_0");
        pkg.package_url = {};
        pkg.channel = "https://my.mirror.example/chan/";
        REQUIRE(sbom::detail::channel_repository_url(pkg) == "https://my.mirror.example/chan");
    }

    SECTION("raw fallback when channel_home_url returns nullopt and channel has scheme")
    {
        auto pkg = conda_pkg("libx", "1.0", "0");
        pkg.package_url = {};
        pkg.channel = "http://user@40email.com:test@localhost";
        REQUIRE(sbom::detail::channel_repository_url(pkg) == "http://user@40email.com:test@localhost");
    }

    SECTION("raw fallback with trailing slash when channel_home_url returns nullopt")
    {
        auto pkg = conda_pkg("libx", "1.0", "0");
        pkg.package_url = {};
        pkg.channel = "http://user@40email.com:test@localhost/";
        REQUIRE(sbom::detail::channel_repository_url(pkg) == "http://user@40email.com:test@localhost");
    }
}

TEST_CASE("sbom::detail::collect_qualifiers")
{
    SECTION("all fields and no repository_url")
    {
        const auto pkg = conda_pkg("zlib", "1.2.13", "hcfbbd0e_0", zlib_sha256);
        const auto expected = "?build=hcfbbd0e_0&build_number=0&subdir=linux-64"
                              "&type=conda&checksum=sha256:"
                              + zlib_sha256;
        REQUIRE(sbom::detail::collect_qualifiers(pkg, std::nullopt) == expected);
    }

    SECTION("checksum is lowercased")
    {
        auto pkg = conda_pkg("zlib", "1.2.13", "hcfbbd0e_0", "ABCD1234EF");
        REQUIRE(
            sbom::detail::collect_qualifiers(pkg, std::nullopt)
            == "?build=hcfbbd0e_0&build_number=0&subdir=linux-64&type=conda&checksum=sha256:abcd1234ef"
        );
    }

    SECTION("repository_url is appended last and percent-encoded")
    {
        const auto pkg = conda_pkg("zlib", "1.2.13", "hcfbbd0e_0");
        REQUIRE(
            sbom::detail::collect_qualifiers(
                pkg,
                std::string_view{ "https://conda.anaconda.org/conda-forge" }
            )
            == "?build=hcfbbd0e_0&build_number=0&subdir=linux-64&type=conda"
               "&repository_url=https%3A%2F%2Fconda.anaconda.org%2Fconda-forge"
        );
    }

    SECTION("empty build string omits the build qualifier")
    {
        auto pkg = conda_pkg("zlib", "1.2.13", "");
        REQUIRE(
            sbom::detail::collect_qualifiers(pkg, std::nullopt)
            == "?build_number=0&subdir=linux-64&type=conda"
        );
    }

    SECTION("empty platform omits the subdir qualifier")
    {
        auto pkg = conda_pkg("zlib", "1.2.13", "hcfbbd0e_0");
        pkg.platform = {};
        REQUIRE(
            sbom::detail::collect_qualifiers(pkg, std::nullopt)
            == "?build=hcfbbd0e_0&build_number=0&type=conda"
        );
    }

    SECTION("absent sha256 omits the checksum qualifier")
    {
        const auto pkg = conda_pkg("zlib", "1.2.13", "hcfbbd0e_0");
        REQUIRE(
            sbom::detail::collect_qualifiers(pkg, std::nullopt)
            == "?build=hcfbbd0e_0&build_number=0&subdir=linux-64&type=conda"
        );
    }

    SECTION("type is tar.bz2 when the package_url ends with .tar.bz2")
    {
        auto pkg = conda_pkg("openssl", "3.0.0", "h0b41bf4_0");
        pkg.filename = {};
        pkg.package_url = "https://conda.anaconda.org/conda-forge/linux-64/"
                          "openssl-3.0.0-h0b41bf4_0.tar.bz2";
        REQUIRE(
            sbom::detail::collect_qualifiers(pkg, std::nullopt)
            == "?build=h0b41bf4_0&build_number=0&subdir=linux-64&type=tar.bz2"
        );
    }

    SECTION("minimal package still emits build_number")
    {
        auto pkg = conda_pkg("zlib", "1.2.13", "");
        pkg.platform = {};
        pkg.filename = {};
        pkg.package_url = {};
        REQUIRE(sbom::detail::collect_qualifiers(pkg, std::nullopt) == "?build_number=0");
    }
}

TEST_CASE("make_conda_purl default repository uses the channel as namespace")
{
    const auto pkg = conda_pkg("zlib", "1.2.13", "hcfbbd0e_0", zlib_sha256);
    const auto purl = sbom::make_conda_purl(pkg);
    const auto expected = "pkg:conda/conda-forge/zlib@1.2.13?build=hcfbbd0e_0&build_number=0"
                          "&subdir=linux-64&type=conda&checksum=sha256:"
                          + zlib_sha256;
    REQUIRE(purl == expected);
}

TEST_CASE("make_conda_purl tar.bz2 archive type")
{
    auto pkg = conda_pkg("openssl", "3.0.0", "h0b41bf4_0");
    pkg.package_url = "https://conda.anaconda.org/conda-forge/linux-64/"
                      "openssl-3.0.0-h0b41bf4_0.tar.bz2";
    pkg.filename = "openssl-3.0.0-h0b41bf4_0.tar.bz2";
    REQUIRE(
        sbom::make_conda_purl(pkg)
        == "pkg:conda/conda-forge/openssl@3.0.0?build=h0b41bf4_0"
           "&build_number=0&subdir=linux-64&type=tar.bz2"
    );
}

TEST_CASE("make_conda_purl custom repository uses repository_url")
{
    auto pkg = conda_pkg("numpy", "1.26.0", "py310h01a7c6b_0");
    pkg.channel = "repo.anaconda.com/pkgs/main";
    pkg.package_url = "https://repo.anaconda.com/pkgs/main/linux-64/"
                      "numpy-1.26.0-py310h01a7c6b_0.conda";
    const auto purl = sbom::make_conda_purl(pkg);
    REQUIRE(
        purl
        == "pkg:conda/numpy@1.26.0?build=py310h01a7c6b_0&build_number=0"
           "&subdir=linux-64&type=conda"
           "&repository_url=https%3A%2F%2Frepo.anaconda.com%2Fpkgs%2Fmain"
    );
}

TEST_CASE("make_conda_purl subchannels use repository_url")
{
    auto pkg = conda_pkg("libx", "1.0", "0");
    pkg.package_url = "https://conda.anaconda.org/conda-forge/label/dev/linux-64/libx-1.0-0.conda";
    REQUIRE(
        sbom::make_conda_purl(pkg)
        == "pkg:conda/libx@1.0?build=0&build_number=0&subdir=linux-64&type=conda"
           "&repository_url=https%3A%2F%2Fconda.anaconda.org%2Fconda-forge%2Flabel%2Fdev"
    );
}

TEST_CASE("make_conda_purl banned namespaces use repository_url")
{
    auto pkg = conda_pkg("libx", "1.0", "0", {}, "defaults");
    pkg.package_url = "https://conda.anaconda.org/defaults/linux-64/libx-1.0-0.conda";
    REQUIRE(
        sbom::make_conda_purl(pkg)
        == "pkg:conda/libx@1.0?build=0&build_number=0&subdir=linux-64&type=conda"
           "&repository_url=https%3A%2F%2Fconda.anaconda.org%2Fdefaults"
    );
}

TEST_CASE("make_conda_purl percent encodes version")
{
    const auto pkg = conda_pkg("blas", "1!1.2.3+openblas", "0");
    REQUIRE(
        sbom::make_conda_purl(pkg)
        == "pkg:conda/conda-forge/blas@1%211.2.3%2Bopenblas?build=0&build_number=0"
           "&subdir=linux-64&type=conda"
    );
}

TEST_CASE("make_conda_purl bare channel name falls back to namespace")
{
    auto pkg = conda_pkg("zlib", "1.2.13", "hcfbbd0e_0");
    pkg.channel = "conda-forge";
    pkg.package_url = {};
    REQUIRE(
        sbom::make_conda_purl(pkg)
        == "pkg:conda/conda-forge/zlib@1.2.13?build=hcfbbd0e_0&build_number=0"
           "&subdir=linux-64&type=conda"
    );
}

TEST_CASE("make_conda_purl prefix.dev mirror uses repository_url")
{
    auto pkg = conda_pkg("numpy", "2.5.2", "py313h0000000_0");
    pkg.channel = "https://prefix.dev/conda-forge";
    pkg.package_url = "https://prefix.dev/conda-forge/linux-64/numpy-2.5.2-py313h0000000_0.conda";
    REQUIRE(
        sbom::make_conda_purl(pkg)
        == "pkg:conda/numpy@2.5.2?build=py313h0000000_0&build_number=0"
           "&subdir=linux-64&type=conda"
           "&repository_url=https%3A%2F%2Fprefix.dev%2Fconda-forge"
    );
}

TEST_CASE("make_conda_purl commercial repository matches the CEP-49 normative example")
{
    auto pkg = conda_pkg(
        "numpy",
        "2.5.0",
        "py312h0000000_0",
        {},
        "https://repo.anaconda.cloud/repo/main",
        "win-64"
    );
    pkg.package_url = "https://repo.anaconda.cloud/repo/main/win-64/"
                      "numpy-2.5.0-py312h0000000_0.conda";
    REQUIRE(
        sbom::make_conda_purl(pkg)
        == "pkg:conda/numpy@2.5.0?build=py312h0000000_0&build_number=0"
           "&subdir=win-64&type=conda"
           "&repository_url=https%3A%2F%2Frepo.anaconda.cloud%2Frepo%2Fmain"
    );
}

TEST_CASE("make_conda_purl Anaconda main channel uses the main namespace")
{
    auto pkg = conda_pkg("python", "3.13", "h0000000_0", {}, "main");
    pkg.package_url = "https://conda.anaconda.org/main/linux-64/python-3.13-h0000000_0.conda";
    REQUIRE(
        sbom::make_conda_purl(pkg)
        == "pkg:conda/main/python@3.13?build=h0000000_0&build_number=0"
           "&subdir=linux-64&type=conda"
    );
}

TEST_CASE("make_conda_purl bare defaults channel uses the default repository")
{
    auto pkg = conda_pkg("libx", "1.0", "0", {}, "defaults");
    pkg.package_url = {};
    REQUIRE(
        sbom::make_conda_purl(pkg)
        == "pkg:conda/libx@1.0?build=0&build_number=0&subdir=linux-64&type=conda"
           "&repository_url=https%3A%2F%2Fconda.anaconda.org%2Fdefaults"
    );
}

TEST_CASE("make_conda_purl bare local channel uses the default repository")
{
    auto pkg = conda_pkg("libx", "1.0", "0", {}, "local");
    pkg.package_url = {};
    REQUIRE(
        sbom::make_conda_purl(pkg)
        == "pkg:conda/libx@1.0?build=0&build_number=0&subdir=linux-64&type=conda"
           "&repository_url=https%3A%2F%2Fconda.anaconda.org%2Flocal"
    );
}

TEST_CASE("make_conda_purl bare subdir-named channels use repository_url")
{
    SECTION("linux-64")
    {
        auto pkg = conda_pkg("libx", "1.0", "0", {}, "linux-64");
        pkg.package_url = {};
        REQUIRE(
            sbom::make_conda_purl(pkg)
            == "pkg:conda/libx@1.0?build=0&build_number=0&subdir=linux-64&type=conda"
               "&repository_url=https%3A%2F%2Fconda.anaconda.org%2Flinux-64"
        );
    }

    SECTION("noarch")
    {
        auto pkg = conda_pkg("libx", "1.0", "0", {}, "noarch");
        pkg.package_url = {};
        REQUIRE(
            sbom::make_conda_purl(pkg)
            == "pkg:conda/libx@1.0?build=0&build_number=0&subdir=linux-64&type=conda"
               "&repository_url=https%3A%2F%2Fconda.anaconda.org%2Fnoarch"
        );
    }
}

TEST_CASE("make_conda_purl schemeless multi-segment channel resolves against channel alias")
{
    auto pkg = conda_pkg("libx", "1.0", "0", {}, "conda-forge/label/dev");
    pkg.package_url = {};
    REQUIRE(
        sbom::make_conda_purl(pkg)
        == "pkg:conda/libx@1.0?build=0&build_number=0&subdir=linux-64&type=conda"
           "&repository_url=https%3A%2F%2Fconda.anaconda.org%2Fconda-forge%2Flabel%2Fdev"
    );
}

TEST_CASE("make_conda_purl file channel uses repository_url")
{
    auto pkg = conda_pkg("libx", "1.0", "0", {}, "file:///srv/mirror/chan");
    pkg.package_url = {};
    REQUIRE(
        sbom::make_conda_purl(pkg)
        == "pkg:conda/libx@1.0?build=0&build_number=0&subdir=linux-64&type=conda"
           "&repository_url=file%3A%2F%2F%2Fsrv%2Fmirror%2Fchan"
    );
}

TEST_CASE("make_conda_purl lowercases the package name")
{
    auto pkg = conda_pkg("NumPy", "1.0", "0");
    REQUIRE(
        sbom::make_conda_purl(pkg)
        == "pkg:conda/conda-forge/numpy@1.0?build=0&build_number=0&subdir=linux-64&type=conda"
    );
}

TEST_CASE("make_conda_purl lowercases the namespace")
{
    auto pkg = conda_pkg("zlib", "1.0", "0");
    pkg.package_url = "https://conda.anaconda.org/Conda-Forge/linux-64/zlib-1.0-0.conda";
    REQUIRE(
        sbom::make_conda_purl(pkg)
        == "pkg:conda/conda-forge/zlib@1.0?build=0&build_number=0&subdir=linux-64&type=conda"
    );
}

TEST_CASE("make_conda_purl empty build string omits the build qualifier")
{
    auto pkg = conda_pkg("zlib", "1.0", "");
    REQUIRE(
        sbom::make_conda_purl(pkg)
        == "pkg:conda/conda-forge/zlib@1.0?build_number=0&subdir=linux-64&type=conda"
    );
}

TEST_CASE("make_conda_purl absent sha256 omits the checksum qualifier")
{
    const auto pkg = conda_pkg("zlib", "1.0", "abc");
    REQUIRE(
        sbom::make_conda_purl(pkg)
        == "pkg:conda/conda-forge/zlib@1.0?build=abc&build_number=0&subdir=linux-64&type=conda"
    );
}

TEST_CASE("make_conda_purl empty channel and package_url use the default repository")
{
    auto pkg = conda_pkg("zlib", "1.0", "0");
    pkg.channel = {};
    pkg.package_url = {};
    REQUIRE(
        sbom::make_conda_purl(pkg)
        == "pkg:conda/zlib@1.0?build=0&build_number=0&subdir=linux-64&type=conda"
           "&repository_url=https%3A%2F%2Fconda.anaconda.org"
    );
}

TEST_CASE("make_conda_purl infers tar.bz2 type from package_url")
{
    auto pkg = conda_pkg("openssl", "3.0.0", "0");
    pkg.filename = {};
    pkg.package_url = "https://conda.anaconda.org/conda-forge/linux-64/"
                      "openssl-3.0.0-0.tar.bz2";
    REQUIRE(
        sbom::make_conda_purl(pkg)
        == "pkg:conda/conda-forge/openssl@3.0.0?build=0&build_number=0"
           "&subdir=linux-64&type=tar.bz2"
    );
}

TEST_CASE("make_conda_purl schemed channel that fails to parse uses raw repository_url")
{
    auto pkg = conda_pkg("libx", "1.0", "0");
    pkg.package_url = {};
    pkg.channel = "http://user@40email.com:test@localhost";
    REQUIRE(
        sbom::make_conda_purl(pkg)
        == "pkg:conda/libx@1.0?build=0&build_number=0&subdir=linux-64&type=conda"
           "&repository_url=http%3A%2F%2Fuser%4040email.com%3Atest%40localhost"
    );
}

TEST_CASE("make_conda_purl schemed package_url that fails to parse falls back to channel")
{
    auto pkg = conda_pkg("libx", "1.0", "0");
    pkg.package_url = "http://user@40email.com:test@localhost/linux-64/libx-1.0-0.conda";
    pkg.channel = "http://user@40email.com:test@localhost";
    REQUIRE(
        sbom::make_conda_purl(pkg)
        == "pkg:conda/libx@1.0?build=0&build_number=0&subdir=linux-64&type=conda"
           "&repository_url=http%3A%2F%2Fuser%4040email.com%3Atest%40localhost"
    );
}
