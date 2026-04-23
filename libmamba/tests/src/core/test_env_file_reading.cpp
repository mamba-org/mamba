// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <catch2/catch_all.hpp>

#include "mamba/api/environment_yaml.hpp"
#include "mamba/api/install.hpp"
#include "mamba/util/build.hpp"

#include "mambatests.hpp"

namespace mamba
{
    namespace
    {
        TEST_CASE("selector")
        {
            const auto& context = mambatests::context();
            using namespace detail;
            if constexpr (util::on_linux || util::on_mac)
            {
                REQUIRE(eval_selector("sel(unix)", context.platform));
                if (util::on_mac)
                {
                    REQUIRE(eval_selector("sel(osx)", context.platform));
                    REQUIRE_FALSE(eval_selector("sel(linux)", context.platform));
                    REQUIRE_FALSE(eval_selector("sel(win)", context.platform));
                }
                else
                {
                    REQUIRE(eval_selector("sel(linux)", context.platform));
                    REQUIRE_FALSE(eval_selector("sel(osx)", context.platform));
                    REQUIRE_FALSE(eval_selector("sel(win)", context.platform));
                }
            }
            else if (util::on_win)
            {
                REQUIRE(eval_selector("sel(win)", context.platform));
                REQUIRE_FALSE(eval_selector("sel(osx)", context.platform));
                REQUIRE_FALSE(eval_selector("sel(linux)", context.platform));
            }
        }

        TEST_CASE("specs_selection")
        {
            const auto& context = mambatests::context();
            using V = std::vector<std::string>;
            auto res = file_to_yaml_contents(
                context,
                (mambatests::test_data_dir / "env_file/env_1.yaml").string(),
                context.platform,
                false
            );
            REQUIRE(res.name == "env_1");
            REQUIRE(res.channels == V({ "conda-forge", "bioconda" }));
            REQUIRE(res.dependencies == V({ "test1", "test2", "test3" }));
            REQUIRE_FALSE(res.others_pkg_mgrs_specs.size());

            auto res2 = file_to_yaml_contents(
                context,
                (mambatests::test_data_dir / "env_file/env_2.yaml").string(),
                context.platform,
                false
            );
            REQUIRE(res2.name == "env_2");
            REQUIRE(res2.channels == V({ "conda-forge", "bioconda" }));
#ifdef __linux__
            REQUIRE(res2.dependencies == V({ "test1-unix", "test1-linux", "test2-linux", "test4" }));
#elif __APPLE__
            REQUIRE(res2.dependencies == V({ "test1-unix", "test1-osx", "test4" }));
#elif _WIN32
            REQUIRE(res2.dependencies == V({ "test1-win", "test4" }));
#endif
            REQUIRE_FALSE(res2.others_pkg_mgrs_specs.size());
        }

        TEST_CASE("external_pkg_mgrs")
        {
            const auto& context = mambatests::context();
            using V = std::vector<std::string>;
            auto res = file_to_yaml_contents(
                context,
                (mambatests::test_data_dir / "env_file/env_3.yaml").string(),
                context.platform,
                false
            );
            REQUIRE(res.name == "env_3");
            REQUIRE(res.channels == V({ "conda-forge", "bioconda" }));
            REQUIRE(res.dependencies == V({ "test1", "test2", "test3", "pip" }));

            REQUIRE(res.others_pkg_mgrs_specs.size() == 1);
            auto o = res.others_pkg_mgrs_specs[0];
            REQUIRE(o.pkg_mgr == "pip");
            REQUIRE(o.deps == V({ "pytest", "numpy" }));
            REQUIRE(o.cwd == mamba::fs::absolute(mambatests::test_data_dir / "env_file").string());
        }

        TEST_CASE("remote_yaml_file")
        {
            SECTION("classic_env_yaml_file")
            {
                const auto& context = mambatests::context();
                using V = std::vector<std::string>;
                auto res = file_to_yaml_contents(
                    context,
                    "https://raw.githubusercontent.com/mamba-org/mamba/refs/heads/main/micromamba/tests/env-create-export.yaml",
                    context.platform,
                    false
                );
                REQUIRE(res.name == "");
                REQUIRE(res.channels == V({ "https://conda.anaconda.org/conda-forge" }));
                REQUIRE(res.dependencies == V({ "micromamba=0.24.0" }));
            }

            SECTION("env_yaml_file_with_pip")
            {
                const auto& context = mambatests::context();
                using V = std::vector<std::string>;
                auto res = file_to_yaml_contents(
                    context,
                    "https://raw.githubusercontent.com/mamba-org/mamba/refs/heads/main/libmamba/tests/data/env_file/env_3.yaml",
                    context.platform,
                    false
                );
                REQUIRE(res.name == "env_3");
                REQUIRE(res.channels == V({ "conda-forge", "bioconda" }));
                REQUIRE(res.dependencies == V({ "test1", "test2", "test3", "pip" }));

                REQUIRE(res.others_pkg_mgrs_specs.size() == 1);
                auto o = res.others_pkg_mgrs_specs[0];
                REQUIRE(o.pkg_mgr == "pip");
                REQUIRE(o.deps == V({ "pytest", "numpy" }));
                REQUIRE(
                    o.cwd == "https://raw.githubusercontent.com/mamba-org/mamba/refs/heads/main/libmamba/tests/data/env_file/env_3.yaml"
                );
            }

            SECTION("env_yaml_file_with_uv_override")
            {
                const auto& context = mambatests::context();
                using V = std::vector<std::string>;
                auto res = file_to_yaml_contents(
                    context,
                    "https://raw.githubusercontent.com/iisakkirotko/mamba/refs/heads/yaml-install-uv/libmamba/tests/data/env_file/env_4.yaml",
                    context.platform,
                    false
                );
                REQUIRE(res.name == "env_4");
                REQUIRE(res.channels == V({ "conda-forge", "bioconda" }));
                REQUIRE(res.dependencies == V({ "test1", "test2", "uv" }));

                REQUIRE(res.others_pkg_mgrs_specs.size() == 1);
                auto o = res.others_pkg_mgrs_specs[0];
                REQUIRE(o.pkg_mgr == "uv");
                REQUIRE(o.deps == V({ "pytest", "numpy" }));
                REQUIRE(
                    o.cwd == "https://raw.githubusercontent.com/iisakkirotko/mamba/refs/heads/yaml-install-uv/libmamba/tests/data/env_file/env_4.yaml"
                );
            }

            SECTION("env_yaml_file_with_uv")
            {
                const auto& context = mambatests::context();
                using V = std::vector<std::string>;
                auto res = file_to_yaml_contents(
                    context,
                    "https://raw.githubusercontent.com/mamba-org/mamba/refs/heads/main/libmamba/tests/data/env_file/env_3.yaml",
                    context.platform,
                    true
                );
                REQUIRE(res.name == "env_3");
                REQUIRE(res.channels == V({ "conda-forge", "bioconda" }));
                REQUIRE(res.dependencies == V({ "test1", "test2", "test3", "uv" }));

                REQUIRE(res.others_pkg_mgrs_specs.size() == 1);
                auto o = res.others_pkg_mgrs_specs[0];
                REQUIRE(o.pkg_mgr == "uv");
                REQUIRE(o.deps == V({ "pytest", "numpy" }));
                REQUIRE(
                    o.cwd == "https://raw.githubusercontent.com/mamba-org/mamba/refs/heads/main/libmamba/tests/data/env_file/env_3.yaml"
                );
            }

            SECTION("env_yaml_file_with_specs_selection")
            {
                const auto& context = mambatests::context();
                using V = std::vector<std::string>;
                auto res = file_to_yaml_contents(
                    context,
                    "https://raw.githubusercontent.com/mamba-org/mamba/refs/heads/main/libmamba/tests/data/env_file/env_2.yaml",
                    context.platform,
                    false
                );
                REQUIRE(res.name == "env_2");
                REQUIRE(res.channels == V({ "conda-forge", "bioconda" }));
#ifdef __linux__
                REQUIRE(res.dependencies == V({ "test1-unix", "test1-linux", "test2-linux", "test4" }));
#elif __APPLE__
                REQUIRE(res.dependencies == V({ "test1-unix", "test1-osx", "test4" }));
#elif _WIN32
                REQUIRE(res.dependencies == V({ "test1-win", "test4" }));
#endif
                REQUIRE_FALSE(res.others_pkg_mgrs_specs.size());
            }
        }
    }

}  // namespace mamba
