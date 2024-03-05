// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <doctest/doctest.h>

#include "mamba/specs/match_spec.hpp"

using namespace mamba;
using namespace mamba::specs;

TEST_SUITE("specs::match_spec")
{
    using PlatformSet = typename util::flat_set<std::string>;

    TEST_CASE("parse")
    {
        SUBCASE("xtensor==0.12.3")
        {
            auto ms = MatchSpec::parse("xtensor==0.12.3").value();
            CHECK_EQ(ms.version().str(), "==0.12.3");
            CHECK_EQ(ms.name().str(), "xtensor");
        }

        SUBCASE("<empty>")
        {
            auto ms = MatchSpec::parse("").value();
            CHECK_EQ(ms.version().str(), "=*");
            CHECK_EQ(ms.name().str(), "*");
        }

        SUBCASE("ipykernel ")
        {
            auto ms = MatchSpec::parse("ipykernel").value();
            CHECK_EQ(ms.version().str(), "=*");
            CHECK_EQ(ms.name().str(), "ipykernel");
        }

        SUBCASE("ipykernel ")
        {
            auto ms = MatchSpec::parse("ipykernel ").value();
            CHECK_EQ(ms.version().str(), "=*");
            CHECK_EQ(ms.name().str(), "ipykernel");
        }

        SUBCASE("numpy 1.7*")
        {
            auto ms = MatchSpec::parse("numpy 1.7*").value();
            CHECK_EQ(ms.version().str(), "=1.7");
            CHECK_EQ(ms.name().str(), "numpy");
            CHECK_EQ(ms.conda_build_form(), "numpy 1.7.*");
            CHECK_EQ(ms.str(), "numpy=1.7");
        }

        SUBCASE("conda-forge:pypi:xtensor==0.12.3")
        {
            auto ms = MatchSpec::parse("conda-forge:pypi:xtensor==0.12.3").value();
            CHECK_EQ(ms.name().str(), "xtensor");
            CHECK_EQ(ms.channel().value().str(), "conda-forge");
            CHECK_EQ(ms.name_space(), "pypi");
        }

        SUBCASE("conda-forge/linux-64::xtensor==0.12.3")
        {
            auto ms = MatchSpec::parse("numpy[version='1.7|1.8']").value();
            CHECK_EQ(ms.name().str(), "numpy");
            CHECK_EQ(ms.version().str(), "==1.7|==1.8");
            CHECK_EQ(ms.str(), "numpy[version='==1.7|==1.8']");
        }

        SUBCASE("conda-forge/linux-64::xtensor==0.12.3")
        {
            auto ms = MatchSpec::parse("conda-forge/linux-64::xtensor==0.12.3").value();
            CHECK_EQ(ms.version().str(), "==0.12.3");
            CHECK_EQ(ms.name().str(), "xtensor");
            REQUIRE(ms.channel().has_value());
            CHECK_EQ(ms.channel()->location(), "conda-forge");
            CHECK_EQ(ms.channel()->platform_filters(), PlatformSet{ "linux-64" });
            CHECK_EQ(ms.optional(), false);
        }

        SUBCASE("conda-forge::foo[build=3](target=blarg,optional)")
        {
            auto ms = MatchSpec::parse("conda-forge::foo[build=3](target=blarg,optional)").value();
            CHECK_EQ(ms.version().str(), "=*");
            CHECK_EQ(ms.name().str(), "foo");
            REQUIRE(ms.channel().has_value());
            CHECK_EQ(ms.channel()->location(), "conda-forge");
            CHECK_EQ(ms.build_string().str(), "3");
            CHECK_EQ(ms.optional(), true);
        }

        SUBCASE("python[build_number=3]")
        {
            auto ms = MatchSpec::parse("python[build_number=3]").value();
            CHECK_EQ(ms.name().str(), "python");
            CHECK_EQ(ms.build_number().str(), "=3");
        }

        SUBCASE(R"(blas[track_features="mkl avx"])")
        {
            auto ms = MatchSpec::parse(R"(blas[track_features="mkl avx"])").value();
            CHECK_EQ(ms.name().str(), "blas");
            CHECK_EQ(ms.track_features().value().get(), MatchSpec::string_set{ "avx", "mkl" });
        }

        SUBCASE("python[build_number='<=3']")
        {
            auto ms = MatchSpec::parse("python[build_number='<=3']").value();
            CHECK_EQ(ms.name().str(), "python");
            CHECK_EQ(ms.build_number().str(), "<=3");
        }

        SUBCASE("https://conda.anaconda.org/conda-forge/linux-64/ncurses-6.4-h59595ed_2.conda#7dbaa197d7ba6032caf7ae7f32c1efa0"
        )
        {
            auto ms = MatchSpec::parse(
                          "https://conda.anaconda.org/conda-forge/linux-64/ncurses-6.4-h59595ed_2.conda"
                          "#7dbaa197d7ba6032caf7ae7f32c1efa0"
            )
                          .value();
            CHECK_EQ(ms.name().str(), "ncurses");
            CHECK_EQ(ms.version().str(), "==6.4");
            CHECK_EQ(ms.build_string().str(), "h59595ed_2");
            CHECK_EQ(
                ms.channel().value().str(),
                "https://conda.anaconda.org/conda-forge/linux-64/ncurses-6.4-h59595ed_2.conda"
            );
            CHECK_EQ(ms.filename(), "ncurses-6.4-h59595ed_2.conda");
            CHECK_EQ(ms.md5(), "7dbaa197d7ba6032caf7ae7f32c1efa0");
        }

        SUBCASE("https://conda.anaconda.org/conda-forge/linux-64/_libgcc_mutex-0.1-conda_forge.tar.bz2")
        {
            auto ms = MatchSpec::parse(
                          "https://conda.anaconda.org/conda-forge/linux-64/_libgcc_mutex-0.1-conda_forge.tar.bz2"
            )
                          .value();
            CHECK_EQ(ms.name().str(), "_libgcc_mutex");
            CHECK_EQ(ms.version().str(), "==0.1");
            CHECK_EQ(ms.build_string().str(), "conda_forge");
            CHECK_EQ(
                ms.channel().value().str(),
                "https://conda.anaconda.org/conda-forge/linux-64/_libgcc_mutex-0.1-conda_forge.tar.bz2"
            );
            CHECK_EQ(ms.filename(), "_libgcc_mutex-0.1-conda_forge.tar.bz2");
        }

        SUBCASE("https://conda.anaconda.org/conda-forge/linux-64/libgcc-ng-11.2.0-h1d223b6_13.tar.bz2")
        {
            auto ms = MatchSpec::parse(
                          "https://conda.anaconda.org/conda-forge/linux-64/libgcc-ng-11.2.0-h1d223b6_13.tar.bz2"
            )
                          .value();
            CHECK_EQ(ms.name().str(), "libgcc-ng");
            CHECK_EQ(ms.version().str(), "==11.2.0");
            CHECK_EQ(ms.build_string().str(), "h1d223b6_13");
            CHECK_EQ(
                ms.channel().value().str(),
                "https://conda.anaconda.org/conda-forge/linux-64/libgcc-ng-11.2.0-h1d223b6_13.tar.bz2"
            );
            CHECK_EQ(ms.filename(), "libgcc-ng-11.2.0-h1d223b6_13.tar.bz2");
        }

        SUBCASE("https://conda.anaconda.org/conda-canary/linux-64/conda-4.3.21.post699+1dab973-py36h4a561cd_0.tar.bz2"
        )
        {
            auto ms = MatchSpec::parse(
                          "https://conda.anaconda.org/conda-canary/linux-64/conda-4.3.21.post699+1dab973-py36h4a561cd_0.tar.bz2"
            )
                          .value();
            CHECK_EQ(ms.name().str(), "conda");
            CHECK_EQ(ms.version().str(), "==4.3.21.0post699+1dab973");  // Note the ``.0post``
            CHECK_EQ(ms.build_string().str(), "py36h4a561cd_0");
        }

        SUBCASE("/home/randomguy/Downloads/linux-64/_libgcc_mutex-0.1-conda_forge.tar.bz2")
        {
            auto ms = MatchSpec::parse(
                          "/home/randomguy/Downloads/linux-64/_libgcc_mutex-0.1-conda_forge.tar.bz2"
            )
                          .value();
            CHECK_EQ(ms.name().str(), "_libgcc_mutex");
            CHECK_EQ(ms.version().str(), "==0.1");
            CHECK_EQ(ms.build_string().str(), "conda_forge");
            CHECK_EQ(
                ms.channel().value().str(),
                "/home/randomguy/Downloads/linux-64/_libgcc_mutex-0.1-conda_forge.tar.bz2"
            );
            CHECK_EQ(ms.filename(), "_libgcc_mutex-0.1-conda_forge.tar.bz2");
        }

        SUBCASE("xtensor[url=file:///home/wolfv/Downloads/xtensor-0.21.4-hc9558a2_0.tar.bz2]")
        {
            auto ms = MatchSpec::parse(
                          "xtensor[url=file:///home/wolfv/Downloads/xtensor-0.21.4-hc9558a2_0.tar.bz2]"
            )
                          .value();
            CHECK_EQ(ms.name().str(), "xtensor");
            CHECK_EQ(
                ms.channel().value().str(),
                "file:///home/wolfv/Downloads/xtensor-0.21.4-hc9558a2_0.tar.bz2"
            );
        }

        SUBCASE("foo=1.0=2")
        {
            auto ms = MatchSpec::parse("foo=1.0=2").value();
            CHECK_EQ(ms.conda_build_form(), "foo 1.0.* 2");
            CHECK_EQ(ms.name().str(), "foo");
            CHECK_EQ(ms.version().str(), "=1.0");
            CHECK_EQ(ms.build_string().str(), "2");
            CHECK_EQ(ms.str(), "foo=1.0=2");
        }

        SUBCASE("foo=1.0=2[md5=123123123, license=BSD-3, fn='test 123.tar.bz2']")
        {
            auto ms = MatchSpec::parse("foo=1.0=2[md5=123123123, license=BSD-3, fn='test 123.tar.bz2']")
                          .value();
            CHECK_EQ(ms.name().str(), "foo");
            CHECK_EQ(ms.version().str(), "=1.0");
            CHECK_EQ(ms.build_string().str(), "2");
            CHECK_EQ(ms.conda_build_form(), "foo 1.0.* 2");
            CHECK_EQ(ms.str(), R"ms(foo=1.0=2[fn="test 123.tar.bz2",md5=123123123,license=BSD-3])ms");
        }

        SUBCASE("foo=1.0=2[md5=123123123, license=BSD-3, fn='test 123.tar.bz2', url='abcdef']")
        {
            auto ms = MatchSpec::parse(
                          "foo=1.0=2[md5=123123123, license=BSD-3, fn='test 123.tar.bz2', url='abcdef']"
            )
                          .value();
            CHECK_EQ(ms.channel().value().str(), "abcdef");
            CHECK_EQ(ms.name().str(), "foo");
            CHECK_EQ(ms.version().str(), "=1.0");
            CHECK_EQ(ms.build_string().str(), "2");
            CHECK_EQ(ms.conda_build_form(), "foo 1.0.* 2");
            CHECK_EQ(
                ms.str(),
                R"ms(abcdef::foo=1.0=2[fn="test 123.tar.bz2",md5=123123123,license=BSD-3])ms"
            );
        }

        SUBCASE(R"(defaults::numpy=1.8=py27_0 [name="pytorch" channel='anaconda',version=">=1.8,<2|1.9", build='3'])"
        )
        {
            auto ms = MatchSpec::parse(
                          R"(defaults::numpy=1.8=py27_0 [name="pytorch" channel='anaconda',version=">=1.8,<2|1.9", build='3'])"
            )
                          .value();
            CHECK_EQ(ms.channel().value().str(), "defaults");
            CHECK_EQ(ms.name().str(), "numpy");
            CHECK_EQ(ms.version().str(), "=1.8");
            CHECK_EQ(ms.build_string().str(), "py27_0");
        }

        SUBCASE(R"(defaults::numpy [ "pytorch" channel='anaconda',version=">=1.8,<2|1.9", build='3'])")
        {
            auto ms = MatchSpec::parse(
                          R"(defaults::numpy [ "pytorch" channel='anaconda',version=">=1.8,<2|1.9", build='3'])"
            )
                          .value();
            CHECK_EQ(ms.channel().value().str(), "defaults");
            CHECK_EQ(ms.name().str(), "numpy");
            CHECK_EQ(ms.version().str(), ">=1.8,(<2|==1.9)");
            CHECK_EQ(ms.build_string().str(), "3");
        }

        SUBCASE("numpy >1.8,<2|==1.7,!=1.9,~=1.7.1 py34_0")
        {
            auto ms = MatchSpec::parse(R"(numpy >1.8,<2|==1.7,!=1.9,~=1.7.1 py34_0)").value();
            CHECK_EQ(ms.name().str(), "numpy");
            CHECK_EQ(ms.version().str(), ">1.8,((<2|==1.7),(!=1.9,~=1.7))");
            CHECK_EQ(ms.build_string().str(), "py34_0");
        }

        SUBCASE("*[md5=fewjaflknd]")
        {
            auto ms = MatchSpec::parse("*[md5=fewjaflknd]").value();
            CHECK(ms.name().is_free());
            CHECK_EQ(ms.md5(), "fewjaflknd");
        }

        SUBCASE("libblas=*=*mkl")
        {
            auto ms = MatchSpec::parse("libblas=*=*mkl").value();
            CHECK_EQ(ms.conda_build_form(), "libblas * *mkl");
            CHECK_EQ(ms.name().str(), "libblas");
            CHECK_EQ(ms.version().str(), "=*");
            CHECK_EQ(ms.build_string().str(), "*mkl");
            // CHECK_EQ(ms.str(), "foo==1.0=2");
        }

        SUBCASE("libblas=0.15*")
        {
            // '*' is part of the version, not the glob
            auto ms = MatchSpec::parse("libblas=0.15*").value();
            CHECK_EQ(ms.conda_build_form(), "libblas 0.15*.*");
            CHECK_EQ(ms.name().str(), "libblas");
            CHECK_EQ(ms.version().str(), "=0.15*");
            CHECK(ms.build_string().is_free());
        }

        SUBCASE("xtensor =0.15*")
        {
            // '*' is part of the version, not the glob
            auto ms = MatchSpec::parse("xtensor =0.15*").value();
            CHECK_EQ(ms.conda_build_form(), "xtensor 0.15*.*");
            CHECK_EQ(ms.str(), "xtensor=0.15*");
            CHECK_EQ(ms.name().str(), "xtensor");
            CHECK_EQ(ms.version().str(), "=0.15*");
            CHECK(ms.build_string().is_free());
        }

        SUBCASE("numpy=1.20")
        {
            auto ms = MatchSpec::parse("numpy=1.20").value();
            CHECK_EQ(ms.str(), "numpy=1.20");
            CHECK_EQ(ms.name().str(), "numpy");
            CHECK_EQ(ms.version().str(), "=1.20");
            CHECK(ms.build_string().is_free());
        }

        SUBCASE("conda-forge::tzdata")
        {
            auto ms = MatchSpec::parse("conda-forge::tzdata").value();
            CHECK_EQ(ms.str(), "conda-forge::tzdata");
            CHECK_EQ(ms.channel().value().str(), "conda-forge");
            CHECK_EQ(ms.name().str(), "tzdata");
            CHECK(ms.version().is_explicitly_free());
            CHECK(ms.build_string().is_free());
        }

        SUBCASE("conda-forge/noarch::tzdata")
        {
            auto ms = MatchSpec::parse("conda-forge/noarch::tzdata").value();
            CHECK_EQ(ms.str(), "conda-forge[noarch]::tzdata");
            CHECK_EQ(ms.channel().value().str(), "conda-forge[noarch]");
            CHECK_EQ(ms.name().str(), "tzdata");
            CHECK(ms.version().is_explicitly_free());
            CHECK(ms.build_string().is_free());
        }

        SUBCASE("conda-forge[noarch]::tzdata")
        {
            auto ms = MatchSpec::parse("conda-forge/noarch::tzdata").value();
            CHECK_EQ(ms.str(), "conda-forge[noarch]::tzdata");
            CHECK_EQ(ms.channel().value().str(), "conda-forge[noarch]");
            CHECK_EQ(ms.name().str(), "tzdata");
            CHECK(ms.version().is_explicitly_free());
            CHECK(ms.build_string().is_free());
        }

        SUBCASE("pkgs/main::tzdata")
        {
            auto ms = MatchSpec::parse("pkgs/main::tzdata").value();
            CHECK_EQ(ms.str(), "pkgs/main::tzdata");
            CHECK_EQ(ms.channel().value().str(), "pkgs/main");
            CHECK_EQ(ms.name().str(), "tzdata");
            CHECK(ms.version().is_explicitly_free());
            CHECK(ms.build_string().is_free());
        }

        SUBCASE("pkgs/main/noarch::tzdata")
        {
            auto ms = MatchSpec::parse("pkgs/main/noarch::tzdata").value();
            CHECK_EQ(ms.str(), "pkgs/main[noarch]::tzdata");
            CHECK_EQ(ms.channel().value().str(), "pkgs/main[noarch]");
            CHECK_EQ(ms.name().str(), "tzdata");
            CHECK(ms.version().is_explicitly_free());
            CHECK(ms.build_string().is_free());
        }

        SUBCASE("conda-forge[noarch]::tzdata[subdir=linux64]")
        {
            auto ms = MatchSpec::parse("conda-forge[noarch]::tzdata[subdir=linux64]").value();
            CHECK_EQ(ms.str(), "conda-forge[noarch]::tzdata");
            CHECK_EQ(ms.channel().value().str(), "conda-forge[noarch]");
            CHECK_EQ(ms.platforms().value().get(), MatchSpec::platform_set{ "noarch" });
            CHECK_EQ(ms.name().str(), "tzdata");
            CHECK(ms.version().is_explicitly_free());
            CHECK(ms.build_string().is_free());
        }

        SUBCASE("conda-forge::tzdata[subdir=mamba-37]")
        {
            auto ms = MatchSpec::parse("conda-forge::tzdata[subdir=mamba-37]").value();
            CHECK_EQ(ms.str(), "conda-forge[mamba-37]::tzdata");
            CHECK_EQ(ms.channel().value().str(), "conda-forge[mamba-37]");
            CHECK_EQ(ms.platforms().value().get(), MatchSpec::platform_set{ "mamba-37" });
            CHECK_EQ(ms.name().str(), "tzdata");
            CHECK(ms.version().is_explicitly_free());
            CHECK(ms.build_string().is_free());
        }

        SUBCASE("conda-canary/linux-64::conda==4.3.21.post699+1dab973=py36h4a561cd_0")
        {
            auto ms = MatchSpec::parse(
                          "conda-canary/linux-64::conda==4.3.21.post699+1dab973=py36h4a561cd_0"
            )
                          .value();
            CHECK_EQ(ms.channel().value().str(), "conda-canary[linux-64]");
            CHECK_EQ(ms.platforms().value().get(), MatchSpec::platform_set{ "linux-64" });
            CHECK_EQ(ms.name().str(), "conda");
            CHECK_EQ(ms.version().str(), "==4.3.21.0post699+1dab973");  // Not ``.0post`` diff
            CHECK_EQ(ms.build_string().str(), "py36h4a561cd_0");
        }
    }

    TEST_CASE("is_simple")
    {
        SUBCASE("libblas")
        {
            auto ms = MatchSpec::parse("libblas").value();
            CHECK(ms.is_simple());
        }

        SUBCASE("libblas=12.9=abcdef")
        {
            auto ms = MatchSpec::parse("libblas=12.9=abcdef").value();
            CHECK_FALSE(ms.is_simple());
        }

        SUBCASE("libblas=0.15*")
        {
            auto ms = MatchSpec::parse("libblas=0.15*").value();
            CHECK_FALSE(ms.is_simple());
        }

        SUBCASE("libblas[version=12.2]")
        {
            auto ms = MatchSpec::parse("libblas[version=12.2]").value();
            CHECK_FALSE(ms.is_simple());
        }

        SUBCASE("xtensor =0.15*")
        {
            auto ms = MatchSpec::parse("xtensor =0.15*").value();
            CHECK_FALSE(ms.is_simple());
        }
    }
}
