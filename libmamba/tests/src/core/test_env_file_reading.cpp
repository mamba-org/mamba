// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <catch2/catch_all.hpp>

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
            auto res = detail::read_yaml_file(
                mambatests::test_data_dir / "env_file/env_1.yaml",
                context.platform
            );
            CHECK_EQ(res.name, "env_1");
            CHECK_EQ(res.channels, V({ "conda-forge", "bioconda" }));
            CHECK_EQ(res.dependencies, V({ "test1", "test2", "test3" }));
            REQUIRE_FALSE(res.others_pkg_mgrs_specs.size());

            auto res2 = detail::read_yaml_file(
                mambatests::test_data_dir / "env_file/env_2.yaml",
                context.platform
            );
            CHECK_EQ(res2.name, "env_2");
            CHECK_EQ(res2.channels, V({ "conda-forge", "bioconda" }));
#ifdef __linux__
            CHECK_EQ(res2.dependencies, V({ "test1-unix", "test1-linux", "test2-linux", "test4" }));
#elif __APPLE__
            CHECK_EQ(res2.dependencies, V({ "test1-unix", "test1-osx", "test4" }));
#elif _WIN32
            CHECK_EQ(res2.dependencies, V({ "test1-win", "test4" }));
#endif
            REQUIRE_FALSE(res2.others_pkg_mgrs_specs.size());
        }

        TEST_CASE("external_pkg_mgrs")
        {
            const auto& context = mambatests::context();
            using V = std::vector<std::string>;
            auto res = detail::read_yaml_file(
                mambatests::test_data_dir / "env_file/env_3.yaml",
                context.platform
            );
            CHECK_EQ(res.name, "env_3");
            CHECK_EQ(res.channels, V({ "conda-forge", "bioconda" }));
            CHECK_EQ(res.dependencies, V({ "test1", "test2", "test3", "pip" }));

            CHECK_EQ(res.others_pkg_mgrs_specs.size(), 1);
            auto o = res.others_pkg_mgrs_specs[0];
            CHECK_EQ(o.pkg_mgr, "pip");
            CHECK_EQ(o.deps, V({ "pytest", "numpy" }));
            REQUIRE(o.cwd == fs::absolute(mambatests::test_data_dir / "env_file");
        }
    }

}  // namespace mamba
