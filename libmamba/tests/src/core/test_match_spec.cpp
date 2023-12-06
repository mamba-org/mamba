// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <doctest/doctest.h>

#include "mamba/core/channel_context.hpp"
#include "mamba/core/match_spec.hpp"

#include "mambatests.hpp"

using namespace mamba;

TEST_SUITE("MatchSpec")
{
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
        auto& ctx = mambatests::context();
        auto channel_context = ChannelContext::make_conda_compatible(ctx);
        {
            MatchSpec ms("xtensor==0.12.3", ctx, channel_context);
            CHECK_EQ(ms.version, "0.12.3");
            CHECK_EQ(ms.name, "xtensor");
        }
        {
            MatchSpec ms("", ctx, channel_context);
            CHECK_EQ(ms.version, "");
            CHECK_EQ(ms.name, "");
        }
        {
            MatchSpec ms("ipykernel", ctx, channel_context);
            CHECK_EQ(ms.version, "");
            CHECK_EQ(ms.name, "ipykernel");
        }
        {
            MatchSpec ms("ipykernel ", ctx, channel_context);
            CHECK_EQ(ms.version, "");
            CHECK_EQ(ms.name, "ipykernel");
        }
        {
            MatchSpec ms("numpy 1.7*", ctx, channel_context);
            CHECK_EQ(ms.version, "1.7*");
            CHECK_EQ(ms.name, "numpy");
            CHECK_EQ(ms.conda_build_form(), "numpy 1.7*");
            CHECK_EQ(ms.str(), "numpy=1.7");
        }
        {
            MatchSpec ms("numpy[version='1.7|1.8']", ctx, channel_context);
            // TODO!
            // CHECK_EQ(ms.version, "1.7|1.8");
            CHECK_EQ(ms.name, "numpy");
            CHECK_EQ(ms.brackets["version"], "1.7|1.8");
            CHECK_EQ(ms.str(), "numpy[version='1.7|1.8']");
        }
        {
            MatchSpec ms("conda-forge/linux64::xtensor==0.12.3", ctx, channel_context);
            CHECK_EQ(ms.version, "0.12.3");
            CHECK_EQ(ms.name, "xtensor");
            CHECK_EQ(ms.channel, "conda-forge/linux64");
            CHECK_EQ(ms.optional, false);
        }
        {
            MatchSpec ms("conda-forge::foo[build=3](target=blarg,optional)", ctx, channel_context);
            CHECK_EQ(ms.version, "");
            CHECK_EQ(ms.name, "foo");
            CHECK_EQ(ms.channel, "conda-forge");
            CHECK_EQ(ms.brackets["build"], "3");
            CHECK_EQ(ms.parens["target"], "blarg");
            CHECK_EQ(ms.optional, true);
        }
        {
            MatchSpec ms("python[build_number=3]", ctx, channel_context);
            CHECK_EQ(ms.name, "python");
            CHECK_EQ(ms.brackets["build_number"], "3");
            CHECK_EQ(ms.build_number, "3");
        }
        {
            MatchSpec ms("python[build_number='<=3']", ctx, channel_context);
            CHECK_EQ(ms.name, "python");
            CHECK_EQ(ms.brackets["build_number"], "<=3");
            CHECK_EQ(ms.build_number, "<=3");
        }
        {
            MatchSpec ms(
                "https://conda.anaconda.org/conda-forge/linux-64/_libgcc_mutex-0.1-conda_forge.tar.bz2",
                ctx,
                channel_context
            );
            CHECK_EQ(ms.name, "_libgcc_mutex");
            CHECK_EQ(ms.version, "0.1");
            CHECK_EQ(ms.build_string, "conda_forge");
            CHECK_EQ(
                ms.url,
                "https://conda.anaconda.org/conda-forge/linux-64/_libgcc_mutex-0.1-conda_forge.tar.bz2"
            );
            CHECK_EQ(ms.fn, "_libgcc_mutex-0.1-conda_forge.tar.bz2");
        }
        {
            MatchSpec ms(
                "/home/randomguy/Downloads/linux-64/_libgcc_mutex-0.1-conda_forge.tar.bz2",
                ctx,
                channel_context
            );
            CHECK_EQ(ms.name, "_libgcc_mutex");
            CHECK_EQ(ms.version, "0.1");
            CHECK_EQ(ms.build_string, "conda_forge");
#ifdef _WIN32
            std::string driveletter = fs::absolute(fs::u8path("/")).string().substr(0, 1);
            CHECK_EQ(
                ms.url,
                std::string("file://") + driveletter
                    + ":/home/randomguy/Downloads/linux-64/_libgcc_mutex-0.1-conda_forge.tar.bz2"
            );
#else
            CHECK_EQ(
                ms.url,
                "file:///home/randomguy/Downloads/linux-64/_libgcc_mutex-0.1-conda_forge.tar.bz2"
            );
#endif
            CHECK_EQ(ms.fn, "_libgcc_mutex-0.1-conda_forge.tar.bz2");
        }
        {
            MatchSpec ms(
                "xtensor[url=file:///home/wolfv/Downloads/"
                "xtensor-0.21.4-hc9558a2_0.tar.bz2]",
                ctx,
                channel_context
            );
            CHECK_EQ(ms.name, "xtensor");
            CHECK_EQ(
                ms.brackets["url"],
                "file:///home/wolfv/Downloads/xtensor-0.21.4-hc9558a2_0.tar.bz2"
            );
            CHECK_EQ(ms.url, "file:///home/wolfv/Downloads/xtensor-0.21.4-hc9558a2_0.tar.bz2");
        }
        {
            MatchSpec ms("foo=1.0=2", ctx, channel_context);
            CHECK_EQ(ms.conda_build_form(), "foo 1.0 2");
            CHECK_EQ(ms.str(), "foo==1.0=2");
        }
        {
            MatchSpec ms(
                "foo=1.0=2[md5=123123123, license=BSD-3, fn='test 123.tar.bz2']",
                ctx,
                channel_context
            );
            CHECK_EQ(ms.conda_build_form(), "foo 1.0 2");
            CHECK_EQ(ms.str(), "foo==1.0=2[md5=123123123,license=BSD-3,fn='test 123.tar.bz2']");
        }
        {
            MatchSpec ms(
                "foo=1.0=2[md5=123123123, license=BSD-3, fn='test 123.tar.bz2', url='abcdef']",
                ctx,
                channel_context
            );
            CHECK_EQ(ms.conda_build_form(), "foo 1.0 2");
            CHECK_EQ(ms.str(), "foo==1.0=2[url=abcdef,md5=123123123,license=BSD-3]");
        }
        {
            MatchSpec ms("libblas=*=*mkl", ctx, channel_context);
            CHECK_EQ(ms.conda_build_form(), "libblas * *mkl");
            // CHECK_EQ(ms.str(), "foo==1.0=2");
        }
        {
            MatchSpec ms("libblas=0.15*", ctx, channel_context);
            CHECK_EQ(ms.conda_build_form(), "libblas 0.15*");
        }
        {
            MatchSpec ms("xtensor =0.15*", ctx, channel_context);
            CHECK_EQ(ms.conda_build_form(), "xtensor 0.15*");
            CHECK_EQ(ms.str(), "xtensor=0.15");
        }
        {
            MatchSpec ms("numpy=1.20", ctx, channel_context);
            CHECK_EQ(ms.str(), "numpy=1.20");
        }

        {
            MatchSpec ms("conda-forge::tzdata", ctx, channel_context);
            CHECK_EQ(ms.str(), "conda-forge::tzdata");
        }
        {
            MatchSpec ms("conda-forge::noarch/tzdata", ctx, channel_context);
            CHECK_EQ(ms.str(), "conda-forge::noarch/tzdata");
        }
        {
            MatchSpec ms("pkgs/main::tzdata", ctx, channel_context);
            CHECK_EQ(ms.str(), "pkgs/main::tzdata");
        }
        {
            MatchSpec ms("pkgs/main/noarch::tzdata", ctx, channel_context);
            CHECK_EQ(ms.str(), "pkgs/main/noarch::tzdata");
        }
        {
            MatchSpec ms("conda-forge/noarch::tzdata[subdir=linux64]", ctx, channel_context);
            CHECK_EQ(ms.str(), "conda-forge/noarch::tzdata");
        }
        {
            MatchSpec ms("conda-forge::tzdata[subdir=linux64]", ctx, channel_context);
            CHECK_EQ(ms.str(), "conda-forge/linux64::tzdata");
        }
    }

    TEST_CASE("is_simple")
    {
        auto& ctx = mambatests::context();
        auto channel_context = ChannelContext::make_conda_compatible(ctx);
        {
            MatchSpec ms("libblas", ctx, channel_context);
            CHECK(ms.is_simple());
        }
        {
            MatchSpec ms("libblas=12.9=abcdef", ctx, channel_context);
            CHECK_FALSE(ms.is_simple());
        }
        {
            MatchSpec ms("libblas=0.15*", ctx, channel_context);
            CHECK_FALSE(ms.is_simple());
        }
        {
            MatchSpec ms("libblas[version=12.2]", ctx, channel_context);
            CHECK_FALSE(ms.is_simple());
        }
        {
            MatchSpec ms("xtensor =0.15*", ctx, channel_context);
            CHECK_FALSE(ms.is_simple());
        }
    }
}
