#include <gtest/gtest.h>

#include "mamba/pinning.hpp"


namespace mamba
{
    namespace testing
    {
        TEST(pinning, pin_python_spec)
        {
            std::vector<std::string> specs;
            PrefixData prefix_data("");
            ASSERT_EQ(prefix_data.records().size(), 0);

            specs = { "python" };
            pin_python_spec(prefix_data, specs);
            ASSERT_EQ(specs.size(), 1);
            EXPECT_EQ(specs[0], "python");

            specs = { "python=3" };
            pin_python_spec(prefix_data, specs);
            ASSERT_EQ(specs.size(), 1);
            EXPECT_EQ(specs[0], "python=3");

            specs = { "python==3.8" };
            pin_python_spec(prefix_data, specs);
            ASSERT_EQ(specs.size(), 1);
            EXPECT_EQ(specs[0], "python==3.8");

            specs = { "python==3.8.3" };
            pin_python_spec(prefix_data, specs);
            ASSERT_EQ(specs.size(), 1);
            EXPECT_EQ(specs[0], "python==3.8.3");

            specs = { "numpy" };
            pin_python_spec(prefix_data, specs);
            ASSERT_EQ(specs.size(), 1);
            EXPECT_EQ(specs[0], "numpy");

            PackageInfo pkg_info("python", "3.7.10", "abcde", 0);
            prefix_data.m_package_records.insert({ "python", pkg_info });
            ASSERT_EQ(prefix_data.records().size(), 1);

            specs = { "python" };
            pin_python_spec(prefix_data, specs);
            ASSERT_EQ(specs.size(), 1);
            EXPECT_EQ(specs[0], "python=3.7.10");

            specs = { "python==3" };
            pin_python_spec(prefix_data, specs);
            ASSERT_EQ(specs.size(), 1);
            EXPECT_EQ(specs[0], "python=3.7.10");

            specs = { "python=3.*" };
            pin_python_spec(prefix_data, specs);
            ASSERT_EQ(specs.size(), 1);
            EXPECT_EQ(specs[0], "python=3.7.10");

            specs = { "python=3.8" };
            pin_python_spec(prefix_data, specs);
            ASSERT_EQ(specs.size(), 1);
            EXPECT_EQ(specs[0], "python=3.8");

            specs = { "python=3.8.3" };
            pin_python_spec(prefix_data, specs);
            ASSERT_EQ(specs.size(), 1);
            EXPECT_EQ(specs[0], "python=3.8.3");

            specs = { "numpy" };
            pin_python_spec(prefix_data, specs);
            ASSERT_EQ(specs.size(), 2);
            EXPECT_EQ(specs[0], "numpy");
            EXPECT_EQ(specs[1], "python=3.7.10");

            specs = { "numpy", "python" };
            pin_python_spec(prefix_data, specs);
            ASSERT_EQ(specs.size(), 2);
            EXPECT_EQ(specs[0], "numpy");
            EXPECT_EQ(specs[1], "python=3.7.10");

            specs = { "numpy", "python=3.8" };
            pin_python_spec(prefix_data, specs);
            ASSERT_EQ(specs.size(), 2);
            EXPECT_EQ(specs[0], "numpy");
            EXPECT_EQ(specs[1], "python=3.8");

            specs = { "numpy", "python=3.8.3" };
            pin_python_spec(prefix_data, specs);
            ASSERT_EQ(specs.size(), 2);
            EXPECT_EQ(specs[0], "numpy");
            EXPECT_EQ(specs[1], "python=3.8.3");
        }

        TEST(pinning, pin_config_specs)
        {
            std::vector<std::string> specs;
            std::vector<std::string> config_specs;

            specs = { "python" };
            config_specs = { "numpy=1.13", "jupyterlab=3" };
            pin_config_specs(config_specs, specs);
            ASSERT_EQ(specs.size(), 3);
            EXPECT_EQ(specs[0], "python");
            EXPECT_EQ(specs[1], "numpy=1.13");
            EXPECT_EQ(specs[2], "jupyterlab=3");

            specs = { "python=3.7.10" };
            config_specs = { "numpy=1.13", "python=3.7.5" };
            pin_config_specs(config_specs, specs);
            ASSERT_EQ(specs.size(), 3);
            EXPECT_EQ(specs[0], "python=3.7.10");
            EXPECT_EQ(specs[1], "numpy=1.13");
            EXPECT_EQ(specs[2], "python=3.7.5");
        }

        TEST(pinning, pin_file_specs)
        {
            std::vector<std::string> specs;

            auto tempfile = std::make_unique<TemporaryFile>("pinned", "");
            std::string path = tempfile->path();
            std::ofstream out_file(path);
            out_file << "numpy=1.13\njupyterlab=3";
            out_file.close();

            specs = { "python" };
            pin_file_specs(path, specs);
            ASSERT_EQ(specs.size(), 3);
            EXPECT_EQ(specs[0], "python");
            EXPECT_EQ(specs[1], "numpy=1.13");
            EXPECT_EQ(specs[2], "jupyterlab=3");

            specs = { "python=3.7.10" };
            out_file.open(path, std::ofstream::out | std::ofstream::trunc);
            out_file << "numpy=1.13\npython=3.7.5";
            out_file.close();

            pin_file_specs(path, specs);
            ASSERT_EQ(specs.size(), 3);
            EXPECT_EQ(specs[0], "python=3.7.10");
            EXPECT_EQ(specs[1], "numpy=1.13");
            EXPECT_EQ(specs[2], "python=3.7.5");
        }
    }  // namespace testing
}  // namespace mamba
