// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <doctest/doctest.h>

#include "mamba/specs/match_spec.hpp"
#include "mamba/specs/package_info.hpp"
#include "mamba/util/build.hpp"
#include "mamba/util/string.hpp"

using namespace mamba;
using namespace mamba::specs;

TEST_SUITE("specs::match_spec")
{
    using PlatformSet = typename util::flat_set<std::string>;

    TEST_CASE("parse")
    {
        SUBCASE("<empty>")
        {
            auto ms = MatchSpec::parse("").value();
            CHECK(ms.name().is_free());
            CHECK(ms.version().is_explicitly_free());
            CHECK(ms.build_string().is_explicitly_free());
            CHECK(ms.build_number().is_explicitly_free());
            CHECK_EQ(ms.str(), "*");
        }

        SUBCASE("xtensor==0.12.3")
        {
            auto ms = MatchSpec::parse("xtensor==0.12.3").value();
            CHECK_EQ(ms.name().str(), "xtensor");
            CHECK_EQ(ms.version().str(), "==0.12.3");
            CHECK_EQ(ms.str(), "xtensor==0.12.3");
        }

        SUBCASE("xtensor      >=       0.12.3")
        {
            auto ms = MatchSpec::parse("xtensor      >=       0.12.3").value();
            CHECK_EQ(ms.name().str(), "xtensor");
            CHECK_EQ(ms.version().str(), ">=0.12.3");
            CHECK(ms.build_string().is_explicitly_free());
            CHECK(ms.build_number().is_explicitly_free());
            CHECK_EQ(ms.str(), "xtensor>=0.12.3");
        }

        SUBCASE("python > 3.11")
        {
            auto ms = MatchSpec::parse("python > 3.11").value();
            CHECK_EQ(ms.name().str(), "python");
            CHECK_EQ(ms.version().str(), ">3.11");
            CHECK(ms.build_string().is_explicitly_free());
            CHECK(ms.build_number().is_explicitly_free());
            CHECK_EQ(ms.str(), "python>3.11");
        }

        SUBCASE("numpy < 2.0")
        {
            auto ms = MatchSpec::parse("numpy < 2.0").value();
            CHECK_EQ(ms.name().str(), "numpy");
            CHECK_EQ(ms.version().str(), "<2.0");
            CHECK(ms.build_string().is_explicitly_free());
            CHECK(ms.build_number().is_explicitly_free());
            CHECK_EQ(ms.str(), "numpy<2.0");
        }

        SUBCASE("pytorch-cpu = 1.13.0")
        {
            auto ms = MatchSpec::parse("pytorch-cpu = 1.13.0").value();
            CHECK_EQ(ms.name().str(), "pytorch-cpu");
            CHECK_EQ(ms.version().str(), "=1.13.0");
            CHECK(ms.build_string().is_explicitly_free());
            CHECK(ms.build_number().is_explicitly_free());
            CHECK_EQ(ms.str(), "pytorch-cpu=1.13.0");
        }

        SUBCASE("scipy   >=    1.5.0,  < 2.0.0")
        {
            auto ms = MatchSpec::parse("scipy   >=    1.5.0,  < 2.0.0").value();
            CHECK_EQ(ms.name().str(), "scipy");
            CHECK_EQ(ms.version().str(), ">=1.5.0,<2.0.0");
            CHECK(ms.build_string().is_explicitly_free());
            CHECK(ms.build_number().is_explicitly_free());
            CHECK_EQ(ms.str(), "scipy[version=\">=1.5.0,<2.0.0\"]");
        }

        SUBCASE("scikit-learn >1.0.0")
        {
            auto ms = MatchSpec::parse("scikit-learn >1.0.0").value();
            CHECK_EQ(ms.name().str(), "scikit-learn");
            CHECK_EQ(ms.version().str(), ">1.0.0");
            CHECK(ms.build_string().is_explicitly_free());
            CHECK(ms.build_number().is_explicitly_free());
            CHECK_EQ(ms.str(), "scikit-learn>1.0.0");
        }

        SUBCASE("kytea >=0.1.4, 0.2.0")
        {
            auto ms = MatchSpec::parse("kytea >=0.1.4, 0.2.0").value();
            CHECK_EQ(ms.name().str(), "kytea");
            CHECK_EQ(ms.version().str(), ">=0.1.4,==0.2.0");
            CHECK(ms.build_string().is_explicitly_free());
            CHECK(ms.build_number().is_explicitly_free());
            CHECK_EQ(ms.str(), "kytea[version=\">=0.1.4,==0.2.0\"]");
        }

        // Invalid case from `inform2w64-sysroot_win-64-v12.0.0.r2.ggc561118da-h707e725_0.conda`
        // which is currently supported but which must not.
        SUBCASE("mingw-w64-ucrt-x86_64-crt-git v12.0.0.r2.ggc561118da h707e725_0")
        {
            auto ms = MatchSpec::parse("mingw-w64-ucrt-x86_64-crt-git v12.0.0.r2.ggc561118da h707e725_0")
                          .value();
            CHECK_EQ(ms.name().str(), "mingw-w64-ucrt-x86_64-crt-git");
            CHECK_EQ(ms.version().str(), "==0v12.0.0.0r2.0ggc561118da");
            CHECK_EQ(ms.build_string().str(), "h707e725_0");
            CHECK(ms.build_number().is_explicitly_free());
            CHECK_EQ(ms.str(), "mingw-w64-ucrt-x86_64-crt-git==0v12.0.0.0r2.0ggc561118da=h707e725_0");
        }

        SUBCASE("_libgcc_mutex 0.1 conda_forge")
        {
            auto ms = MatchSpec::parse("_libgcc_mutex 0.1 conda_forge").value();
            CHECK_EQ(ms.name().str(), "_libgcc_mutex");
            CHECK_EQ(ms.version().str(), "==0.1");
            CHECK_EQ(ms.build_string().str(), "conda_forge");
            CHECK(ms.build_number().is_explicitly_free());
            CHECK_EQ(ms.str(), "_libgcc_mutex==0.1=conda_forge");
        }

        SUBCASE("_libgcc_mutex    0.1       conda_forge     ")
        {
            auto ms = MatchSpec::parse("_libgcc_mutex    0.1       conda_forge     ").value();
            CHECK_EQ(ms.name().str(), "_libgcc_mutex");
            CHECK_EQ(ms.version().str(), "==0.1");
            CHECK_EQ(ms.build_string().str(), "conda_forge");
            CHECK(ms.build_number().is_explicitly_free());
            CHECK_EQ(ms.str(), "_libgcc_mutex==0.1=conda_forge");
        }

        SUBCASE("ipykernel")
        {
            auto ms = MatchSpec::parse("ipykernel").value();
            CHECK_EQ(ms.name().str(), "ipykernel");
            CHECK(ms.version().is_explicitly_free());
            CHECK_EQ(ms.str(), "ipykernel");
        }

        SUBCASE("ipykernel ")
        {
            auto ms = MatchSpec::parse("ipykernel ").value();
            CHECK_EQ(ms.name().str(), "ipykernel");
            CHECK(ms.version().is_explicitly_free());
        }

        SUBCASE("disperse=v0.9.24")
        {
            auto ms = MatchSpec::parse("disperse=v0.9.24").value();
            CHECK_EQ(ms.name().str(), "disperse");
            CHECK_EQ(ms.version().str(), "=0v0.9.24");
            CHECK(ms.build_string().is_explicitly_free());
            CHECK(ms.build_number().is_explicitly_free());
            CHECK_EQ(ms.str(), "disperse=0v0.9.24");
        }

        SUBCASE("disperse v0.9.24")
        {
            auto ms = MatchSpec::parse("disperse v0.9.24").value();
            CHECK_EQ(ms.name().str(), "disperse");
            CHECK_EQ(ms.version().str(), "==0v0.9.24");
            CHECK(ms.build_string().is_explicitly_free());
            CHECK(ms.build_number().is_explicitly_free());
            CHECK_EQ(ms.str(), "disperse==0v0.9.24");
        }

        SUBCASE("foo V0.9.24")
        {
            auto ms = MatchSpec::parse("foo V0.9.24");
            CHECK_FALSE(ms.has_value());
            CHECK_EQ(std::string(ms.error().what()), "Found invalid version predicate in \"V0.9.24\"");
        }

        SUBCASE("importlib-metadata  # drop this when dropping Python 3.8")
        {
            auto ms = MatchSpec::parse("importlib-metadata  # drop this when dropping Python 3.8")
                          .value();
            CHECK_EQ(ms.name().str(), "importlib-metadata");
            CHECK(ms.version().is_explicitly_free());
            CHECK_EQ(ms.str(), "importlib-metadata");
        }

        SUBCASE("foo=V0.9.24")
        {
            auto ms = MatchSpec::parse("foo=V0.9.24").value();
            CHECK_EQ(ms.name().str(), "foo");
            CHECK_EQ(ms.version().str(), "=0v0.9.24");
            CHECK(ms.build_string().is_explicitly_free());
            CHECK(ms.build_number().is_explicitly_free());
            CHECK_EQ(ms.str(), "foo=0v0.9.24");
        }

        SUBCASE("numpy 1.7*")
        {
            auto ms = MatchSpec::parse("numpy 1.7*").value();
            CHECK_EQ(ms.name().str(), "numpy");
            CHECK_EQ(ms.version().str(), "=1.7");
            CHECK_EQ(ms.conda_build_form(), "numpy 1.7.*");
            CHECK_EQ(ms.str(), "numpy=1.7");
        }

        SUBCASE("conda-forge:pypi:xtensor==0.12.3")
        {
            auto ms = MatchSpec::parse("conda-forge:pypi:xtensor==0.12.3").value();
            CHECK_EQ(ms.name().str(), "xtensor");
            CHECK_EQ(ms.version().str(), "==0.12.3");
            CHECK_EQ(ms.channel().value().str(), "conda-forge");
            CHECK_EQ(ms.name_space(), "pypi");
            CHECK_EQ(ms.str(), "conda-forge:pypi:xtensor==0.12.3");
        }

        SUBCASE("conda-forge/linux-64::xtensor==0.12.3")
        {
            auto ms = MatchSpec::parse("numpy[version='1.7|1.8']").value();
            CHECK_EQ(ms.name().str(), "numpy");
            CHECK_EQ(ms.version().str(), "==1.7|==1.8");
            CHECK_EQ(ms.str(), R"(numpy[version="==1.7|==1.8"])");
        }

        SUBCASE("conda-forge/linux-64::xtensor==0.12.3")
        {
            auto ms = MatchSpec::parse("conda-forge/linux-64::xtensor==0.12.3").value();
            CHECK_EQ(ms.name().str(), "xtensor");
            CHECK_EQ(ms.version().str(), "==0.12.3");
            REQUIRE(ms.channel().has_value());
            CHECK_EQ(ms.channel()->location(), "conda-forge");
            CHECK_EQ(ms.platforms().value().get(), PlatformSet{ "linux-64" });
            CHECK_EQ(ms.str(), "conda-forge[linux-64]::xtensor==0.12.3");
        }

        SUBCASE("conda-forge::foo[build=bld](target=blarg,optional)")
        {
            auto ms = MatchSpec::parse("conda-forge::foo[build=bld](target=blarg,optional)").value();
            CHECK_EQ(ms.name().str(), "foo");
            CHECK(ms.version().is_explicitly_free());
            REQUIRE(ms.channel().has_value());
            CHECK_EQ(ms.channel()->location(), "conda-forge");
            CHECK_EQ(ms.build_string().str(), "bld");
            CHECK_EQ(ms.optional(), true);
            CHECK_EQ(ms.str(), "conda-forge::foo=*=bld[optional]");
        }

        SUBCASE("python[build_number=3]")
        {
            auto ms = MatchSpec::parse("python[build_number=3]").value();
            CHECK_EQ(ms.name().str(), "python");
            CHECK_EQ(ms.version().str(), "=*");
            CHECK_EQ(ms.build_number().str(), "=3");
            CHECK_EQ(ms.str(), R"(python[build_number="=3"])");
        }

        SUBCASE(R"(blas[track_features="mkl avx"])")
        {
            auto ms = MatchSpec::parse(R"(blas[track_features="mkl avx"])").value();
            CHECK_EQ(ms.name().str(), "blas");
            CHECK_EQ(ms.track_features().value().get(), MatchSpec::string_set{ "avx", "mkl" });
            CHECK_EQ(ms.str(), R"(blas[track_features="avx mkl"])");
        }

        SUBCASE("python[build_number='<=3']")
        {
            auto ms = MatchSpec::parse("python[build_number='<=3']").value();
            CHECK_EQ(ms.name().str(), "python");
            CHECK_EQ(ms.build_number().str(), "<=3");
            CHECK_EQ(ms.str(), R"(python[build_number="<=3"])");
        }

        SUBCASE("https://conda.anaconda.org/conda-forge/linux-64/ncurses-6.4-h59595ed_2.conda#7dbaa197d7ba6032caf7ae7f32c1efa0"
        )
        {
            constexpr auto str = std::string_view{
                "https://conda.anaconda.org/conda-forge/linux-64/ncurses-6.4-h59595ed_2.conda"
                "#7dbaa197d7ba6032caf7ae7f32c1efa0"
            };

            auto ms = MatchSpec::parse(str).value();
            CHECK_EQ(ms.name().str(), "ncurses");
            CHECK_EQ(ms.version().str(), "==6.4");
            CHECK_EQ(ms.build_string().str(), "h59595ed_2");
            CHECK_EQ(
                ms.channel().value().str(),
                "https://conda.anaconda.org/conda-forge/linux-64/ncurses-6.4-h59595ed_2.conda"
            );
            CHECK_EQ(ms.filename(), "ncurses-6.4-h59595ed_2.conda");
            CHECK_EQ(ms.md5(), "7dbaa197d7ba6032caf7ae7f32c1efa0");
            CHECK_EQ(ms.str(), str);
        }

        SUBCASE("https://conda.anaconda.org/conda-forge/linux-64/_libgcc_mutex-0.1-conda_forge.tar.bz2")
        {
            constexpr auto str = std::string_view{
                "https://conda.anaconda.org/conda-forge/linux-64/_libgcc_mutex-0.1-conda_forge.tar.bz2"
            };
            auto ms = MatchSpec::parse(str).value();
            CHECK_EQ(ms.name().str(), "_libgcc_mutex");
            CHECK_EQ(ms.version().str(), "==0.1");
            CHECK_EQ(ms.build_string().str(), "conda_forge");
            CHECK_EQ(
                ms.channel().value().str(),
                "https://conda.anaconda.org/conda-forge/linux-64/_libgcc_mutex-0.1-conda_forge.tar.bz2"
            );
            CHECK_EQ(ms.filename(), "_libgcc_mutex-0.1-conda_forge.tar.bz2");
            CHECK_EQ(ms.str(), str);
        }

        SUBCASE("https://conda.anaconda.org/conda-forge/linux-64/libgcc-ng-11.2.0-h1d223b6_13.tar.bz2")
        {
            constexpr auto str = std::string_view{
                "https://conda.anaconda.org/conda-forge/linux-64/libgcc-ng-11.2.0-h1d223b6_13.tar.bz2"
            };
            auto ms = MatchSpec::parse(str).value();
            CHECK_EQ(ms.name().str(), "libgcc-ng");
            CHECK_EQ(ms.version().str(), "==11.2.0");
            CHECK_EQ(ms.build_string().str(), "h1d223b6_13");
            CHECK_EQ(
                ms.channel().value().str(),
                "https://conda.anaconda.org/conda-forge/linux-64/libgcc-ng-11.2.0-h1d223b6_13.tar.bz2"
            );
            CHECK_EQ(ms.filename(), "libgcc-ng-11.2.0-h1d223b6_13.tar.bz2");
            CHECK_EQ(ms.str(), str);
        }

        SUBCASE("https://conda.anaconda.org/conda-canary/linux-64/conda-4.3.21.post699+1dab973-py36h4a561cd_0.tar.bz2"
        )
        {
            constexpr auto str = std::string_view{
                "https://conda.anaconda.org/conda-canary/linux-64/conda-4.3.21.post699+1dab973-py36h4a561cd_0.tar.bz2"
            };
            auto ms = MatchSpec::parse(str).value();
            CHECK_EQ(ms.name().str(), "conda");
            CHECK_EQ(ms.version().str(), "==4.3.21.0post699+1dab973");  // Note the ``.0post``
            CHECK_EQ(ms.build_string().str(), "py36h4a561cd_0");
            CHECK_EQ(ms.str(), str);
        }

        SUBCASE("/home/randomguy/Downloads/linux-64/_libgcc_mutex-0.1-conda_forge.tar.bz2")
        {
            constexpr auto str = std::string_view{
                "/home/randomguy/Downloads/linux-64/_libgcc_mutex-0.1-conda_forge.tar.bz2"
            };
            auto ms = MatchSpec::parse(str).value();
            CHECK_EQ(ms.name().str(), "_libgcc_mutex");
            CHECK_EQ(ms.version().str(), "==0.1");
            CHECK_EQ(ms.build_string().str(), "conda_forge");
            CHECK_EQ(
                ms.channel().value().str(),
                "/home/randomguy/Downloads/linux-64/_libgcc_mutex-0.1-conda_forge.tar.bz2"
            );
            CHECK_EQ(ms.filename(), "_libgcc_mutex-0.1-conda_forge.tar.bz2");
            CHECK_EQ(ms.str(), str);
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
            CHECK_EQ(ms.str(), "file:///home/wolfv/Downloads/xtensor-0.21.4-hc9558a2_0.tar.bz2");
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

        SUBCASE("foo   =    1.0    =    2")
        {
            auto ms = MatchSpec::parse("foo   =    1.0    =    2").value();
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

        SUBCASE(R"(defaults::numpy=1.8=py27_0 [name="pytorch",channel='anaconda',version=">=1.8,<2|1.9", build='3'])"
        )
        {
            auto ms = MatchSpec::parse(
                          R"(defaults::numpy=1.8=py27_0 [name="pytorch",channel='anaconda',version=">=1.8,<2|1.9", build='3'])"
            )
                          .value();
            CHECK_EQ(ms.channel().value().str(), "anaconda");
            CHECK_EQ(ms.name().str(), "numpy");
            CHECK_EQ(ms.version().str(), "=1.8");
            CHECK_EQ(ms.build_string().str(), "py27_0");
            CHECK_EQ(ms.str(), R"(anaconda::numpy=1.8=py27_0)");
        }

        SUBCASE(R"(defaults::numpy [ name="pytorch",channel='anaconda',version=">=1.8,<2|1.9", build='3'])"
        )
        {
            auto ms = MatchSpec::parse(
                          R"(defaults::numpy [ name="pytorch",channel='anaconda',version=">=1.8,<2|1.9", build='3'])"
            )
                          .value();
            CHECK_EQ(ms.channel().value().str(), "anaconda");
            CHECK_EQ(ms.name().str(), "numpy");
            CHECK_EQ(ms.version().str(), ">=1.8,(<2|==1.9)");
            CHECK_EQ(ms.build_string().str(), "3");
            CHECK_EQ(ms.str(), R"ms(anaconda::numpy[version=">=1.8,(<2|==1.9)",build="3"])ms");
        }

        SUBCASE("numpy >1.8,<2|==1.7,!=1.9,~=1.7.1 py34_0")
        {
            auto ms = MatchSpec::parse(R"(numpy >1.8,<2|==1.7,!=1.9,~=1.7.1 py34_0)").value();
            CHECK_EQ(ms.name().str(), "numpy");
            CHECK_EQ(ms.version().str(), ">1.8,((<2|==1.7),(!=1.9,(>=1.7.1,=1.7)))");
            CHECK_EQ(ms.build_string().str(), "py34_0");
            CHECK_EQ(
                ms.str(),
                R"ms(numpy[version=">1.8,((<2|==1.7),(!=1.9,(>=1.7.1,=1.7)))",build="py34_0"])ms"
            );
        }

        SUBCASE("python-graphviz~=0.20")
        {
            auto ms = MatchSpec::parse("python-graphviz~=0.20").value();
            CHECK_EQ(ms.name().str(), "python-graphviz");
            CHECK_EQ(ms.version().str(), ">=0.20,=0");
            CHECK_EQ(ms.str(), R"ms(python-graphviz[version=">=0.20,=0"])ms");
        }

        SUBCASE("python-graphviz  ~=      0.20")
        {
            auto ms = MatchSpec::parse("python-graphviz  ~=      0.20").value();
            CHECK_EQ(ms.name().str(), "python-graphviz");
            CHECK_EQ(ms.version().str(), ">=0.20,=0");
            CHECK_EQ(ms.str(), R"ms(python-graphviz[version=">=0.20,=0"])ms");
        }

        SUBCASE("*[md5=fewjaflknd]")
        {
            auto ms = MatchSpec::parse("*[md5=fewjaflknd]").value();
            CHECK(ms.name().is_free());
            CHECK_EQ(ms.md5(), "fewjaflknd");
            CHECK_EQ(ms.str(), "*[md5=fewjaflknd]");
        }

        SUBCASE("libblas=*=*mkl")
        {
            auto ms = MatchSpec::parse("libblas=*=*mkl").value();
            CHECK_EQ(ms.name().str(), "libblas");
            CHECK(ms.version().is_explicitly_free());
            CHECK_EQ(ms.build_string().str(), "*mkl");
            CHECK_EQ(ms.str(), R"(libblas[build="*mkl"])");
            CHECK_EQ(ms.conda_build_form(), "libblas * *mkl");
        }

        SUBCASE("libblas=0.15*")
        {
            // '*' is part of the version, not the glob
            auto ms = MatchSpec::parse("libblas=0.15*").value();
            CHECK_EQ(ms.name().str(), "libblas");
            CHECK_EQ(ms.version().str(), "=0.15*");
            CHECK(ms.build_string().is_explicitly_free());
            CHECK_EQ(ms.str(), "libblas=0.15*");
            CHECK_EQ(ms.conda_build_form(), "libblas 0.15*.*");
        }

        SUBCASE("xtensor =0.15*")
        {
            // '*' is part of the version, not the glob
            auto ms = MatchSpec::parse("xtensor =0.15*").value();
            CHECK_EQ(ms.name().str(), "xtensor");
            CHECK_EQ(ms.version().str(), "=0.15*");
            CHECK(ms.build_string().is_explicitly_free());
            CHECK_EQ(ms.str(), "xtensor=0.15*");
            CHECK_EQ(ms.conda_build_form(), "xtensor 0.15*.*");
        }

        SUBCASE("numpy=1.20")
        {
            auto ms = MatchSpec::parse("numpy=1.20").value();
            CHECK_EQ(ms.name().str(), "numpy");
            CHECK_EQ(ms.version().str(), "=1.20");
            CHECK(ms.build_string().is_explicitly_free());
            CHECK_EQ(ms.str(), "numpy=1.20");
        }

        SUBCASE("conda-forge::tzdata")
        {
            auto ms = MatchSpec::parse("conda-forge::tzdata").value();
            CHECK_EQ(ms.channel().value().str(), "conda-forge");
            CHECK_EQ(ms.name().str(), "tzdata");
            CHECK(ms.version().is_explicitly_free());
            CHECK(ms.build_string().is_explicitly_free());
            CHECK_EQ(ms.str(), "conda-forge::tzdata");
        }

        SUBCASE("conda-forge/noarch::tzdata")
        {
            auto ms = MatchSpec::parse("conda-forge/noarch::tzdata").value();
            CHECK_EQ(ms.channel().value().str(), "conda-forge[noarch]");
            CHECK_EQ(ms.name().str(), "tzdata");
            CHECK(ms.version().is_explicitly_free());
            CHECK(ms.build_string().is_explicitly_free());
            CHECK_EQ(ms.str(), "conda-forge[noarch]::tzdata");
        }

        SUBCASE("conda-forge[noarch]::tzdata")
        {
            auto ms = MatchSpec::parse("conda-forge/noarch::tzdata").value();
            CHECK_EQ(ms.channel().value().str(), "conda-forge[noarch]");
            CHECK_EQ(ms.name().str(), "tzdata");
            CHECK(ms.version().is_explicitly_free());
            CHECK(ms.build_string().is_explicitly_free());
            CHECK_EQ(ms.str(), "conda-forge[noarch]::tzdata");
        }

        SUBCASE("pkgs/main::tzdata")
        {
            auto ms = MatchSpec::parse("pkgs/main::tzdata").value();
            CHECK_EQ(ms.channel().value().str(), "pkgs/main");
            CHECK_EQ(ms.name().str(), "tzdata");
            CHECK(ms.version().is_explicitly_free());
            CHECK(ms.build_string().is_explicitly_free());
            CHECK_EQ(ms.str(), "pkgs/main::tzdata");
        }

        SUBCASE("pkgs/main/noarch::tzdata")
        {
            auto ms = MatchSpec::parse("pkgs/main/noarch::tzdata").value();
            CHECK_EQ(ms.channel().value().str(), "pkgs/main[noarch]");
            CHECK_EQ(ms.name().str(), "tzdata");
            CHECK(ms.version().is_explicitly_free());
            CHECK(ms.build_string().is_explicitly_free());
            CHECK_EQ(ms.str(), "pkgs/main[noarch]::tzdata");
        }

        SUBCASE("conda-forge[noarch]::tzdata[subdir=linux64]")
        {
            auto ms = MatchSpec::parse("conda-forge[noarch]::tzdata[subdir=linux64]").value();
            CHECK_EQ(ms.channel().value().str(), "conda-forge[noarch]");
            CHECK_EQ(ms.platforms().value().get(), MatchSpec::platform_set{ "noarch" });
            CHECK_EQ(ms.name().str(), "tzdata");
            CHECK(ms.version().is_explicitly_free());
            CHECK(ms.build_string().is_explicitly_free());
            CHECK_EQ(ms.str(), "conda-forge[noarch]::tzdata");
        }

        SUBCASE("conda-forge::tzdata[subdir=mamba-37]")
        {
            auto ms = MatchSpec::parse("conda-forge::tzdata[subdir=mamba-37]").value();
            CHECK_EQ(ms.channel().value().str(), "conda-forge[mamba-37]");
            CHECK_EQ(ms.platforms().value().get(), MatchSpec::platform_set{ "mamba-37" });
            CHECK_EQ(ms.name().str(), "tzdata");
            CHECK(ms.version().is_explicitly_free());
            CHECK(ms.build_string().is_explicitly_free());
            CHECK_EQ(ms.str(), "conda-forge[mamba-37]::tzdata");
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
            CHECK_EQ(ms.str(), "conda-canary[linux-64]::conda==4.3.21.0post699+1dab973=py36h4a561cd_0");
        }

        SUBCASE("libblas[build=^.*(accelerate|mkl)$]")
        {
            auto ms = MatchSpec::parse("libblas[build=^.*(accelerate|mkl)$]").value();
            CHECK_EQ(ms.name().str(), "libblas");
            CHECK_EQ(ms.build_string().str(), "^.*(accelerate|mkl)$");
            CHECK_FALSE(ms.build_string().is_glob());
        }
    }

    TEST_CASE("parse_url")
    {
        SUBCASE("https://conda.com/pkg-2-bld.conda")
        {
            auto ms = MatchSpec::parse_url("https://conda.com/pkg-2-bld.conda").value();
            CHECK(ms.is_file());
            CHECK_EQ(ms.name().str(), "pkg");
            CHECK_EQ(ms.version().str(), "==2");
            CHECK_EQ(ms.str(), "https://conda.com/pkg-2-bld.conda");
            CHECK_EQ(ms.build_string().str(), "bld");
            CHECK_EQ(ms.filename(), "pkg-2-bld.conda");
        }

        SUBCASE("/home/usr/mamba/micromamba/tests/data/cph_test_data-0.0.1-0.tar.bz2")
        {
            auto ms = MatchSpec::parse_url(
                          "/home/usr/mamba/micromamba/tests/data/cph_test_data-0.0.1-0.tar.bz2"
            )
                          .value();
            CHECK(ms.is_file());
            CHECK_EQ(ms.name().str(), "cph_test_data");
            CHECK_EQ(ms.version().str(), "==0.0.1");
            CHECK_EQ(ms.str(), "/home/usr/mamba/micromamba/tests/data/cph_test_data-0.0.1-0.tar.bz2");
            CHECK_EQ(ms.build_string().str(), "0");
            CHECK_EQ(ms.filename(), "cph_test_data-0.0.1-0.tar.bz2");
        }

        SUBCASE(R"(D:\a\mamba\mamba\micromamba\tests\data\cph_test_data-0.0.1-0.tar.bz2)")
        {
            if (util::on_win)
            {
                auto ms = MatchSpec::parse_url(
                              R"(D:\a\mamba\mamba\micromamba\tests\data\cph_test_data-0.0.1-0.tar.bz2)"
                )
                              .value();
                CHECK(ms.is_file());
                CHECK_EQ(ms.name().str(), "cph_test_data");
                CHECK_EQ(ms.version().str(), "==0.0.1");
                CHECK_EQ(
                    ms.str(),
                    "D:/a/mamba/mamba/micromamba/tests/data/cph_test_data-0.0.1-0.tar.bz2"
                );
                CHECK_EQ(ms.build_string().str(), "0");
                CHECK_EQ(ms.filename(), "cph_test_data-0.0.1-0.tar.bz2");
            }
        }
    }

    TEST_CASE("Conda discrepancies")
    {
        SUBCASE("python=3.7=bld")
        {
            // For some reason, conda parses version differently in `python=3.7` and
            // `python=3.7=bld`.
            // It is `=3.7` and `==3.7` in the later.
            auto ms = MatchSpec::parse("python=3.7=bld").value();
            CHECK_EQ(ms.version().str(), "=3.7");
            CHECK_EQ(ms.build_string().str(), "bld");
        }

        SUBCASE("python[version>3]")
        {
            // Supported by conda but we consider to be already served by `version=">3"`
            auto error = MatchSpec::parse("python[version>3]").error();
            CHECK(util::contains(error.what(), R"(use "version='>3'" instead)"));
        }

        SUBCASE("python[version=3.7]")
        {
            // Ambiguous, `version=` parsed as attribute assignment, which leads to
            // `3.7` (similar to `==3.7`) being parsed as VersionSpec
            auto ms = MatchSpec::parse("python[version=3.7]").value();
            CHECK_EQ(ms.version().str(), "==3.7");
        }
    }

    TEST_CASE("is_simple")
    {
        SUBCASE("Positive")
        {
            for (std::string_view str : {
                     "libblas",
                     "libblas=12.9=abcdef",
                     "libblas=0.15*",
                     "libblas[version=12.2]",
                     "xtensor =0.15*",
                 })
            {
                CAPTURE(str);
                const auto ms = MatchSpec::parse(str).value();
                CHECK(ms.is_simple());
            }
        }

        SUBCASE("Negative")
        {
            for (std::string_view str : {
                     "pkg[build_number=3]",
                     "pkg[md5=85094328554u9543215123]",
                     "pkg[sha256=0320104934325453]",
                     "pkg[license=MIT]",
                     "pkg[track_features=mkl]",
                     "pkg[version='(>2,<3)|=4']",
                     "conda-forge::pkg",
                     "pypi:pkg",
                 })
            {
                CAPTURE(str);
                const auto ms = MatchSpec::parse(str).value();
                CHECK_FALSE(ms.is_simple());
            }
        }
    }

    TEST_CASE("MatchSpec::contains")
    {
        // Note that tests for individual ``contains`` functions (``VersionSpec::contains``,
        // ``BuildNumber::contains``, ``GlobSpec::contains``...) are tested in their respective
        // test files.

        using namespace specs::match_spec_literals;
        using namespace specs::version_literals;

        struct Pkg
        {
            std::string name = {};
            specs::Version version = {};
            std::string build_string = {};
            std::size_t build_number = {};
            std::string md5 = {};
            std::string sha256 = {};
            std::string license = {};
            DynamicPlatform platform = {};
            MatchSpec::string_set track_features = {};
        };

        SUBCASE("python")
        {
            const auto ms = "python"_ms;
            CHECK(ms.contains_except_channel(Pkg{ "python" }));
            CHECK_FALSE(ms.contains_except_channel(Pkg{ "pypy" }));

            CHECK(ms.contains_except_channel(PackageInfo{ "python" }));
            CHECK_FALSE(ms.contains_except_channel(PackageInfo{ "pypy" }));
        }

        SUBCASE("py*")
        {
            const auto ms = "py*"_ms;
            CHECK(ms.contains_except_channel(Pkg{ "python" }));
            CHECK(ms.contains_except_channel(Pkg{ "pypy" }));
            CHECK_FALSE(ms.contains_except_channel(Pkg{ "rust" }));

            CHECK(ms.contains_except_channel(PackageInfo{ "python" }));
            CHECK(ms.contains_except_channel(PackageInfo{ "pypy" }));
            CHECK_FALSE(ms.contains_except_channel(PackageInfo{ "rust" }));
        }

        SUBCASE("py*>=3.7")
        {
            const auto ms = "py*>=3.7"_ms;
            CHECK(ms.contains_except_channel(Pkg{ "python", "3.7"_v }));
            CHECK_FALSE(ms.contains_except_channel(Pkg{ "pypy", "3.6"_v }));
            CHECK_FALSE(ms.contains_except_channel(Pkg{ "rust", "3.7"_v }));

            CHECK(ms.contains_except_channel(PackageInfo{ "python", "3.7", "bld", 0 }));
            CHECK_FALSE(ms.contains_except_channel(PackageInfo{ "pypy", "3.6", "bld", 0 }));
            CHECK_FALSE(ms.contains_except_channel(PackageInfo{ "rust", "3.7", "bld", 0 }));
        }

        SUBCASE("py*>=3.7=*cpython")
        {
            const auto ms = "py*>=3.7=*cpython"_ms;
            CHECK(ms.contains_except_channel(Pkg{ "python", "3.7"_v, "37_cpython" }));
            CHECK_FALSE(ms.contains_except_channel(Pkg{ "pypy", "3.6"_v, "cpython" }));
            CHECK_FALSE(ms.contains_except_channel(Pkg{ "pypy", "3.8"_v, "pypy" }));
            CHECK_FALSE(ms.contains_except_channel(Pkg{ "rust", "3.7"_v, "cpyhton" }));
        }

        SUBCASE("py*[version='>=3.7', build=*cpython]")
        {
            const auto ms = "py*[version='>=3.7', build=*cpython]"_ms;
            CHECK(ms.contains_except_channel(Pkg{ "python", "3.7"_v, "37_cpython" }));
            CHECK_FALSE(ms.contains_except_channel(Pkg{ "pypy", "3.6"_v, "cpython" }));
            CHECK_FALSE(ms.contains_except_channel(Pkg{ "pypy", "3.8"_v, "pypy" }));
            CHECK_FALSE(ms.contains_except_channel(Pkg{ "rust", "3.7"_v, "cpyhton" }));
        }

        SUBCASE("pkg[build_number='>3']")
        {
            const auto ms = "pkg[build_number='>3']"_ms;
            auto pkg = Pkg{ "pkg" };
            pkg.build_number = 4;
            CHECK(ms.contains_except_channel(pkg));
            pkg.build_number = 2;
            CHECK_FALSE(ms.contains_except_channel(pkg));
        }

        SUBCASE("pkg[md5=helloiamnotreallymd5haha]")
        {
            const auto ms = "pkg[md5=helloiamnotreallymd5haha]"_ms;

            auto pkg = Pkg{ "pkg" };
            pkg.md5 = "helloiamnotreallymd5haha";
            CHECK(ms.contains_except_channel(pkg));

            for (auto md5 : { "helloiamnotreallymd5hahaevillaugh", "hello", "" })
            {
                CAPTURE(std::string_view(md5));
                pkg.md5 = md5;
                CHECK_FALSE(ms.contains_except_channel(pkg));
            }
        }

        SUBCASE("pkg[sha256=helloiamnotreallysha256hihi]")
        {
            const auto ms = "pkg[sha256=helloiamnotreallysha256hihi]"_ms;

            auto pkg = Pkg{ "pkg" };
            pkg.sha256 = "helloiamnotreallysha256hihi";
            CHECK(ms.contains_except_channel(pkg));

            for (auto sha256 : { "helloiamnotreallysha256hihicutelaugh", "hello", "" })
            {
                CAPTURE(std::string_view(sha256));
                pkg.sha256 = sha256;
                CHECK_FALSE(ms.contains_except_channel(pkg));
            }
        }

        SUBCASE("pkg[license=helloiamnotreallylicensehoho]")
        {
            const auto ms = "pkg[license=helloiamnotreallylicensehoho]"_ms;

            auto pkg = Pkg{ "pkg" };
            pkg.license = "helloiamnotreallylicensehoho";
            CHECK(ms.contains_except_channel(pkg));

            for (auto license : { "helloiamnotreallylicensehohodadlaugh", "hello", "" })
            {
                CAPTURE(std::string_view(license));
                pkg.license = license;
                CHECK_FALSE(ms.contains_except_channel(pkg));
            }
        }

        SUBCASE("pkg[subdir='linux-64,linux-64-512']")
        {
            const auto ms = "pkg[subdir='linux-64,linux-64-512']"_ms;

            auto pkg = Pkg{ "pkg" };

            for (auto plat : { "linux-64", "linux-64-512" })
            {
                CAPTURE(std::string_view(plat));
                pkg.platform = plat;
                CHECK(ms.contains_except_channel(pkg));
            }

            for (auto plat : { "linux", "linux-512", "", "linux-64,linux-64-512" })
            {
                CAPTURE(std::string_view(plat));
                pkg.platform = plat;
                CHECK_FALSE(ms.contains_except_channel(pkg));
            }
        }

        SUBCASE("pkg[track_features='mkl,openssl']")
        {
            using string_set = typename MatchSpec::string_set;

            const auto ms = "pkg[track_features='mkl,openssl']"_ms;

            auto pkg = Pkg{ "pkg" };

            for (auto tfeats : { string_set{ "openssl", "mkl" } })
            {
                pkg.track_features = tfeats;
                CHECK(ms.contains_except_channel(pkg));
            }

            for (auto tfeats : { string_set{ "openssl" }, string_set{ "mkl" }, string_set{} })
            {
                pkg.track_features = tfeats;
                CHECK_FALSE(ms.contains_except_channel(pkg));
            }
        }

        SUBCASE("Complex")
        {
            const auto ms = "py*>=3.7=bld[build_number='<=2', md5=lemd5, track_features='mkl,openssl']"_ms;

            CHECK(ms.contains_except_channel(Pkg{
                /* .name= */ "python",
                /* .version= */ "3.8.0"_v,
                /* .build_string= */ "bld",
                /* .build_number= */ 2,
                /* .md5= */ "lemd5",
                /* .sha256= */ "somesha256",
                /* .license= */ "MIT",
                /* .platform= */ "linux-64",
                /* .track_features =*/{ "openssl", "mkl" },
            }));
            CHECK(ms.contains_except_channel(Pkg{
                /* .name= */ "python",
                /* .version= */ "3.12.0"_v,
                /* .build_string= */ "bld",
                /* .build_number= */ 0,
                /* .md5= */ "lemd5",
                /* .sha256= */ "somesha256",
                /* .license= */ "GPL",
                /* .platform= */ "linux-64",
                /* .track_features =*/{ "openssl", "mkl" },
            }));
            CHECK_FALSE(ms.contains_except_channel(Pkg{
                /* .name= */ "python",
                /* .version= */ "3.3.0"_v,  // Not matching
                /* .build_string= */ "bld",
                /* .build_number= */ 0,
                /* .md5= */ "lemd5",
                /* .sha256= */ "somesha256",
                /* .license= */ "GPL",
                /* .platform= */ "linux-64",
                /* .track_features =*/{ "openssl", "mkl" },
            }));
            CHECK_FALSE(ms.contains_except_channel(Pkg{
                /* .name= */ "python",
                /* .version= */ "3.12.0"_v,
                /* .build_string= */ "bld",
                /* .build_number= */ 0,
                /* .md5= */ "wrong",  // Not matching
                /* .sha256= */ "somesha256",
                /* .license= */ "GPL",
                /* .platform= */ "linux-64",
                /* .track_features =*/{ "openssl", "mkl" },
            }));
        }
    }

    TEST_CASE("MatchSpec comparability and hashability")
    {
        using namespace specs::match_spec_literals;
        using namespace specs::version_literals;

        const auto spec1 = "py*>=3.7=bld[build_number='<=2', md5=lemd5, track_features='mkl,openssl']"_ms;

        // Create an identical specification
        const auto spec2 = "py*>=3.7=bld[build_number='<=2', md5=lemd5, track_features='mkl,openssl']"_ms;

        // Create a different specification
        const auto spec3 = "py*>=3.7=bld[build_number='<=2', md5=lemd5, track_features='mkl']"_ms;

        // Check that the two copies are equal
        CHECK_EQ(spec1, spec2);
        // Check that the different specification is not equal to the first one
        CHECK_NE(spec1, spec3);

        // Check that the hash of the two copies is the same
        auto spec1_hash = std::hash<MatchSpec>{}(spec1);
        auto spec2_hash = std::hash<MatchSpec>{}(spec2);
        auto spec3_hash = std::hash<MatchSpec>{}(spec3);

        CHECK_EQ(spec1_hash, spec2_hash);
        CHECK_NE(spec1_hash, spec3_hash);
    }
}
