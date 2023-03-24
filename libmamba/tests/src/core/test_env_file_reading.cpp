#include <gtest/gtest.h>

#include "mamba/api/install.hpp"

#include "test_data.hpp"

namespace mamba
{
    TEST(env_file_reading, selector)
    {
        using namespace detail;
        if constexpr (on_linux || on_mac)
        {
            EXPECT_TRUE(eval_selector("sel(unix)"));
            if (on_mac)
            {
                EXPECT_TRUE(eval_selector("sel(osx)"));
                EXPECT_FALSE(eval_selector("sel(linux)"));
                EXPECT_FALSE(eval_selector("sel(win)"));
            }
            else
            {
                EXPECT_TRUE(eval_selector("sel(linux)"));
                EXPECT_FALSE(eval_selector("sel(osx)"));
                EXPECT_FALSE(eval_selector("sel(win)"));
            }
        }
        else if (on_win)
        {
            EXPECT_TRUE(eval_selector("sel(win)"));
            EXPECT_FALSE(eval_selector("sel(osx)"));
            EXPECT_FALSE(eval_selector("sel(linux)"));
        }
    }

    TEST(env_file_reading, specs_selection)
    {
        using V = std::vector<std::string>;
        auto res = detail::read_yaml_file(test_data_dir / "env_file/env_1.yaml");
        EXPECT_EQ(res.name, "env_1");
        EXPECT_EQ(res.channels, V({ "conda-forge", "bioconda" }));
        EXPECT_EQ(res.dependencies, V({ "test1", "test2", "test3" }));
        EXPECT_FALSE(res.others_pkg_mgrs_specs.size());

        auto res2 = detail::read_yaml_file(test_data_dir / "env_file/env_2.yaml");
        EXPECT_EQ(res2.name, "env_2");
        EXPECT_EQ(res2.channels, V({ "conda-forge", "bioconda" }));
#ifdef __linux__
        EXPECT_EQ(res2.dependencies, V({ "test1-unix", "test1-linux", "test2-linux", "test4" }));
#elif __APPLE__
        EXPECT_EQ(res2.dependencies, V({ "test1-unix", "test1-osx", "test4" }));
#elif _WIN32
        EXPECT_EQ(res2.dependencies, V({ "test1-win", "test4" }));
#endif
        EXPECT_FALSE(res2.others_pkg_mgrs_specs.size());
    }

    TEST(env_file_reading, external_pkg_mgrs)
    {
        using V = std::vector<std::string>;
        auto res = detail::read_yaml_file(test_data_dir / "env_file/env_3.yaml");
        EXPECT_EQ(res.name, "env_3");
        EXPECT_EQ(res.channels, V({ "conda-forge", "bioconda" }));
        EXPECT_EQ(res.dependencies, V({ "test1", "test2", "test3", "pip" }));

        EXPECT_EQ(res.others_pkg_mgrs_specs.size(), 1);
        auto o = res.others_pkg_mgrs_specs[0];
        EXPECT_EQ(o.pkg_mgr, "pip");
        EXPECT_EQ(o.deps, V({ "pytest", "numpy" }));
        EXPECT_EQ(o.cwd, fs::absolute(test_data_dir / "env_file"));
    }

}  // namespace mamba
