// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <fstream>

#include <catch2/catch_all.hpp>
#include <nlohmann/json.hpp>

#include "mamba/specs/match_spec.hpp"
#include "mamba/specs/package_info.hpp"
#include "mamba/util/build.hpp"
#include "mamba/util/environment.hpp"
#include "mamba/util/string.hpp"

using namespace mamba;
using namespace mamba::specs;

namespace
{
    using PlatformSet = typename util::flat_set<std::string>;

    TEST_CASE("MatchSpec parse", "[mamba::specs][mamba::specs::MatchSpec]")
    {
        SECTION("<empty>")
        {
            auto ms = MatchSpec::parse("").value();
            REQUIRE(ms.name().is_free());
            REQUIRE(ms.version().is_explicitly_free());
            REQUIRE(ms.build_string().is_explicitly_free());
            REQUIRE(ms.build_number().is_explicitly_free());
            REQUIRE(ms.str() == "*");
            REQUIRE_FALSE(ms.is_only_package_name());
        }

        SECTION("xtensor==0.12.3")
        {
            auto ms = MatchSpec::parse("xtensor==0.12.3").value();
            REQUIRE(ms.name().str() == "xtensor");
            REQUIRE(ms.version().str() == "==0.12.3");
            REQUIRE(ms.str() == "xtensor==0.12.3");
            REQUIRE_FALSE(ms.is_only_package_name());
        }

        SECTION("xtensor      >=       0.12.3")
        {
            auto ms = MatchSpec::parse("xtensor      >=       0.12.3").value();
            REQUIRE(ms.name().str() == "xtensor");
            REQUIRE(ms.version().str() == ">=0.12.3");
            REQUIRE(ms.build_string().is_explicitly_free());
            REQUIRE(ms.build_number().is_explicitly_free());
            REQUIRE(ms.str() == "xtensor>=0.12.3");
            REQUIRE_FALSE(ms.is_only_package_name());
        }

        SECTION("python > 3.11")
        {
            auto ms = MatchSpec::parse("python > 3.11").value();
            REQUIRE(ms.name().str() == "python");
            REQUIRE(ms.version().str() == ">3.11");
            REQUIRE(ms.build_string().is_explicitly_free());
            REQUIRE(ms.build_number().is_explicitly_free());
            REQUIRE(ms.str() == "python>3.11");
            REQUIRE_FALSE(ms.is_only_package_name());
        }

        SECTION("numpy < 2.0")
        {
            auto ms = MatchSpec::parse("numpy < 2.0").value();
            REQUIRE(ms.name().str() == "numpy");
            REQUIRE(ms.version().str() == "<2.0");
            REQUIRE(ms.build_string().is_explicitly_free());
            REQUIRE(ms.build_number().is_explicitly_free());
            REQUIRE(ms.str() == "numpy<2.0");
            REQUIRE_FALSE(ms.is_only_package_name());
        }

        SECTION("pytorch-cpu = 1.13.0")
        {
            auto ms = MatchSpec::parse("pytorch-cpu = 1.13.0").value();
            REQUIRE(ms.name().str() == "pytorch-cpu");
            REQUIRE(ms.version().str() == "=1.13.0");
            REQUIRE(ms.build_string().is_explicitly_free());
            REQUIRE(ms.build_number().is_explicitly_free());
            REQUIRE(ms.str() == "pytorch-cpu=1.13.0");
            REQUIRE_FALSE(ms.is_only_package_name());
        }

        SECTION("scipy   >=    1.5.0,  < 2.0.0")
        {
            auto ms = MatchSpec::parse("scipy   >=    1.5.0,  < 2.0.0").value();
            REQUIRE(ms.name().str() == "scipy");
            REQUIRE(ms.version().str() == ">=1.5.0,<2.0.0");
            REQUIRE(ms.build_string().is_explicitly_free());
            REQUIRE(ms.build_number().is_explicitly_free());
            REQUIRE(ms.str() == "scipy[version=\">=1.5.0,<2.0.0\"]");
            REQUIRE_FALSE(ms.is_only_package_name());
        }

        SECTION("scikit-learn >1.0.0")
        {
            auto ms = MatchSpec::parse("scikit-learn >1.0.0").value();
            REQUIRE(ms.name().str() == "scikit-learn");
            REQUIRE(ms.version().str() == ">1.0.0");
            REQUIRE(ms.build_string().is_explicitly_free());
            REQUIRE(ms.build_number().is_explicitly_free());
            REQUIRE(ms.str() == "scikit-learn>1.0.0");
        }

        SECTION("kytea >=0.1.4, 0.2.0")
        {
            auto ms = MatchSpec::parse("kytea >=0.1.4, 0.2.0").value();
            REQUIRE(ms.name().str() == "kytea");
            REQUIRE(ms.version().str() == ">=0.1.4,==0.2.0");
            REQUIRE(ms.build_string().is_explicitly_free());
            REQUIRE(ms.build_number().is_explicitly_free());
            REQUIRE(ms.str() == "kytea[version=\">=0.1.4,==0.2.0\"]");
        }

        SECTION("abc>12")
        {
            auto ms = MatchSpec::parse("abc>12").value();
            REQUIRE(ms.name().str() == "abc");
            REQUIRE(ms.version().str() == ">12");
            REQUIRE(ms.build_string().is_explicitly_free());
            REQUIRE(ms.build_number().is_explicitly_free());
            REQUIRE(ms.str() == "abc>12");
        }

        SECTION("abc[version='>3']")
        {
            auto ms = MatchSpec::parse("abc[version='>3']").value();
            REQUIRE(ms.name().str() == "abc");
            REQUIRE(ms.version().str() == ">3");
            REQUIRE(ms.build_string().is_explicitly_free());
            REQUIRE(ms.build_number().is_explicitly_free());
            REQUIRE(ms.str() == "abc>3");
        }

        SECTION("numpy~=1.26.0")
        {
            auto ms = MatchSpec::parse("numpy~=1.26.0").value();
            REQUIRE(ms.name().str() == "numpy");
            REQUIRE(ms.version().str() == "~=1.26.0");
            REQUIRE(ms.build_string().is_explicitly_free());
            REQUIRE(ms.build_number().is_explicitly_free());
            REQUIRE(ms.str() == "numpy~=1.26.0");

            // TODO: test this assumption for many more cases
            auto ms2 = MatchSpec::parse(ms.str()).value();
            REQUIRE(ms2 == ms);
        }

        SECTION("mingw-w64-ucrt-x86_64-crt-git v12.0.0.r2.ggc561118da h707e725_0")
        {
            // Invalid case from `inform2w64-sysroot_win-64-v12.0.0.r2.ggc561118da-h707e725_0.conda`
            // which is currently supported but which must not.
            auto ms = MatchSpec::parse("mingw-w64-ucrt-x86_64-crt-git v12.0.0.r2.ggc561118da h707e725_0")
                          .value();
            REQUIRE(ms.name().str() == "mingw-w64-ucrt-x86_64-crt-git");
            REQUIRE(ms.version().str() == "==v12.0.0.r2.ggc561118da");
            REQUIRE(ms.build_string().str() == "h707e725_0");
            REQUIRE(ms.build_number().is_explicitly_free());
            REQUIRE(ms.str() == "mingw-w64-ucrt-x86_64-crt-git==v12.0.0.r2.ggc561118da=h707e725_0");
        }

        SECTION("openblas 0.2.18|0.2.18.*.")
        {
            // Invalid case from `inform2w64-sysroot_win-64-v12.0.0.r2.ggc561118da-h707e725_0.conda`
            // which is currently supported but which must not.
            auto ms = MatchSpec::parse("openblas 0.2.18|0.2.18.*.").value();
            REQUIRE(ms.name().str() == "openblas");
            REQUIRE(ms.version().str() == "==0.2.18|=0.2.18");
        }

        SECTION("_libgcc_mutex 0.1 conda_forge")
        {
            auto ms = MatchSpec::parse("_libgcc_mutex 0.1 conda_forge").value();
            REQUIRE(ms.name().str() == "_libgcc_mutex");
            REQUIRE(ms.version().str() == "==0.1");
            REQUIRE(ms.build_string().str() == "conda_forge");
            REQUIRE(ms.build_number().is_explicitly_free());
            REQUIRE(ms.str() == "_libgcc_mutex==0.1=conda_forge");
        }

        SECTION("_libgcc_mutex    0.1       conda_forge     ")
        {
            auto ms = MatchSpec::parse("_libgcc_mutex    0.1       conda_forge     ").value();
            REQUIRE(ms.name().str() == "_libgcc_mutex");
            REQUIRE(ms.version().str() == "==0.1");
            REQUIRE(ms.build_string().str() == "conda_forge");
            REQUIRE(ms.build_number().is_explicitly_free());
            REQUIRE(ms.str() == "_libgcc_mutex==0.1=conda_forge");
        }

        SECTION("ipykernel")
        {
            auto ms = MatchSpec::parse("ipykernel").value();
            REQUIRE(ms.name().str() == "ipykernel");
            REQUIRE(ms.version().is_explicitly_free());
            REQUIRE(ms.str() == "ipykernel");
            REQUIRE(ms.is_only_package_name());
        }

        SECTION("ipykernel ")
        {
            auto ms = MatchSpec::parse("ipykernel ").value();
            REQUIRE(ms.name().str() == "ipykernel");
            REQUIRE(ms.version().is_explicitly_free());
            REQUIRE(ms.is_only_package_name());
        }

        SECTION("disperse=v0.9.24")
        {
            auto ms = MatchSpec::parse("disperse=v0.9.24").value();
            REQUIRE(ms.name().str() == "disperse");
            REQUIRE(ms.version().str() == "=v0.9.24");
            REQUIRE(ms.build_string().is_explicitly_free());
            REQUIRE(ms.build_number().is_explicitly_free());
            REQUIRE(ms.str() == "disperse=v0.9.24");
        }

        SECTION("disperse v0.9.24")
        {
            auto ms = MatchSpec::parse("disperse v0.9.24").value();
            REQUIRE(ms.name().str() == "disperse");
            REQUIRE(ms.version().str() == "==v0.9.24");
            REQUIRE(ms.build_string().is_explicitly_free());
            REQUIRE(ms.build_number().is_explicitly_free());
            REQUIRE(ms.str() == "disperse==v0.9.24");
        }

        SECTION("foo V0.9.24")
        {
            auto ms = MatchSpec::parse("foo V0.9.24");
            REQUIRE_FALSE(ms.has_value());
            REQUIRE(std::string(ms.error().what()) == "Found invalid version predicate in \"V0.9.24\"");
        }

        SECTION("importlib-metadata  # drop this when dropping Python 3.8")
        {
            auto ms = MatchSpec::parse("importlib-metadata  # drop this when dropping Python 3.8")
                          .value();
            REQUIRE(ms.name().str() == "importlib-metadata");
            REQUIRE(ms.version().is_explicitly_free());
            REQUIRE(ms.str() == "importlib-metadata");
        }

        SECTION("foo=V0.9.24")
        {
            auto ms = MatchSpec::parse("foo=V0.9.24").value();
            REQUIRE(ms.name().str() == "foo");
            REQUIRE(ms.version().str() == "=v0.9.24");
            REQUIRE(ms.build_string().is_explicitly_free());
            REQUIRE(ms.build_number().is_explicitly_free());
            REQUIRE(ms.str() == "foo=v0.9.24");
        }

        SECTION("numpy 1.7*")
        {
            auto ms = MatchSpec::parse("numpy 1.7*").value();
            REQUIRE(ms.name().str() == "numpy");
            REQUIRE(ms.version().str() == "=1.7");
            REQUIRE(ms.conda_build_form() == "numpy 1.7.*");
            REQUIRE(ms.str() == "numpy=1.7");
        }

        SECTION("conda-forge:pypi:xtensor==0.12.3")
        {
            auto ms = MatchSpec::parse("conda-forge:pypi:xtensor==0.12.3").value();
            REQUIRE(ms.name().str() == "xtensor");
            REQUIRE(ms.version().str() == "==0.12.3");
            REQUIRE(ms.channel().value().str() == "conda-forge");
            REQUIRE(ms.name_space() == "pypi");
            REQUIRE(ms.str() == "conda-forge:pypi:xtensor==0.12.3");
        }

        SECTION("conda-forge/linux-64::xtensor==0.12.3")
        {
            auto ms = MatchSpec::parse("numpy[version='1.7|1.8']").value();
            REQUIRE(ms.name().str() == "numpy");
            REQUIRE(ms.version().str() == "==1.7|==1.8");
            REQUIRE(ms.str() == R"(numpy[version="==1.7|==1.8"])");
        }

        SECTION("conda-forge/linux-64::xtensor==0.12.3")
        {
            auto ms = MatchSpec::parse("conda-forge/linux-64::xtensor==0.12.3").value();
            REQUIRE(ms.name().str() == "xtensor");
            REQUIRE(ms.version().str() == "==0.12.3");
            REQUIRE(ms.channel().has_value());
            REQUIRE(ms.channel()->location() == "conda-forge");
            REQUIRE(ms.platforms().value().get() == PlatformSet{ "linux-64" });
            REQUIRE(ms.str() == "conda-forge[linux-64]::xtensor==0.12.3");
        }

        SECTION("conda-forge::foo[build=bld](target=blarg,optional)")
        {
            auto ms = MatchSpec::parse("conda-forge::foo[build=bld](target=blarg,optional)").value();
            REQUIRE(ms.name().str() == "foo");
            REQUIRE(ms.version().is_explicitly_free());
            REQUIRE(ms.channel().has_value());
            REQUIRE(ms.channel()->location() == "conda-forge");
            REQUIRE(ms.build_string().str() == "bld");
            REQUIRE(ms.optional() == true);
            REQUIRE(ms.str() == "conda-forge::foo=*=bld[optional]");
        }

        SECTION("python[build_number=3]")
        {
            auto ms = MatchSpec::parse("python[build_number=3]").value();
            REQUIRE(ms.name().str() == "python");
            REQUIRE(ms.version().str() == "=*");
            REQUIRE(ms.build_number().str() == "=3");
            REQUIRE(ms.str() == R"(python[build_number="=3"])");
        }

        SECTION(R"(blas[track_features="mkl avx"])")
        {
            auto ms = MatchSpec::parse(R"(blas[track_features="mkl avx"])").value();
            REQUIRE(ms.name().str() == "blas");
            REQUIRE(ms.track_features().value().get() == MatchSpec::string_set{ "avx", "mkl" });
            REQUIRE(ms.str() == R"(blas[track_features="avx mkl"])");
        }

        SECTION("python[build_number='<=3']")
        {
            auto ms = MatchSpec::parse("python[build_number='<=3']").value();
            REQUIRE(ms.name().str() == "python");
            REQUIRE(ms.build_number().str() == "<=3");
            REQUIRE(ms.str() == R"(python[build_number="<=3"])");
        }

        SECTION("https://conda.anaconda.org/conda-forge/linux-64/ncurses-6.4-h59595ed_2.conda#7dbaa197d7ba6032caf7ae7f32c1efa0"
        )
        {
            constexpr auto str = std::string_view{
                "https://conda.anaconda.org/conda-forge/linux-64/ncurses-6.4-h59595ed_2.conda"
                "#7dbaa197d7ba6032caf7ae7f32c1efa0"
            };

            auto ms = MatchSpec::parse(str).value();
            REQUIRE(ms.name().str() == "ncurses");
            REQUIRE(ms.version().str() == "==6.4");
            REQUIRE(ms.build_string().str() == "h59595ed_2");
            REQUIRE(
                ms.channel().value().str()
                == "https://conda.anaconda.org/conda-forge/linux-64/ncurses-6.4-h59595ed_2.conda"
            );
            REQUIRE(ms.filename() == "ncurses-6.4-h59595ed_2.conda");
            REQUIRE(ms.md5() == "7dbaa197d7ba6032caf7ae7f32c1efa0");
            REQUIRE(ms.str() == str);
        }

        SECTION("https://conda.anaconda.org/conda-forge/linux-64/_libgcc_mutex-0.1-conda_forge.tar.bz2")
        {
            constexpr auto str = std::string_view{
                "https://conda.anaconda.org/conda-forge/linux-64/_libgcc_mutex-0.1-conda_forge.tar.bz2"
            };
            auto ms = MatchSpec::parse(str).value();
            REQUIRE(ms.name().str() == "_libgcc_mutex");
            REQUIRE(ms.version().str() == "==0.1");
            REQUIRE(ms.build_string().str() == "conda_forge");
            REQUIRE(
                ms.channel().value().str()
                == "https://conda.anaconda.org/conda-forge/linux-64/_libgcc_mutex-0.1-conda_forge.tar.bz2"
            );
            REQUIRE(ms.filename() == "_libgcc_mutex-0.1-conda_forge.tar.bz2");
            REQUIRE(ms.str() == str);
        }

        SECTION("https://conda.anaconda.org/conda-forge/linux-64/libgcc-ng-11.2.0-h1d223b6_13.tar.bz2")
        {
            constexpr auto str = std::string_view{
                "https://conda.anaconda.org/conda-forge/linux-64/libgcc-ng-11.2.0-h1d223b6_13.tar.bz2"
            };
            auto ms = MatchSpec::parse(str).value();
            REQUIRE(ms.name().str() == "libgcc-ng");
            REQUIRE(ms.version().str() == "==11.2.0");
            REQUIRE(ms.build_string().str() == "h1d223b6_13");
            REQUIRE(
                ms.channel().value().str()
                == "https://conda.anaconda.org/conda-forge/linux-64/libgcc-ng-11.2.0-h1d223b6_13.tar.bz2"
            );
            REQUIRE(ms.filename() == "libgcc-ng-11.2.0-h1d223b6_13.tar.bz2");
            REQUIRE(ms.str() == str);
        }

        SECTION("https://conda.anaconda.org/conda-canary/linux-64/conda-4.3.21.post699+1dab973-py36h4a561cd_0.tar.bz2"
        )
        {
            constexpr auto str = std::string_view{
                "https://conda.anaconda.org/conda-canary/linux-64/conda-4.3.21.post699+1dab973-py36h4a561cd_0.tar.bz2"
            };
            auto ms = MatchSpec::parse(str).value();
            REQUIRE(ms.name().str() == "conda");
            REQUIRE(ms.version().str() == "==4.3.21.post699+1dab973");
            REQUIRE(ms.build_string().str() == "py36h4a561cd_0");
            REQUIRE(ms.str() == str);
        }

        SECTION("/home/randomguy/Downloads/linux-64/_libgcc_mutex-0.1-conda_forge.tar.bz2")
        {
            constexpr auto str = std::string_view{
                "/home/randomguy/Downloads/linux-64/_libgcc_mutex-0.1-conda_forge.tar.bz2"
            };
            auto ms = MatchSpec::parse(str).value();
            REQUIRE(ms.name().str() == "_libgcc_mutex");
            REQUIRE(ms.version().str() == "==0.1");
            REQUIRE(ms.build_string().str() == "conda_forge");
            REQUIRE(
                ms.channel().value().str()
                == "/home/randomguy/Downloads/linux-64/_libgcc_mutex-0.1-conda_forge.tar.bz2"
            );
            REQUIRE(ms.filename() == "_libgcc_mutex-0.1-conda_forge.tar.bz2");
            REQUIRE(ms.str() == str);
        }

        SECTION("xtensor[url=file:///home/wolfv/Downloads/xtensor-0.21.4-hc9558a2_0.tar.bz2]")
        {
            auto ms = MatchSpec::parse(
                          "xtensor[url=file:///home/wolfv/Downloads/xtensor-0.21.4-hc9558a2_0.tar.bz2]"
            )
                          .value();
            REQUIRE(ms.name().str() == "xtensor");
            REQUIRE(
                ms.channel().value().str()
                == "file:///home/wolfv/Downloads/xtensor-0.21.4-hc9558a2_0.tar.bz2"
            );
            REQUIRE(ms.str() == "file:///home/wolfv/Downloads/xtensor-0.21.4-hc9558a2_0.tar.bz2");
        }

        SECTION("foo=1.0=2")
        {
            auto ms = MatchSpec::parse("foo=1.0=2").value();
            REQUIRE(ms.conda_build_form() == "foo 1.0.* 2");
            REQUIRE(ms.name().str() == "foo");
            REQUIRE(ms.version().str() == "=1.0");
            REQUIRE(ms.build_string().str() == "2");
            REQUIRE(ms.str() == "foo=1.0=2");
        }

        SECTION("foo   =    1.0    =    2")
        {
            auto ms = MatchSpec::parse("foo   =    1.0    =    2").value();
            REQUIRE(ms.conda_build_form() == "foo 1.0.* 2");
            REQUIRE(ms.name().str() == "foo");
            REQUIRE(ms.version().str() == "=1.0");
            REQUIRE(ms.build_string().str() == "2");
            REQUIRE(ms.str() == "foo=1.0=2");
        }

        SECTION("foo=1.0=2[md5=123123123, license=BSD-3, fn='test 123.tar.bz2']")
        {
            auto ms = MatchSpec::parse("foo=1.0=2[md5=123123123, license=BSD-3, fn='test 123.tar.bz2']")
                          .value();
            REQUIRE(ms.name().str() == "foo");
            REQUIRE(ms.version().str() == "=1.0");
            REQUIRE(ms.build_string().str() == "2");
            REQUIRE(ms.conda_build_form() == "foo 1.0.* 2");
            REQUIRE(ms.str() == R"ms(foo=1.0=2[fn="test 123.tar.bz2",md5=123123123,license=BSD-3])ms");
        }

        SECTION("foo=1.0=2[md5=123123123, license=BSD-3, fn='test 123.tar.bz2', url='abcdef']")
        {
            auto ms = MatchSpec::parse(
                          "foo=1.0=2[md5=123123123, license=BSD-3, fn='test 123.tar.bz2', url='abcdef']"
            )
                          .value();
            REQUIRE(ms.channel().value().str() == "abcdef");
            REQUIRE(ms.name().str() == "foo");
            REQUIRE(ms.version().str() == "=1.0");
            REQUIRE(ms.build_string().str() == "2");
            REQUIRE(ms.conda_build_form() == "foo 1.0.* 2");
            REQUIRE(
                ms.str() == R"ms(abcdef::foo=1.0=2[fn="test 123.tar.bz2",md5=123123123,license=BSD-3])ms"
            );
        }

        SECTION(R"(defaults::numpy=1.8=py27_0 [name="pytorch",channel='anaconda',version=">=1.8,<2|1.9", build='3'])"
        )
        {
            auto ms = MatchSpec::parse(
                          R"(defaults::numpy=1.8=py27_0 [name="pytorch",channel='anaconda',version=">=1.8,<2|1.9", build='3'])"
            )
                          .value();
            REQUIRE(ms.channel().value().str() == "anaconda");
            REQUIRE(ms.name().str() == "numpy");
            REQUIRE(ms.version().str() == "=1.8");
            REQUIRE(ms.build_string().str() == "py27_0");
            REQUIRE(ms.str() == R"(anaconda::numpy=1.8=py27_0)");
        }

        SECTION(R"(defaults::numpy [ name="pytorch",channel='anaconda',version=">=1.8,<2|1.9", build='3'])"
        )
        {
            auto ms = MatchSpec::parse(
                          R"(defaults::numpy [ name="pytorch",channel='anaconda',version=">=1.8,<2|1.9", build='3'])"
            )
                          .value();
            REQUIRE(ms.channel().value().str() == "anaconda");
            REQUIRE(ms.name().str() == "numpy");
            REQUIRE(ms.version().str() == ">=1.8,(<2|==1.9)");
            REQUIRE(ms.build_string().str() == "3");
            REQUIRE(ms.str() == R"ms(anaconda::numpy[version=">=1.8,(<2|==1.9)",build="3"])ms");
        }

        SECTION("numpy >1.8,<2|==1.7,!=1.9,~=1.7.1 py34_0")
        {
            auto ms = MatchSpec::parse(R"(numpy >1.8,<2|==1.7,!=1.9,~=1.7.1 py34_0)").value();
            REQUIRE(ms.name().str() == "numpy");
            REQUIRE(ms.version().str() == ">1.8,((<2|==1.7),(!=1.9,~=1.7.1))");
            REQUIRE(ms.build_string().str() == "py34_0");
            REQUIRE(
                ms.str() == R"ms(numpy[version=">1.8,((<2|==1.7),(!=1.9,~=1.7.1))",build="py34_0"])ms"
            );
        }

        SECTION("python-graphviz~=0.20")
        {
            auto ms = MatchSpec::parse("python-graphviz~=0.20").value();
            REQUIRE(ms.name().str() == "python-graphviz");
            REQUIRE(ms.version().str() == "~=0.20");
            REQUIRE(ms.str() == R"ms(python-graphviz~=0.20)ms");
        }

        SECTION("python-graphviz  ~=      0.20")
        {
            auto ms = MatchSpec::parse("python-graphviz  ~=      0.20").value();
            REQUIRE(ms.name().str() == "python-graphviz");
            REQUIRE(ms.version().str() == "~=0.20");
            REQUIRE(ms.str() == R"ms(python-graphviz~=0.20)ms");
        }

        SECTION("python[version='~=3.11.0',build=*_cpython]")
        {
            auto ms = MatchSpec::parse("python[version='~=3.11.0',build=*_cpython]").value();
            REQUIRE(ms.name().str() == "python");
            REQUIRE(ms.version().str() == "~=3.11.0");
            REQUIRE(ms.build_string().str() == "*_cpython");
            REQUIRE(ms.str() == R"ms(python[version="~=3.11.0",build="*_cpython"])ms");
        }

        SECTION("*[md5=fewjaflknd]")
        {
            auto ms = MatchSpec::parse("*[md5=fewjaflknd]").value();
            REQUIRE(ms.name().is_free());
            REQUIRE(ms.md5() == "fewjaflknd");
            REQUIRE(ms.str() == "*[md5=fewjaflknd]");
        }

        SECTION("libblas=*=*mkl")
        {
            auto ms = MatchSpec::parse("libblas=*=*mkl").value();
            REQUIRE(ms.name().str() == "libblas");
            REQUIRE(ms.version().is_explicitly_free());
            REQUIRE(ms.build_string().str() == "*mkl");
            REQUIRE(ms.str() == R"(libblas[build="*mkl"])");
            REQUIRE(ms.conda_build_form() == "libblas * *mkl");
        }

        SECTION("libblas=0.15*")
        {
            // '*' is part of the version, not the glob
            auto ms = MatchSpec::parse("libblas=0.15*").value();
            REQUIRE(ms.name().str() == "libblas");
            REQUIRE(ms.version().str() == "=0.15*");
            REQUIRE(ms.build_string().is_explicitly_free());
            REQUIRE(ms.str() == "libblas=0.15*");
            REQUIRE(ms.conda_build_form() == "libblas 0.15*.*");
        }

        SECTION("xtensor =0.15*")
        {
            // '*' is part of the version, not the glob
            auto ms = MatchSpec::parse("xtensor =0.15*").value();
            REQUIRE(ms.name().str() == "xtensor");
            REQUIRE(ms.version().str() == "=0.15*");
            REQUIRE(ms.build_string().is_explicitly_free());
            REQUIRE(ms.str() == "xtensor=0.15*");
            REQUIRE(ms.conda_build_form() == "xtensor 0.15*.*");
        }

        SECTION("numpy=1.20")
        {
            auto ms = MatchSpec::parse("numpy=1.20").value();
            REQUIRE(ms.name().str() == "numpy");
            REQUIRE(ms.version().str() == "=1.20");
            REQUIRE(ms.build_string().is_explicitly_free());
            REQUIRE(ms.str() == "numpy=1.20");
        }

        SECTION("conda-forge::tzdata")
        {
            auto ms = MatchSpec::parse("conda-forge::tzdata").value();
            REQUIRE(ms.channel().value().str() == "conda-forge");
            REQUIRE(ms.name().str() == "tzdata");
            REQUIRE(ms.version().is_explicitly_free());
            REQUIRE(ms.build_string().is_explicitly_free());
            REQUIRE(ms.str() == "conda-forge::tzdata");
        }

        SECTION("conda-forge/noarch::tzdata")
        {
            auto ms = MatchSpec::parse("conda-forge/noarch::tzdata").value();
            REQUIRE(ms.channel().value().str() == "conda-forge[noarch]");
            REQUIRE(ms.name().str() == "tzdata");
            REQUIRE(ms.version().is_explicitly_free());
            REQUIRE(ms.build_string().is_explicitly_free());
            REQUIRE(ms.str() == "conda-forge[noarch]::tzdata");
        }

        SECTION("conda-forge[noarch]::tzdata")
        {
            auto ms = MatchSpec::parse("conda-forge/noarch::tzdata").value();
            REQUIRE(ms.channel().value().str() == "conda-forge[noarch]");
            REQUIRE(ms.name().str() == "tzdata");
            REQUIRE(ms.version().is_explicitly_free());
            REQUIRE(ms.build_string().is_explicitly_free());
            REQUIRE(ms.str() == "conda-forge[noarch]::tzdata");
        }

        SECTION("pkgs/main::tzdata")
        {
            auto ms = MatchSpec::parse("pkgs/main::tzdata").value();
            REQUIRE(ms.channel().value().str() == "pkgs/main");
            REQUIRE(ms.name().str() == "tzdata");
            REQUIRE(ms.version().is_explicitly_free());
            REQUIRE(ms.build_string().is_explicitly_free());
            REQUIRE(ms.str() == "pkgs/main::tzdata");
        }

        SECTION("pkgs/main/noarch::tzdata")
        {
            auto ms = MatchSpec::parse("pkgs/main/noarch::tzdata").value();
            REQUIRE(ms.channel().value().str() == "pkgs/main[noarch]");
            REQUIRE(ms.name().str() == "tzdata");
            REQUIRE(ms.version().is_explicitly_free());
            REQUIRE(ms.build_string().is_explicitly_free());
            REQUIRE(ms.str() == "pkgs/main[noarch]::tzdata");
        }

        SECTION("conda-forge[noarch]::tzdata[subdir=linux64]")
        {
            auto ms = MatchSpec::parse("conda-forge[noarch]::tzdata[subdir=linux64]").value();
            REQUIRE(ms.channel().value().str() == "conda-forge[noarch]");
            REQUIRE(ms.platforms().value().get() == MatchSpec::platform_set{ "noarch" });
            REQUIRE(ms.name().str() == "tzdata");
            REQUIRE(ms.version().is_explicitly_free());
            REQUIRE(ms.build_string().is_explicitly_free());
            REQUIRE(ms.str() == "conda-forge[noarch]::tzdata");
        }

        SECTION("conda-forge::tzdata[subdir=mamba-37]")
        {
            auto ms = MatchSpec::parse("conda-forge::tzdata[subdir=mamba-37]").value();
            REQUIRE(ms.channel().value().str() == "conda-forge[mamba-37]");
            REQUIRE(ms.platforms().value().get() == MatchSpec::platform_set{ "mamba-37" });
            REQUIRE(ms.name().str() == "tzdata");
            REQUIRE(ms.version().is_explicitly_free());
            REQUIRE(ms.build_string().is_explicitly_free());
            REQUIRE(ms.str() == "conda-forge[mamba-37]::tzdata");
        }

        SECTION("conda-canary/linux-64::conda==4.3.21.post699+1dab973=py36h4a561cd_0")
        {
            auto ms = MatchSpec::parse(
                          "conda-canary/linux-64::conda==4.3.21.post699+1dab973=py36h4a561cd_0"
            )
                          .value();
            REQUIRE(ms.channel().value().str() == "conda-canary[linux-64]");
            REQUIRE(ms.platforms().value().get() == MatchSpec::platform_set{ "linux-64" });
            REQUIRE(ms.name().str() == "conda");
            REQUIRE(ms.version().str() == "==4.3.21.post699+1dab973");
            REQUIRE(ms.build_string().str() == "py36h4a561cd_0");
            REQUIRE(ms.str() == "conda-canary[linux-64]::conda==4.3.21.post699+1dab973=py36h4a561cd_0");
        }

        SECTION("libblas[build=^.*(accelerate|mkl)$]")
        {
            auto ms = MatchSpec::parse("libblas[build=^.*(accelerate|mkl)$]").value();
            REQUIRE(ms.name().str() == "libblas");
            REQUIRE(ms.build_string().str() == "^.*(accelerate|mkl)$");
            REQUIRE_FALSE(ms.build_string().is_glob());
        }
    }

    TEST_CASE("parse_url", "[mamba::specs][mamba::specs::MatchSpec]")
    {
        SECTION("https://conda.com/pkg-2-bld.conda")
        {
            auto ms = MatchSpec::parse_url("https://conda.com/pkg-2-bld.conda").value();
            REQUIRE(ms.is_file());
            REQUIRE(ms.name().str() == "pkg");
            REQUIRE(ms.version().str() == "==2");
            REQUIRE(ms.str() == "https://conda.com/pkg-2-bld.conda");
            REQUIRE(ms.build_string().str() == "bld");
            REQUIRE(ms.filename() == "pkg-2-bld.conda");
        }

        SECTION("/home/usr/mamba/micromamba/tests/data/cph_test_data-0.0.1-0.tar.bz2")
        {
            auto ms = MatchSpec::parse_url(
                          "/home/usr/mamba/micromamba/tests/data/cph_test_data-0.0.1-0.tar.bz2"
            )
                          .value();
            REQUIRE(ms.is_file());
            REQUIRE(ms.name().str() == "cph_test_data");
            REQUIRE(ms.version().str() == "==0.0.1");
            REQUIRE(ms.str() == "/home/usr/mamba/micromamba/tests/data/cph_test_data-0.0.1-0.tar.bz2");
            REQUIRE(ms.build_string().str() == "0");
            REQUIRE(ms.filename() == "cph_test_data-0.0.1-0.tar.bz2");
        }

        SECTION(R"(D:\a\mamba\mamba\micromamba\tests\data\cph_test_data-0.0.1-0.tar.bz2)")
        {
            if (util::on_win)
            {
                auto ms = MatchSpec::parse_url(
                              R"(D:\a\mamba\mamba\micromamba\tests\data\cph_test_data-0.0.1-0.tar.bz2)"
                )
                              .value();
                REQUIRE(ms.is_file());
                REQUIRE(ms.name().str() == "cph_test_data");
                REQUIRE(ms.version().str() == "==0.0.1");
                REQUIRE(
                    ms.str() == "D:/a/mamba/mamba/micromamba/tests/data/cph_test_data-0.0.1-0.tar.bz2"
                );
                REQUIRE(ms.build_string().str() == "0");
                REQUIRE(ms.filename() == "cph_test_data-0.0.1-0.tar.bz2");
            }
        }
    }

    TEST_CASE("Conda discrepancies", "[mamba::specs][mamba::specs::MatchSpec]")
    {
        SECTION("python=3.7=bld")
        {
            // For some reason, conda parses version differently in `python=3.7` and
            // `python=3.7=bld`.
            // It is `=3.7` and `==3.7` in the later.
            auto ms = MatchSpec::parse("python=3.7=bld").value();
            REQUIRE(ms.version().str() == "=3.7");
            REQUIRE(ms.build_string().str() == "bld");
        }

        SECTION("python[version>3]")
        {
            // Supported by conda but we consider to be already served by `version=">3"`
            auto error = MatchSpec::parse("python[version>3]").error();
            REQUIRE(util::contains(error.what(), R"(use "version='>3'" instead)"));
        }

        SECTION("python[version=3.7]")
        {
            // Ambiguous, `version=` parsed as attribute assignment, which leads to
            // `3.7` (similar to `==3.7`) being parsed as VersionSpec
            auto ms = MatchSpec::parse("python[version=3.7]").value();
            REQUIRE(ms.version().str() == "==3.7");
        }
    }

    TEST_CASE("is_simple", "[mamba::specs][mamba::specs::MatchSpec]")
    {
        SECTION("Positive")
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
                REQUIRE(ms.is_simple());
            }
        }

        SECTION("Negative")
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
                REQUIRE_FALSE(ms.is_simple());
            }
        }
    }

    TEST_CASE("MatchSpec::to_named_spec", "[mamba::specs][mamba::specs::MatchSpec]")
    {
        SECTION("Alrealy name only")
        {
            const auto str = GENERATE("foo", "foo ");
            const auto ms = MatchSpec::parse(str).value();
            const auto ms_named = ms.to_named_spec();
            REQUIRE(ms == ms_named);
        }

        SECTION("With more restrictions")
        {
            const auto str = GENERATE("foo>1.0", "foo =*", "foo=3=bld");
            const auto ms = MatchSpec::parse(str).value();
            const auto ms_named = ms.to_named_spec();
            REQUIRE(ms_named.name() == ms.name());
            REQUIRE(ms_named.version().is_explicitly_free());
            REQUIRE(ms_named.build_string().is_explicitly_free());
            REQUIRE(ms_named.build_number().is_explicitly_free());
        }
    }

    TEST_CASE("MatchSpec::contains", "[mamba::specs][mamba::specs::MatchSpec]")
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

        SECTION("python")
        {
            const auto ms = "python"_ms;
            REQUIRE(ms.contains_except_channel(Pkg{ "python" }));
            REQUIRE_FALSE(ms.contains_except_channel(Pkg{ "pypy" }));

            REQUIRE(ms.contains_except_channel(PackageInfo{ "python" }));
            REQUIRE_FALSE(ms.contains_except_channel(PackageInfo{ "pypy" }));
        }

        SECTION("py*")
        {
            const auto ms = "py*"_ms;
            REQUIRE(ms.contains_except_channel(Pkg{ "python" }));
            REQUIRE(ms.contains_except_channel(Pkg{ "pypy" }));
            REQUIRE_FALSE(ms.contains_except_channel(Pkg{ "rust" }));

            REQUIRE(ms.contains_except_channel(PackageInfo{ "python" }));
            REQUIRE(ms.contains_except_channel(PackageInfo{ "pypy" }));
            REQUIRE_FALSE(ms.contains_except_channel(PackageInfo{ "rust" }));
        }

        SECTION("py*>=3.7")
        {
            const auto ms = "py*>=3.7"_ms;
            REQUIRE(ms.contains_except_channel(Pkg{ "python", "3.7"_v }));
            REQUIRE_FALSE(ms.contains_except_channel(Pkg{ "pypy", "3.6"_v }));
            REQUIRE_FALSE(ms.contains_except_channel(Pkg{ "rust", "3.7"_v }));

            REQUIRE(ms.contains_except_channel(PackageInfo{ "python", "3.7", "bld", 0 }));
            REQUIRE_FALSE(ms.contains_except_channel(PackageInfo{ "pypy", "3.6", "bld", 0 }));
            REQUIRE_FALSE(ms.contains_except_channel(PackageInfo{ "rust", "3.7", "bld", 0 }));
        }

        SECTION("py*>=3.7=*cpython")
        {
            const auto ms = "py*>=3.7=*cpython"_ms;
            REQUIRE(ms.contains_except_channel(Pkg{ "python", "3.7"_v, "37_cpython" }));
            REQUIRE_FALSE(ms.contains_except_channel(Pkg{ "pypy", "3.6"_v, "cpython" }));
            REQUIRE_FALSE(ms.contains_except_channel(Pkg{ "pypy", "3.8"_v, "pypy" }));
            REQUIRE_FALSE(ms.contains_except_channel(Pkg{ "rust", "3.7"_v, "cpyhton" }));
        }

        SECTION("py*[version='>=3.7', build=*cpython]")
        {
            const auto ms = "py*[version='>=3.7', build=*cpython]"_ms;
            REQUIRE(ms.contains_except_channel(Pkg{ "python", "3.7"_v, "37_cpython" }));
            REQUIRE_FALSE(ms.contains_except_channel(Pkg{ "pypy", "3.6"_v, "cpython" }));
            REQUIRE_FALSE(ms.contains_except_channel(Pkg{ "pypy", "3.8"_v, "pypy" }));
            REQUIRE_FALSE(ms.contains_except_channel(Pkg{ "rust", "3.7"_v, "cpyhton" }));
        }

        SECTION("pkg[build_number='>3']")
        {
            const auto ms = "pkg[build_number='>3']"_ms;
            auto pkg = Pkg{ "pkg" };
            pkg.build_number = 4;
            REQUIRE(ms.contains_except_channel(pkg));
            pkg.build_number = 2;
            REQUIRE_FALSE(ms.contains_except_channel(pkg));
        }

        SECTION("name *,*.* build*")
        {
            const auto ms = "name *,*.* build*"_ms;
            REQUIRE(ms.contains_except_channel(Pkg{ "name", "3.7"_v, "build_foo" }));
            REQUIRE_FALSE(ms.contains_except_channel(Pkg{ "name", "3"_v, "build_foo" }));
            REQUIRE_FALSE(ms.contains_except_channel(Pkg{ "name", "3.7"_v, "bar" }));
        }

        SECTION("pkg[md5=helloiamnotreallymd5haha]")
        {
            const auto ms = "pkg[md5=helloiamnotreallymd5haha]"_ms;

            auto pkg = Pkg{ "pkg" };
            pkg.md5 = "helloiamnotreallymd5haha";
            REQUIRE(ms.contains_except_channel(pkg));

            for (auto md5 : { "helloiamnotreallymd5hahaevillaugh", "hello", "" })
            {
                CAPTURE(std::string_view(md5));
                pkg.md5 = md5;
                REQUIRE_FALSE(ms.contains_except_channel(pkg));
            }
        }

        SECTION("pkg[sha256=helloiamnotreallysha256hihi]")
        {
            const auto ms = "pkg[sha256=helloiamnotreallysha256hihi]"_ms;

            auto pkg = Pkg{ "pkg" };
            pkg.sha256 = "helloiamnotreallysha256hihi";
            REQUIRE(ms.contains_except_channel(pkg));

            for (auto sha256 : { "helloiamnotreallysha256hihicutelaugh", "hello", "" })
            {
                CAPTURE(std::string_view(sha256));
                pkg.sha256 = sha256;
                REQUIRE_FALSE(ms.contains_except_channel(pkg));
            }
        }

        SECTION("pkg[license=helloiamnotreallylicensehoho]")
        {
            const auto ms = "pkg[license=helloiamnotreallylicensehoho]"_ms;

            auto pkg = Pkg{ "pkg" };
            pkg.license = "helloiamnotreallylicensehoho";
            REQUIRE(ms.contains_except_channel(pkg));

            for (auto license : { "helloiamnotreallylicensehohodadlaugh", "hello", "" })
            {
                CAPTURE(std::string_view(license));
                pkg.license = license;
                REQUIRE_FALSE(ms.contains_except_channel(pkg));
            }
        }

        SECTION("pkg[subdir='linux-64,linux-64-512']")
        {
            const auto ms = "pkg[subdir='linux-64,linux-64-512']"_ms;

            auto pkg = Pkg{ "pkg" };

            for (auto plat : { "linux-64", "linux-64-512" })
            {
                CAPTURE(std::string_view(plat));
                pkg.platform = plat;
                REQUIRE(ms.contains_except_channel(pkg));
            }

            for (auto plat : { "linux", "linux-512", "", "linux-64,linux-64-512" })
            {
                CAPTURE(std::string_view(plat));
                pkg.platform = plat;
                REQUIRE_FALSE(ms.contains_except_channel(pkg));
            }
        }

        SECTION("pkg[track_features='mkl,openssl']")
        {
            using string_set = typename MatchSpec::string_set;

            const auto ms = "pkg[track_features='mkl,openssl']"_ms;

            auto pkg = Pkg{ "pkg" };

            for (auto tfeats : { string_set{ "openssl", "mkl" } })
            {
                pkg.track_features = tfeats;
                REQUIRE(ms.contains_except_channel(pkg));
            }

            for (auto tfeats : { string_set{ "openssl" }, string_set{ "mkl" }, string_set{} })
            {
                pkg.track_features = tfeats;
                REQUIRE_FALSE(ms.contains_except_channel(pkg));
            }
        }

        SECTION("Complex")
        {
            const auto ms = "py*>=3.7=bld[build_number='<=2', md5=lemd5, track_features='mkl,openssl']"_ms;

            REQUIRE(ms.contains_except_channel(Pkg{
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
            REQUIRE(ms.contains_except_channel(Pkg{
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
            REQUIRE_FALSE(ms.contains_except_channel(Pkg{
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
            REQUIRE_FALSE(ms.contains_except_channel(Pkg{
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

        SECTION("pytorch=2.3.1=py3.10_cuda11.8*")
        {
            // Check that it contains `pytorch=2.3.1=py3.10_cuda11.8_cudnn8.7.0_0`

            const auto ms = "pytorch=2.3.1=py3.10_cuda11.8*"_ms;

            REQUIRE(ms.contains_except_channel(Pkg{
                /* .name= */ "pytorch",
                /* .version= */ "2.3.1"_v,
                /* .build_string= */ "py3.10_cuda11.8_cudnn8.7.0_0",
                /* .build_number= */ 0,
                /* .md5= */ "lemd5",
                /* .sha256= */ "somesha256",
                /* .license= */ "GPL",
                /* .platform= */ "linux-64",
                /* .track_features =*/{},
            }));
        }

        SECTION("pytorch~=2.3.1=py3.10_cuda11.8*")
        {
            const auto ms = "pytorch~=2.3.1=py3.10_cuda11.8*"_ms;

            REQUIRE(ms.contains_except_channel(Pkg{
                /* .name= */ "pytorch",
                /* .version= */ "2.3.1"_v,
                /* .build_string= */ "py3.10_cuda11.8_cudnn8.7.0_0",
                /* .build_number= */ 0,
                /* .md5= */ "lemd5",
                /* .sha256= */ "somesha256",
                /* .license= */ "GPL",
                /* .platform= */ "linux-64",
                /* .track_features =*/{},
            }));

            REQUIRE(ms.contains_except_channel(Pkg{
                /* .name= */ "pytorch",
                /* .version= */ "2.3.2"_v,
                /* .build_string= */ "py3.10_cuda11.8_cudnn8.7.0_0",
                /* .build_number= */ 0,
                /* .md5= */ "lemd5",
                /* .sha256= */ "somesha256",
                /* .license= */ "GPL",
                /* .platform= */ "linux-64",
                /* .track_features =*/{},
            }));

            REQUIRE_FALSE(ms.contains_except_channel(Pkg{
                /* .name= */ "pytorch",
                /* .version= */ "2.4.0"_v,
                /* .build_string= */ "py3.10_cuda11.8_cudnn8.7.0_0",
                /* .build_number= */ 0,
                /* .md5= */ "lemd5",
                /* .sha256= */ "somesha256",
                /* .license= */ "GPL",
                /* .platform= */ "linux-64",
                /* .track_features =*/{},
            }));

            REQUIRE_FALSE(ms.contains_except_channel(Pkg{
                /* .name= */ "pytorch",
                /* .version= */ "3.0"_v,
                /* .build_string= */ "py3.10_cuda11.8_cudnn8.7.0_0",
                /* .build_number= */ 0,
                /* .md5= */ "lemd5",
                /* .sha256= */ "somesha256",
                /* .license= */ "GPL",
                /* .platform= */ "linux-64",
                /* .track_features =*/{},
            }));

            REQUIRE_FALSE(ms.contains_except_channel(Pkg{
                /* .name= */ "pytorch",
                /* .version= */ "2.3.0"_v,
                /* .build_string= */ "py3.10_cuda11.8_cudnn8.7.0_0",
                /* .build_number= */ 0,
                /* .md5= */ "lemd5",
                /* .sha256= */ "somesha256",
                /* .license= */ "GPL",
                /* .platform= */ "linux-64",
                /* .track_features =*/{},
            }));
        }

        SECTION("numpy~=1.26.0")
        {
            const auto ms = "numpy~=1.26.0"_ms;

            REQUIRE(ms.contains_except_channel(Pkg{
                /* .name= */ "numpy",
                /* .version= */ "1.26.0"_v,
                /* .build_string= */ "py310h1d0b8b9_0",
                /* .build_number= */ 0,
                /* .md5= */ "lemd5",
                /* .sha256= */ "somesha256",
                /* .license= */ "GPL",
                /* .platform= */ "linux-64",
                /* .track_features =*/{},
            }));

            REQUIRE(ms.contains_except_channel(Pkg{
                /* .name= */ "numpy",
                /* .version= */ "1.26.1"_v,
                /* .build_string= */ "py310h1d0b8b9_0",
                /* .build_number= */ 0,
                /* .md5= */ "lemd5",
                /* .sha256= */ "somesha256",
                /* .license= */ "GPL",
                /* .platform= */ "linux-64",
                /* .track_features =*/{},
            }));

            REQUIRE_FALSE(ms.contains_except_channel(Pkg{
                /* .name= */ "numpy",
                /* .version= */ "1.27"_v,
                /* .build_string= */ "py310h1d0b8b9_0",
                /* .build_number= */ 0,
                /* .md5= */ "lemd5",
                /* .sha256= */ "somesha256",
                /* .license= */ "GPL",
                /* .platform= */ "linux-64",
                /* .track_features =*/{},
            }));

            REQUIRE_FALSE(ms.contains_except_channel(Pkg{
                /* .name= */ "numpy",
                /* .version= */ "2.0.0"_v,
                /* .build_string= */ "py310h1d0b8b9_1",
                /* .build_number= */ 1,
                /* .md5= */ "lemd5",
                /* .sha256= */ "somesha256",
                /* .license= */ "GPL",
                /* .platform= */ "linux-64",
                /* .track_features =*/{},
            }));

            REQUIRE_FALSE(ms.contains_except_channel(Pkg{
                /* .name= */ "numpy",
                /* .version= */ "1.25.0"_v,
                /* .build_string= */ "py310h1d0b8b9_0",
                /* .build_number= */ 0,
                /* .md5= */ "lemd5",
                /* .sha256= */ "somesha256",
                /* .license= */ "GPL",
                /* .platform= */ "linux-64",
                /* .track_features =*/{},
            }));
        }

        SECTION("numpy~=1.26")
        {
            const auto ms = "numpy~=1.26"_ms;

            REQUIRE(ms.contains_except_channel(Pkg{
                /* .name= */ "numpy",
                /* .version= */ "1.26.0"_v,
                /* .build_string= */ "py310h1d0b8b9_0",
                /* .build_number= */ 0,
                /* .md5= */ "lemd5",
                /* .sha256= */ "somesha256",
                /* .license= */ "GPL",
                /* .platform= */ "linux-64",
                /* .track_features =*/{},
            }));

            REQUIRE(ms.contains_except_channel(Pkg{
                /* .name= */ "numpy",
                /* .version= */ "1.26.1"_v,
                /* .build_string= */ "py310h1d0b8b9_0",
                /* .build_number= */ 0,
                /* .md5= */ "lemd5",
                /* .sha256= */ "somesha256",
                /* .license= */ "GPL",
                /* .platform= */ "linux-64",
                /* .track_features =*/{},
            }));

            REQUIRE(ms.contains_except_channel(Pkg{
                /* .name= */ "numpy",
                /* .version= */ "1.27"_v,
                /* .build_string= */ "py310h1d0b8b9_0",
                /* .build_number= */ 0,
                /* .md5= */ "lemd5",
                /* .sha256= */ "somesha256",
                /* .license= */ "GPL",
                /* .platform= */ "linux-64",
                /* .track_features =*/{},
            }));

            REQUIRE_FALSE(ms.contains_except_channel(Pkg{
                /* .name= */ "numpy",
                /* .version= */ "2.0.0"_v,
                /* .build_string= */ "py310h1d0b8b9_1",
                /* .build_number= */ 1,
                /* .md5= */ "lemd5",
                /* .sha256= */ "somesha256",
                /* .license= */ "GPL",
                /* .platform= */ "linux-64",
                /* .track_features =*/{},
            }));
        }

        SECTION("python=3.*")
        {
            const auto ms = "python=3.*"_ms;

            REQUIRE(ms.contains_except_channel(Pkg{
                /* .name= */ "python",
                /* .version= */ "3.12.0"_v,
                /* .build_string= */ "bld",
                /* .build_number= */ 0,
                /* .md5= */ "lemd5",
                /* .sha256= */ "somesha256",
                /* .license= */ "some-license",
                /* .platform= */ "linux-64",
                /* .track_features =*/{},
            }));

            REQUIRE_FALSE(ms.contains_except_channel(Pkg{
                /* .name= */ "python",
                /* .version= */ "2.7.12"_v,
                /* .build_string= */ "bld",
                /* .build_number= */ 0,
                /* .md5= */ "lemd5",
                /* .sha256= */ "somesha256",
                /* .license= */ "some-license",
                /* .platform= */ "linux-64",
                /* .track_features =*/{},
            }));
        }

        SECTION("python=3.*.1")
        {
            const auto ms = "python=3.*.1"_ms;

            REQUIRE(ms.contains_except_channel(Pkg{
                /* .name= */ "python",
                /* .version= */ "3.12.1"_v,
                /* .build_string= */ "bld",
                /* .build_number= */ 0,
                /* .md5= */ "lemd5",
                /* .sha256= */ "somesha256",
                /* .license= */ "some-license",
                /* .platform= */ "linux-64",
                /* .track_features =*/{},
            }));

            REQUIRE_FALSE(ms.contains_except_channel(Pkg{
                /* .name= */ "python",
                /* .version= */ "3.12.0"_v,
                /* .build_string= */ "bld",
                /* .build_number= */ 0,
                /* .md5= */ "lemd5",
                /* .sha256= */ "somesha256",
                /* .license= */ "some-license",
                /* .platform= */ "linux-64",
                /* .track_features =*/{},
            }));
        }

        SECTION("python=*.13.1")
        {
            const auto ms = "python=*.13.1"_ms;

            REQUIRE(ms.contains_except_channel(Pkg{
                /* .name= */ "python",
                /* .version= */ "3.13.1"_v,
                /* .build_string= */ "bld",
                /* .build_number= */ 0,
                /* .md5= */ "lemd5",
                /* .sha256= */ "somesha256",
                /* .license= */ "some-license",
                /* .platform= */ "linux-64",
                /* .track_features =*/{},
            }));

            REQUIRE_FALSE(ms.contains_except_channel(Pkg{
                /* .name= */ "python",
                /* .version= */ "3.12.0"_v,
                /* .build_string= */ "bld",
                /* .build_number= */ 0,
                /* .md5= */ "lemd5",
                /* .sha256= */ "somesha256",
                /* .license= */ "some-license",
                /* .platform= */ "linux-64",
                /* .track_features =*/{},
            }));
        }
    }

    TEST_CASE("MatchSpec comparability and hashability", "[mamba::specs][mamba::specs::MatchSpec]")
    {
        using namespace specs::match_spec_literals;
        using namespace specs::version_literals;

        const auto spec1 = "py*>=3.7=bld[build_number='<=2', md5=lemd5, track_features='mkl,openssl']"_ms;

        // Create an identical specification
        const auto spec2 = "py*>=3.7=bld[build_number='<=2', md5=lemd5, track_features='mkl,openssl']"_ms;

        // Create a different specification
        const auto spec3 = "py*>=3.7=bld[build_number='<=2', md5=lemd5, track_features='mkl']"_ms;

        // Check that the two copies are equal
        REQUIRE(spec1 == spec2);
        // Check that the different specification is not equal to the first one
        REQUIRE(spec1 != spec3);

        // Check that the hash of the two copies is the same
        auto spec1_hash = std::hash<MatchSpec>{}(spec1);
        auto spec2_hash = std::hash<MatchSpec>{}(spec2);
        auto spec3_hash = std::hash<MatchSpec>{}(spec3);

        REQUIRE(spec1_hash == spec2_hash);
        REQUIRE(spec1_hash != spec3_hash);
    }

    auto repodata_all_depends(const std::string& path)
        -> std::vector<std::tuple<std::string, std::string>>
    {
        auto input = std::ifstream(path);
        if (!input.is_open())
        {
            throw std::runtime_error("Failed to open file: " + std::string(path));
        }

        auto j = nlohmann::json::parse(input);

        if (!j.contains("packages") || !j["packages"].is_object())
        {
            throw std::runtime_error(R"(Missing or invalid "packages" field)");
        }

        auto result = std::vector<std::tuple<std::string, std::string>>();

        for (const auto& [pkg_name, pkg] : j["packages"].items())
        {
            if (!pkg.contains("depends") || !pkg["depends"].is_array())
            {
                throw std::runtime_error(R"(Missing or invalid "depends" in package)");
            }

            for (const auto& dep : pkg["depends"])
            {
                if (!dep.is_string())
                {
                    throw std::runtime_error(R"(Non-string entry in "depends")");
                }
                result.emplace_back(pkg_name, dep.get<std::string>());
            }
        }

        return result;
    }

    TEST_CASE("Repodata MatchSpec::parse", "[mamba::specs][mamba::specs::MatchSpec][.integration]")
    {
        const auto all_ms_str = []()
        {
            if (const auto path = util::get_env("MAMBA_TEST_REPODATA_JSON"))
            {
                return repodata_all_depends(path.value());
            }
            throw std::runtime_error(R"(Please define "MAMBA_TEST_REPODATA_JSON")");
        }();

        for (const auto& [pkg_name, ms_str] : all_ms_str)
        {
            CAPTURE(pkg_name);
            CAPTURE(ms_str);
            REQUIRE(MatchSpec::parse(ms_str).has_value());
        }
    }
}
