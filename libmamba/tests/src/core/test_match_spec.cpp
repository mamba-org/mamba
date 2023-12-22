// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <doctest/doctest.h>

#include "mamba/core/match_spec.hpp"
#include "mamba/fs/filesystem.hpp"
#include "mamba/util/build.hpp"
#include "mamba/util/string.hpp"

using namespace mamba;

TEST_SUITE("MatchSpec")
{
    using PlatformSet = typename util::flat_set<std::string>;

    TEST_CASE("parse_version_build")
    {
        std::string v, b;
        // >>> _parse_version_plus_build("=1.2.3 0")
        // ('=1.2.3', '0')
        // >>> _parse_version_plus_build("1.2.3=0")
        // ('1.2.3', '0')
        // >>> _parse_version_plus_build(">=1.0 , < 2.0 py34_0")
        // ('>=1.0,<2.0', 'py34_0')
        // >>> _parse_version_plus_build(">=1.0 , < 2.0 =py34_0")
        // ('>=1.0,<2.0', 'py34_0')
        // >>> _parse_version_plus_build("=1.2.3 ")
        // ('=1.2.3', None)
        // >>> _parse_version_plus_build(">1.8,<2|==1.7")
        // ('>1.8,<2|==1.7', None)
        // >>> _parse_version_plus_build("* openblas_0")
        // ('*', 'openblas_0')
        // >>> _parse_version_plus_build("* *")
        // ('*', '*')
        std::tie(v, b) = MatchSpec::parse_version_and_build("=1.2.3 0");
        CHECK_EQ(v, "=1.2.3");
        CHECK_EQ(b, "0");
        std::tie(v, b) = MatchSpec::parse_version_and_build("=1.2.3=0");
        CHECK_EQ(v, "=1.2.3");
        CHECK_EQ(b, "0");
        std::tie(v, b) = MatchSpec::parse_version_and_build(">=1.0 , < 2.0 py34_0");
        CHECK_EQ(v, ">=1.0,<2.0");
        CHECK_EQ(b, "py34_0");
        std::tie(v, b) = MatchSpec::parse_version_and_build(">=1.0 , < 2.0 =py34_0");
        CHECK_EQ(v, ">=1.0,<2.0");
        CHECK_EQ(b, "py34_0");
        std::tie(v, b) = MatchSpec::parse_version_and_build("=1.2.3 ");
        CHECK_EQ(v, "=1.2.3");
        CHECK_EQ(b, "");
        std::tie(v, b) = MatchSpec::parse_version_and_build(">1.8,<2|==1.7");
        CHECK_EQ(v, ">1.8,<2|==1.7");
        CHECK_EQ(b, "");
        std::tie(v, b) = MatchSpec::parse_version_and_build("* openblas_0");
        CHECK_EQ(v, "*");
        CHECK_EQ(b, "openblas_0");
        std::tie(v, b) = MatchSpec::parse_version_and_build("* *");
        CHECK_EQ(v, "*");
        CHECK_EQ(b, "*");
    }

    TEST_CASE("parse")
    {
        {
            auto ms = MatchSpec::parse("xtensor==0.12.3");
            CHECK_EQ(ms.version, "0.12.3");
            CHECK_EQ(ms.name(), "xtensor");
        }
        {
            auto ms = MatchSpec::parse("");
            CHECK_EQ(ms.version, "");
            CHECK_EQ(ms.name(), "");
        }
        {
            auto ms = MatchSpec::parse("ipykernel");
            CHECK_EQ(ms.version, "");
            CHECK_EQ(ms.name(), "ipykernel");
        }
        {
            auto ms = MatchSpec::parse("ipykernel ");
            CHECK_EQ(ms.version, "");
            CHECK_EQ(ms.name(), "ipykernel");
        }
        {
            auto ms = MatchSpec::parse("numpy 1.7*");
            CHECK_EQ(ms.version, "1.7*");
            CHECK_EQ(ms.name(), "numpy");
            CHECK_EQ(ms.conda_build_form(), "numpy 1.7*");
            CHECK_EQ(ms.str(), "numpy=1.7");
        }
        {
            auto ms = MatchSpec::parse("numpy[version='1.7|1.8']");
            // TODO!
            // CHECK_EQ(ms.version, "1.7|1.8");
            CHECK_EQ(ms.name(), "numpy");
            CHECK_EQ(ms.brackets["version"], "1.7|1.8");
            CHECK_EQ(ms.str(), "numpy[version='1.7|1.8']");
        }
        {
            auto ms = MatchSpec::parse("conda-forge/linux-64::xtensor==0.12.3");
            CHECK_EQ(ms.version, "0.12.3");
            CHECK_EQ(ms.name(), "xtensor");
            REQUIRE(ms.channel.has_value());
            CHECK_EQ(ms.channel->location(), "conda-forge");
            CHECK_EQ(ms.channel->platform_filters(), PlatformSet{ "linux-64" });
            CHECK_EQ(ms.optional, false);
        }
        {
            auto ms = MatchSpec::parse("conda-forge::foo[build=3](target=blarg,optional)");
            CHECK_EQ(ms.version, "");
            CHECK_EQ(ms.name(), "foo");
            REQUIRE(ms.channel.has_value());
            CHECK_EQ(ms.channel->location(), "conda-forge");
            CHECK_EQ(ms.brackets["build"], "3");
            CHECK_EQ(ms.parens["target"], "blarg");
            CHECK_EQ(ms.optional, true);
        }
        {
            auto ms = MatchSpec::parse("python[build_number=3]");
            CHECK_EQ(ms.name(), "python");
            CHECK_EQ(ms.brackets["build_number"], "3");
            CHECK_EQ(ms.build_number, "3");
        }
        {
            auto ms = MatchSpec::parse("python[build_number='<=3']");
            CHECK_EQ(ms.name(), "python");
            CHECK_EQ(ms.brackets["build_number"], "<=3");
            CHECK_EQ(ms.build_number, "<=3");
        }
        {
            auto ms = MatchSpec::parse(
                "https://conda.anaconda.org/conda-forge/linux-64/_libgcc_mutex-0.1-conda_forge.tar.bz2"
            );
            CHECK_EQ(ms.name(), "_libgcc_mutex");
            CHECK_EQ(ms.version, "0.1");
            CHECK_EQ(ms.build_string, "conda_forge");
            CHECK_EQ(
                ms.url,
                "https://conda.anaconda.org/conda-forge/linux-64/_libgcc_mutex-0.1-conda_forge.tar.bz2"
            );
            CHECK_EQ(ms.fn, "_libgcc_mutex-0.1-conda_forge.tar.bz2");
        }
        {
            auto ms = MatchSpec::parse(
                "/home/randomguy/Downloads/linux-64/_libgcc_mutex-0.1-conda_forge.tar.bz2"
            );
            CHECK_EQ(ms.name(), "_libgcc_mutex");
            CHECK_EQ(ms.version, "0.1");
            CHECK_EQ(ms.build_string, "conda_forge");
            if (util::on_win)
            {
                std::string driveletter = fs::absolute(fs::u8path("/")).string().substr(0, 1);
                CHECK_EQ(
                    ms.url,
                    util::concat(
                        "file://",
                        driveletter,
                        ":/home/randomguy/Downloads/linux-64/_libgcc_mutex-0.1-conda_forge.tar.bz2"
                    )
                );
            }
            else
            {
                CHECK_EQ(
                    ms.url,
                    "file:///home/randomguy/Downloads/linux-64/_libgcc_mutex-0.1-conda_forge.tar.bz2"
                );
            }

            CHECK_EQ(ms.fn, "_libgcc_mutex-0.1-conda_forge.tar.bz2");
        }
        {
            auto ms = MatchSpec::parse(
                "xtensor[url=file:///home/wolfv/Downloads/xtensor-0.21.4-hc9558a2_0.tar.bz2]"
            );
            CHECK_EQ(ms.name(), "xtensor");
            CHECK_EQ(
                ms.brackets["url"],
                "file:///home/wolfv/Downloads/xtensor-0.21.4-hc9558a2_0.tar.bz2"
            );
            CHECK_EQ(ms.url, "file:///home/wolfv/Downloads/xtensor-0.21.4-hc9558a2_0.tar.bz2");
        }
        {
            auto ms = MatchSpec::parse("foo=1.0=2");
            CHECK_EQ(ms.conda_build_form(), "foo 1.0 2");
            CHECK_EQ(ms.str(), "foo==1.0=2");
        }
        {
            auto ms = MatchSpec::parse("foo=1.0=2[md5=123123123, license=BSD-3, fn='test 123.tar.bz2']"
            );
            CHECK_EQ(ms.conda_build_form(), "foo 1.0 2");
            CHECK_EQ(ms.str(), "foo==1.0=2[md5=123123123,license=BSD-3,fn='test 123.tar.bz2']");
        }
        {
            auto ms = MatchSpec::parse(
                "foo=1.0=2[md5=123123123, license=BSD-3, fn='test 123.tar.bz2', url='abcdef']"
            );
            CHECK_EQ(ms.conda_build_form(), "foo 1.0 2");
            CHECK_EQ(ms.str(), "foo==1.0=2[url=abcdef,md5=123123123,license=BSD-3]");
        }
        {
            auto ms = MatchSpec::parse("libblas=*=*mkl");
            CHECK_EQ(ms.conda_build_form(), "libblas * *mkl");
            // CHECK_EQ(ms.str(), "foo==1.0=2");
        }
        {
            auto ms = MatchSpec::parse("libblas=0.15*");
            CHECK_EQ(ms.conda_build_form(), "libblas 0.15*");
        }
        {
            auto ms = MatchSpec::parse("xtensor =0.15*");
            CHECK_EQ(ms.conda_build_form(), "xtensor 0.15*");
            CHECK_EQ(ms.str(), "xtensor=0.15");
        }
        {
            auto ms = MatchSpec::parse("numpy=1.20");
            CHECK_EQ(ms.str(), "numpy=1.20");
        }

        {
            auto ms = MatchSpec::parse("conda-forge::tzdata");
            CHECK_EQ(ms.str(), "conda-forge::tzdata");
        }
        {
            auto ms = MatchSpec::parse("conda-forge::noarch/tzdata");
            CHECK_EQ(ms.str(), "conda-forge::noarch/tzdata");
        }
        {
            auto ms = MatchSpec::parse("pkgs/main::tzdata");
            CHECK_EQ(ms.str(), "pkgs/main::tzdata");
        }
        {
            auto ms = MatchSpec::parse("pkgs/main/noarch::tzdata");
            CHECK_EQ(ms.str(), "pkgs/main[noarch]::tzdata");
        }
        {
            auto ms = MatchSpec::parse("conda-forge[noarch]::tzdata[subdir=linux64]");
            CHECK_EQ(ms.str(), "conda-forge[noarch]::tzdata");
        }
        {
            auto ms = MatchSpec::parse("conda-forge::tzdata[subdir=mamba-37]");
            CHECK_EQ(ms.str(), "conda-forge[mamba-37]::tzdata");
        }
    }

    TEST_CASE("is_simple")
    {
        {
            auto ms = MatchSpec::parse("libblas");
            CHECK(ms.is_simple());
        }
        {
            auto ms = MatchSpec::parse("libblas=12.9=abcdef");
            CHECK_FALSE(ms.is_simple());
        }
        {
            auto ms = MatchSpec::parse("libblas=0.15*");
            CHECK_FALSE(ms.is_simple());
        }
        {
            auto ms = MatchSpec::parse("libblas[version=12.2]");
            CHECK_FALSE(ms.is_simple());
        }
        {
            auto ms = MatchSpec::parse("xtensor =0.15*");
            CHECK_FALSE(ms.is_simple());
        }
    }
}
