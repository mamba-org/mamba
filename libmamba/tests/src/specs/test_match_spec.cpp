// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <doctest/doctest.h>

#include "mamba/fs/filesystem.hpp"
#include "mamba/specs/match_spec.hpp"
#include "mamba/util/build.hpp"
#include "mamba/util/string.hpp"

using namespace mamba;
using namespace mamba::specs;

TEST_SUITE("specs::match_spec")
{
    using PlatformSet = typename util::flat_set<std::string>;

    TEST_CASE("parse")
    {
        {
            auto ms = MatchSpec::parse("xtensor==0.12.3");
            CHECK_EQ(ms.version().str(), "==0.12.3");
            CHECK_EQ(ms.name().str(), "xtensor");
        }
        {
            auto ms = MatchSpec::parse("");
            CHECK_EQ(ms.version().str(), "=*");
            CHECK_EQ(ms.name().str(), "*");
        }
        {
            auto ms = MatchSpec::parse("ipykernel");
            CHECK_EQ(ms.version().str(), "=*");
            CHECK_EQ(ms.name().str(), "ipykernel");
        }
        {
            auto ms = MatchSpec::parse("ipykernel ");
            CHECK_EQ(ms.version().str(), "=*");
            CHECK_EQ(ms.name().str(), "ipykernel");
        }
        {
            auto ms = MatchSpec::parse("numpy 1.7*");
            CHECK_EQ(ms.version().str(), "=1.7");
            CHECK_EQ(ms.name().str(), "numpy");
            CHECK_EQ(ms.conda_build_form(), "numpy 1.7.*");
            CHECK_EQ(ms.str(), "numpy=1.7");
        }
        {
            auto ms = MatchSpec::parse("numpy[version='1.7|1.8']");
            CHECK_EQ(ms.name().str(), "numpy");
            CHECK_EQ(ms.version().str(), "==1.7|==1.8");
            CHECK_EQ(ms.str(), "numpy[version='==1.7|==1.8']");
        }
        {
            auto ms = MatchSpec::parse("conda-forge/linux-64::xtensor==0.12.3");
            CHECK_EQ(ms.version().str(), "==0.12.3");
            CHECK_EQ(ms.name().str(), "xtensor");
            REQUIRE(ms.channel().has_value());
            CHECK_EQ(ms.channel()->location(), "conda-forge");
            CHECK_EQ(ms.channel()->platform_filters(), PlatformSet{ "linux-64" });
            CHECK_EQ(ms.optional(), false);
        }
        {
            auto ms = MatchSpec::parse("conda-forge::foo[build=3](target=blarg,optional)");
            CHECK_EQ(ms.version().str(), "=*");
            CHECK_EQ(ms.name().str(), "foo");
            REQUIRE(ms.channel().has_value());
            CHECK_EQ(ms.channel()->location(), "conda-forge");
            CHECK_EQ(ms.build_string().str(), "3");
            CHECK_EQ(ms.optional(), true);
        }
        {
            auto ms = MatchSpec::parse("python[build_number=3]");
            CHECK_EQ(ms.name().str(), "python");
            CHECK_EQ(ms.build_number().str(), "=3");
        }
        {
            auto ms = MatchSpec::parse("python[build_number='<=3']");
            CHECK_EQ(ms.name().str(), "python");
            CHECK_EQ(ms.build_number().str(), "<=3");
        }
        {
            auto ms = MatchSpec::parse(
                "https://conda.anaconda.org/conda-forge/linux-64/ncurses-6.4-h59595ed_2.conda"
                "#7dbaa197d7ba6032caf7ae7f32c1efa0"
            );
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
        {
            auto ms = MatchSpec::parse(
                "https://conda.anaconda.org/conda-forge/linux-64/_libgcc_mutex-0.1-conda_forge.tar.bz2"
            );
            CHECK_EQ(ms.name().str(), "_libgcc_mutex");
            CHECK_EQ(ms.version().str(), "==0.1");
            CHECK_EQ(ms.build_string().str(), "conda_forge");
            CHECK_EQ(
                ms.channel().value().str(),
                "https://conda.anaconda.org/conda-forge/linux-64/_libgcc_mutex-0.1-conda_forge.tar.bz2"
            );
            CHECK_EQ(ms.filename(), "_libgcc_mutex-0.1-conda_forge.tar.bz2");
        }
        {
            auto ms = MatchSpec::parse(
                "https://conda.anaconda.org/conda-forge/linux-64/libgcc-ng-11.2.0-h1d223b6_13.tar.bz2"
            );
            CHECK_EQ(ms.name().str(), "libgcc-ng");
            CHECK_EQ(ms.version().str(), "==11.2.0");
            CHECK_EQ(ms.build_string().str(), "h1d223b6_13");
            CHECK_EQ(
                ms.channel().value().str(),
                "https://conda.anaconda.org/conda-forge/linux-64/libgcc-ng-11.2.0-h1d223b6_13.tar.bz2"
            );
            CHECK_EQ(ms.filename(), "libgcc-ng-11.2.0-h1d223b6_13.tar.bz2");
        }
        {
            auto ms = MatchSpec::parse(
                "/home/randomguy/Downloads/linux-64/_libgcc_mutex-0.1-conda_forge.tar.bz2"
            );
            CHECK_EQ(ms.name().str(), "_libgcc_mutex");
            CHECK_EQ(ms.version().str(), "==0.1");
            CHECK_EQ(ms.build_string().str(), "conda_forge");
            CHECK_EQ(
                ms.channel().value().str(),
                "/home/randomguy/Downloads/linux-64/_libgcc_mutex-0.1-conda_forge.tar.bz2"
            );
            CHECK_EQ(ms.filename(), "_libgcc_mutex-0.1-conda_forge.tar.bz2");
        }
        {
            auto ms = MatchSpec::parse(
                "xtensor[url=file:///home/wolfv/Downloads/xtensor-0.21.4-hc9558a2_0.tar.bz2]"
            );
            CHECK_EQ(ms.name().str(), "xtensor");
            CHECK_EQ(
                ms.channel().value().str(),
                "file:///home/wolfv/Downloads/xtensor-0.21.4-hc9558a2_0.tar.bz2"
            );
        }
        {
            auto ms = MatchSpec::parse("foo=1.0=2");
            CHECK_EQ(ms.conda_build_form(), "foo 1.0.* 2");
            CHECK_EQ(ms.str(), "foo=1.0=2");
        }
        {
            auto ms = MatchSpec::parse("foo=1.0=2[md5=123123123, license=BSD-3, fn='test 123.tar.bz2']"
            );
            CHECK_EQ(ms.conda_build_form(), "foo 1.0.* 2");
            CHECK_EQ(ms.str(), R"ms(foo=1.0=2[fn="test 123.tar.bz2",md5=123123123,license=BSD-3])ms");
        }
        {
            auto ms = MatchSpec::parse(
                "foo=1.0=2[md5=123123123, license=BSD-3, fn='test 123.tar.bz2', url='abcdef']"
            );
            CHECK_EQ(ms.channel().value().str(), "abcdef");
            CHECK_EQ(ms.conda_build_form(), "foo 1.0.* 2");
            CHECK_EQ(
                ms.str(),
                R"ms(abcdef::foo=1.0=2[fn="test 123.tar.bz2",md5=123123123,license=BSD-3])ms"
            );
        }
        {
            auto ms = MatchSpec::parse("libblas=*=*mkl");
            CHECK_EQ(ms.conda_build_form(), "libblas * *mkl");
            // CHECK_EQ(ms.str(), "foo==1.0=2");
        }
        {
            auto ms = MatchSpec::parse("libblas=0.15*");
            CHECK_EQ(ms.conda_build_form(), "libblas 0.15*.*");
        }
        {
            // '*' is part of the version, not the glob
            auto ms = MatchSpec::parse("xtensor =0.15*");
            CHECK_EQ(ms.conda_build_form(), "xtensor 0.15*.*");
            CHECK_EQ(ms.str(), "xtensor=0.15*");
            CHECK_EQ(ms.version().str(), "=0.15*");
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
