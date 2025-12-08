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
                std::vector<std::string> pins;

                auto channel_context = ChannelContext::make_conda_compatible(mambatests::context());
                auto sprefix_data = PrefixData::create("", channel_context);
                if (!sprefix_data)
                {
                    // TODO: propagate tl::expected mechanism
                    throw std::runtime_error("could not load prefix data");
                }
                PrefixData& prefix_data = sprefix_data.value();
                REQUIRE(prefix_data.records().size() == 0);

                specs = { "python" };
                pins = python_pin(prefix_data, specs);
                REQUIRE(pins.empty());

                specs = { "python-test" };
                pins = python_pin(prefix_data, specs);
                REQUIRE(pins.empty());

                specs = { "python=3" };
                pins = python_pin(prefix_data, specs);
                REQUIRE(pins.empty());

                specs = { "python==3.8" };
                pins = python_pin(prefix_data, specs);
                REQUIRE(pins.empty());

                specs = { "python==3.8.3" };
                pins = python_pin(prefix_data, specs);
                REQUIRE(pins.empty());

                specs = { "numpy" };
                pins = python_pin(prefix_data, specs);
                REQUIRE(pins.empty());

                specs::PackageInfo pkg_info("python", "3.7.10", "abcde", 0);
                prefix_data.add_packages({ pkg_info });
                REQUIRE(prefix_data.records().size() == 1);

                specs = { "python" };
                pins = python_pin(prefix_data, specs);
                REQUIRE(pins.empty());

                specs = { "numpy" };
                pins = python_pin(prefix_data, specs);
                REQUIRE(pins.size() == 1);
                REQUIRE(pins[0] == "python 3.7.*");

                specs = { "python-test" };
                pins = python_pin(prefix_data, specs);
                REQUIRE(pins.size() == 1);
                REQUIRE(pins[0] == "python 3.7.*");

                specs = { "python==3" };
                pins = python_pin(prefix_data, specs);
                REQUIRE(pins.empty());

                specs = { "python=3.*" };
                pins = python_pin(prefix_data, specs);
                REQUIRE(pins.empty());

                specs = { "python=3.8" };
                pins = python_pin(prefix_data, specs);
                REQUIRE(pins.empty());

                specs = { "python=3.8.3" };
                pins = python_pin(prefix_data, specs);
                REQUIRE(pins.empty());

                specs = { "numpy", "python" };
                pins = python_pin(prefix_data, specs);
                REQUIRE(pins.empty());
            }

            TEST_CASE("python_pin_with_freethreading")
            {
                std::vector<std::string> specs;
                std::vector<std::string> pins;

                auto channel_context = ChannelContext::make_conda_compatible(mambatests::context());
                auto sprefix_data = PrefixData::create("", channel_context);
                if (!sprefix_data)
                {
                    throw std::runtime_error("could not load prefix data");
                }
                PrefixData& prefix_data = sprefix_data.value();

                // Add python 3.14
                specs::PackageInfo python_pkg("python", "3.14.0", "abcde", 0);
                prefix_data.add_packages({ python_pkg });

                // Add python-freethreading
                specs::PackageInfo freethreading_pkg("python-freethreading", "3.14.0", "abcde", 0);
                prefix_data.add_packages({ freethreading_pkg });

                // Add python_abi with free-threaded build string
                specs::PackageInfo python_abi_pkg("python_abi", "3.14", "8_cp314t", 0);
                prefix_data.add_packages({ python_abi_pkg });

                REQUIRE(prefix_data.records().size() == 3);

                // When installing a package (not python), should pin both python and python_abi
                specs = { "numpy" };
                pins = python_pin(prefix_data, specs);
                REQUIRE(pins.size() == 2);
                // Parse expected pins to get canonical format for comparison
                auto expected_py_pin = specs::MatchSpec::parse("python 3.14.*").value();
                auto expected_python_abi_pin = specs::MatchSpec::parse(
                                                   "python_abi[version=\"=3.14\",build=\"*_cp314t\"]"
                )
                                                   .value();
                REQUIRE(pins[0] == expected_py_pin.conda_build_form());
                REQUIRE(pins[1] == expected_python_abi_pin.to_string());

                // When installing python explicitly, should not pin
                specs = { "python" };
                pins = python_pin(prefix_data, specs);
                REQUIRE(pins.empty());
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
                REQUIRE(pins.size() == 2);
                REQUIRE(pins[0] == "numpy=1.13");
                REQUIRE(pins[1] == "jupyterlab=3");

                out_file.open(path.std_path(), std::ofstream::out | std::ofstream::trunc);
                out_file << "numpy=1.13\npython=3.7.5";
                out_file.close();

                pins = file_pins(path);
                REQUIRE(pins.size() == 2);
                REQUIRE(pins[0] == "numpy=1.13");
                REQUIRE(pins[1] == "python=3.7.5");
            }
        }
    }  // namespace testing
}  // namespace mamba
