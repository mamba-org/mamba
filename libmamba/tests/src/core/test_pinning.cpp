#include <gtest/gtest.h>

#include "mamba/core/pinning.hpp"
#include "mamba/core/util.hpp"

namespace mamba
{
    namespace testing
    {
        TEST(pinning, python_pin)
        {
            std::vector<std::string> specs;
            std::string pin;

            auto sprefix_data = PrefixData::create("");
            if (!sprefix_data)
            {
                // TODO: propagate tl::expected mechanism
                throw std::runtime_error("could not load prefix data");
            }
            PrefixData& prefix_data = sprefix_data.value();
            ASSERT_EQ(prefix_data.records().size(), 0);

            specs = { "python" };
            pin = python_pin(prefix_data, specs);
            EXPECT_EQ(pin, "");

            specs = { "python-test" };
            pin = python_pin(prefix_data, specs);
            EXPECT_EQ(pin, "");

            specs = { "python=3" };
            pin = python_pin(prefix_data, specs);
            EXPECT_EQ(pin, "");

            specs = { "python==3.8" };
            pin = python_pin(prefix_data, specs);
            EXPECT_EQ(pin, "");

            specs = { "python==3.8.3" };
            pin = python_pin(prefix_data, specs);
            EXPECT_EQ(pin, "");

            specs = { "numpy" };
            pin = python_pin(prefix_data, specs);
            EXPECT_EQ(pin, "");

            PackageInfo pkg_info("python", "3.7.10", "abcde", 0);
            prefix_data.add_packages({ pkg_info });
            ASSERT_EQ(prefix_data.records().size(), 1);

            specs = { "python" };
            pin = python_pin(prefix_data, specs);
            EXPECT_EQ(pin, "");

            specs = { "numpy" };
            pin = python_pin(prefix_data, specs);
            EXPECT_EQ(pin, "python 3.7.*");

            specs = { "python-test" };
            pin = python_pin(prefix_data, specs);
            EXPECT_EQ(pin, "python 3.7.*");

            specs = { "python==3" };
            pin = python_pin(prefix_data, specs);
            EXPECT_EQ(pin, "");

            specs = { "python=3.*" };
            pin = python_pin(prefix_data, specs);
            EXPECT_EQ(pin, "");

            specs = { "python=3.8" };
            pin = python_pin(prefix_data, specs);
            EXPECT_EQ(pin, "");

            specs = { "python=3.8.3" };
            pin = python_pin(prefix_data, specs);
            EXPECT_EQ(pin, "");

            specs = { "numpy", "python" };
            pin = python_pin(prefix_data, specs);
            EXPECT_EQ(pin, "");
        }

        TEST(pinning, file_pins)
        {
            std::vector<std::string> pins;

            auto tempfile = std::make_unique<TemporaryFile>("pinned", "");
            const auto path = tempfile->path();
            std::ofstream out_file(path.std_path());
            out_file << "numpy=1.13\njupyterlab=3";
            out_file.close();

            pins = file_pins(path);
            ASSERT_EQ(pins.size(), 2);
            EXPECT_EQ(pins[0], "numpy=1.13");
            EXPECT_EQ(pins[1], "jupyterlab=3");

            out_file.open(path.std_path(), std::ofstream::out | std::ofstream::trunc);
            out_file << "numpy=1.13\npython=3.7.5";
            out_file.close();

            pins = file_pins(path);
            ASSERT_EQ(pins.size(), 2);
            EXPECT_EQ(pins[0], "numpy=1.13");
            EXPECT_EQ(pins[1], "python=3.7.5");
        }
    }  // namespace testing
}  // namespace mamba
