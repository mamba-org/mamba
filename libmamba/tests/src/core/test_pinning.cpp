// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <catch2/catch_all.hpp>

#include "mamba/core/channel_context.hpp"
#include "mamba/core/pinning.hpp"
#include "mamba/core/prefix_data.hpp"
#include "mamba/core/util.hpp"

#include "mambatests.hpp"

namespace mamba
{
    namespace testing
    {
        namespace
        {
            TEST_CASE("python_pin")
            {
                std::vector<std::string> specs;
                std::string pin;

                auto channel_context = ChannelContext::make_conda_compatible(mambatests::context());
                auto sprefix_data = PrefixData::create("", channel_context);
                if (!sprefix_data)
                {
                    // TODO: propagate tl::expected mechanism
                    throw std::runtime_error("could not load prefix data");
                }
                PrefixData& prefix_data = sprefix_data.value();
                REQUIRE_EQ(prefix_data.records().size(), 0);

                specs = { "python" };
                pin = python_pin(prefix_data, specs);
                CHECK_EQ(pin, "");

                specs = { "python-test" };
                pin = python_pin(prefix_data, specs);
                CHECK_EQ(pin, "");

                specs = { "python=3" };
                pin = python_pin(prefix_data, specs);
                CHECK_EQ(pin, "");

                specs = { "python==3.8" };
                pin = python_pin(prefix_data, specs);
                CHECK_EQ(pin, "");

                specs = { "python==3.8.3" };
                pin = python_pin(prefix_data, specs);
                CHECK_EQ(pin, "");

                specs = { "numpy" };
                pin = python_pin(prefix_data, specs);
                CHECK_EQ(pin, "");

                specs::PackageInfo pkg_info("python", "3.7.10", "abcde", 0);
                prefix_data.add_packages({ pkg_info });
                REQUIRE_EQ(prefix_data.records().size(), 1);

                specs = { "python" };
                pin = python_pin(prefix_data, specs);
                CHECK_EQ(pin, "");

                specs = { "numpy" };
                pin = python_pin(prefix_data, specs);
                CHECK_EQ(pin, "python 3.7.*");

                specs = { "python-test" };
                pin = python_pin(prefix_data, specs);
                CHECK_EQ(pin, "python 3.7.*");

                specs = { "python==3" };
                pin = python_pin(prefix_data, specs);
                CHECK_EQ(pin, "");

                specs = { "python=3.*" };
                pin = python_pin(prefix_data, specs);
                CHECK_EQ(pin, "");

                specs = { "python=3.8" };
                pin = python_pin(prefix_data, specs);
                CHECK_EQ(pin, "");

                specs = { "python=3.8.3" };
                pin = python_pin(prefix_data, specs);
                CHECK_EQ(pin, "");

                specs = { "numpy", "python" };
                pin = python_pin(prefix_data, specs);
                CHECK_EQ(pin, "");
            }

            TEST_CASE("file_pins")
            {
                std::vector<std::string> pins;

                auto tempfile = std::make_unique<TemporaryFile>("pinned", "");
                const auto path = tempfile->path();
                std::ofstream out_file(path.std_path());
                out_file << "numpy=1.13\njupyterlab=3";
                out_file.close();

                pins = file_pins(path);
                REQUIRE_EQ(pins.size(), 2);
                CHECK_EQ(pins[0], "numpy=1.13");
                CHECK_EQ(pins[1], "jupyterlab=3");

                out_file.open(path.std_path(), std::ofstream::out | std::ofstream::trunc);
                out_file << "numpy=1.13\npython=3.7.5";
                out_file.close();

                pins = file_pins(path);
                REQUIRE_EQ(pins.size(), 2);
                CHECK_EQ(pins[0], "numpy=1.13");
                CHECK_EQ(pins[1], "python=3.7.5");
            }
        }
    }  // namespace testing
}  // namespace mamba
