// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <doctest/doctest.h>

#include "mamba/api/install.hpp"
#include "mamba/core/util.hpp"

#include "test_data.hpp"

namespace mamba
{
    TEST_SUITE("env_file_reading")
    {
        TEST_CASE("selector")
        {
            using namespace detail;
            if constexpr (on_linux || on_mac)
            {
                CHECK(eval_selector("sel(unix)"));
                if (on_mac)
                {
                    CHECK(eval_selector("sel(osx)"));
                    CHECK_FALSE(eval_selector("sel(linux)"));
                    CHECK_FALSE(eval_selector("sel(win)"));
                }
                else
                {
                    CHECK(eval_selector("sel(linux)"));
                    CHECK_FALSE(eval_selector("sel(osx)"));
                    CHECK_FALSE(eval_selector("sel(win)"));
                }
            }
            else if (on_win)
            {
                CHECK(eval_selector("sel(win)"));
                CHECK_FALSE(eval_selector("sel(osx)"));
                CHECK_FALSE(eval_selector("sel(linux)"));
            }
        }

        TEST_CASE("specs_selection")
        {
            using V = std::vector<std::string>;
            auto res = detail::read_txt_file(test_data_dir / "env_file/env_1.txt");
            CHECK_EQ(res.name, "env_1");
            CHECK_EQ(res.channels, V({ "conda-forge", "bioconda" }));
            CHECK_EQ(res.dependencies, V({ "test1", "test2", "test3" }));
            CHECK_FALSE(res.others_pkg_mgrs_specs.size());

            auto res2 = detail::read_txt_file(test_data_dir / "env_file/env_2.txt");
            CHECK_EQ(res2.name, "env_2");
            CHECK_EQ(res2.channels, V({ "conda-forge", "bioconda" }));
#ifdef __linux__
            CHECK_EQ(res2.dependencies, V({ "test1-unix", "test1-linux", "test2-linux", "test4" }));
#elif __APPLE__
            CHECK_EQ(res2.dependencies, V({ "test1-unix", "test1-osx", "test4" }));
#elif _WIN32
            CHECK_EQ(res2.dependencies, V({ "test1-win", "test4" }));
#endif
            CHECK_FALSE(res2.others_pkg_mgrs_specs.size());
        }

        TEST_CASE("external_pkg_mgrs")
        {
            using V = std::vector<std::string>;
            auto res = detail::read_txt_file(test_data_dir / "env_file/env_3.txt");
            CHECK_EQ(res.name, "env_3");
            CHECK_EQ(res.channels, V({ "conda-forge", "bioconda" }));
            CHECK_EQ(res.dependencies, V({ "test1", "test2", "test3", "pip" }));

            CHECK_EQ(res.others_pkg_mgrs_specs.size(), 1);
            auto o = res.others_pkg_mgrs_specs[0];
            CHECK_EQ(o.pkg_mgr, "pip");
            CHECK_EQ(o.deps, V({ "pytest", "numpy" }));
            CHECK_EQ(o.cwd, fs::absolute(test_data_dir / "env_file"));
        }
    }

}  // namespace mamba
