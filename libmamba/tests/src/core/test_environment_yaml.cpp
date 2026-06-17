// Copyright (c) 2026, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <fstream>
#include <map>
#include <vector>

#include <catch2/catch_all.hpp>

#include "mamba/api/environment_yaml.hpp"
#include "mamba/api/install.hpp"
#include "mamba/core/channel_context.hpp"
#include "mamba/core/context.hpp"
#include "mamba/core/prefix_data.hpp"
#include "mamba/core/util.hpp"
#include "mamba/specs/package_info.hpp"

#include "mambatests.hpp"

namespace mamba
{
    namespace
    {
        TEST_CASE("file_to_yaml_contents: basic reading", "[api][environment-yaml]")
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
            REQUIRE(res.others_pkg_mgrs_specs.empty());
            REQUIRE(res.variables.empty());
        }

        TEST_CASE("file_to_yaml_contents: with pip dependencies", "[api][environment-yaml]")
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
            REQUIRE(res.others_pkg_mgrs_specs[0].pkg_mgr == "pip");
            REQUIRE(res.others_pkg_mgrs_specs[0].deps == V({ "pytest", "numpy" }));
        }

        TEST_CASE("file_to_yaml_contents: with variables", "[api][environment-yaml]")
        {
            const auto& context = mambatests::context();
            auto tmp_dir = TemporaryDirectory();
            auto yaml_file = tmp_dir.path() / "test_env.yaml";

            {
                std::ofstream out(yaml_file.string());
                out << "name: test_env\n";
                out << "channels:\n";
                out << "  - conda-forge\n";
                out << "dependencies:\n";
                out << "  - python=3.10\n";
                out << "variables:\n";
                out << "  test_var: test_value\n";
                out << "  another_var: another_value\n";
            }

            auto res = file_to_yaml_contents(context, yaml_file.string(), context.platform, false);

            REQUIRE(res.name == "test_env");
            REQUIRE(res.variables.size() == 2);
            REQUIRE(res.variables.at("test_var") == "test_value");
            REQUIRE(res.variables.at("another_var") == "another_value");
        }

        TEST_CASE("yaml_contents_to_file: basic writing", "[api][environment-yaml]")
        {
            auto tmp_dir = TemporaryDirectory();
            auto yaml_file = tmp_dir.path() / "output.yaml";

            detail::yaml_file_contents contents;
            contents.name = "test_env";
            contents.channels = { "conda-forge", "bioconda" };
            contents.dependencies = { "python=3.10", "numpy" };

            yaml_contents_to_file(contents, yaml_file);

            REQUIRE(fs::exists(yaml_file));

            // Read it back and verify
            const auto& context = mambatests::context();
            auto read_back = file_to_yaml_contents(context, yaml_file.string(), context.platform, false);

            REQUIRE(read_back.name == contents.name);
            REQUIRE(read_back.channels == contents.channels);
            REQUIRE(read_back.dependencies == contents.dependencies);
        }

        TEST_CASE("yaml_contents_to_file: with pip dependencies", "[api][environment-yaml]")
        {
            auto tmp_dir = TemporaryDirectory();
            auto yaml_file = tmp_dir.path() / "output.yaml";

            detail::yaml_file_contents contents;
            contents.name = "test_env";
            contents.channels = { "conda-forge" };
            contents.dependencies = { "python=3.10", "pip" };
            contents.others_pkg_mgrs_specs.push_back(
                { "pip", { "pytest", "numpy" }, tmp_dir.path().string() }
            );

            yaml_contents_to_file(contents, yaml_file);

            REQUIRE(fs::exists(yaml_file));

            // Read it back and verify
            const auto& context = mambatests::context();
            auto read_back = file_to_yaml_contents(context, yaml_file.string(), context.platform, false);

            REQUIRE(read_back.name == contents.name);
            REQUIRE(read_back.channels == contents.channels);
            REQUIRE(read_back.dependencies == contents.dependencies);
            REQUIRE(read_back.others_pkg_mgrs_specs.size() == 1);
            REQUIRE(read_back.others_pkg_mgrs_specs[0].pkg_mgr == "pip");
            REQUIRE(read_back.others_pkg_mgrs_specs[0].deps == contents.others_pkg_mgrs_specs[0].deps);
        }

        TEST_CASE("yaml_contents_to_file: with variables", "[api][environment-yaml]")
        {
            auto tmp_dir = TemporaryDirectory();
            auto yaml_file = tmp_dir.path() / "output.yaml";

            detail::yaml_file_contents contents;
            contents.name = "test_env";
            contents.channels = { "conda-forge" };
            contents.dependencies = { "python=3.10" };
            contents.variables = { { "test_var", "test_value" }, { "another_var", "another_value" } };

            yaml_contents_to_file(contents, yaml_file);

            REQUIRE(fs::exists(yaml_file));

            // Read it back and verify
            const auto& context = mambatests::context();
            auto read_back = file_to_yaml_contents(context, yaml_file.string(), context.platform, false);

            REQUIRE(read_back.variables.size() == 2);
            REQUIRE(read_back.variables.at("test_var") == "test_value");
            REQUIRE(read_back.variables.at("another_var") == "another_value");
        }

        TEST_CASE("prefix_to_yaml_contents: basic conversion", "[api][environment-yaml]")
        {
            auto tmp_dir = TemporaryDirectory();
            auto prefix_path = tmp_dir.path() / "prefix";
            fs::create_directories(prefix_path);

            auto& ctx = mambatests::context();
            auto channel_context = ChannelContext::make_simple(ctx);

            // Create a minimal conda environment structure
            auto conda_meta_dir = prefix_path / "conda-meta";
            fs::create_directories(conda_meta_dir);

            // Create package records
            auto python_pkg_json = conda_meta_dir / "python-3.10.0-h12345_0.json";
            {
                auto out = open_ofstream(python_pkg_json);
                out << R"({
                "name": "python",
                "version": "3.10.0",
                "build_string": "h12345_0",
                "build_number": 0,
                "channel": "conda-forge",
                "platform": "linux-64",
                "package_url": "file:///path/to/python-3.10.0-h12345_0.tar.bz2"
            })";
            }

            auto numpy_pkg_json = conda_meta_dir / "numpy-1.24.0-py310h12345_0.json";
            {
                auto out = open_ofstream(numpy_pkg_json);
                out << R"({
                "name": "numpy",
                "version": "1.24.0",
                "build_string": "py310h12345_0",
                "build_number": 0,
                "channel": "conda-forge",
                "platform": "linux-64",
                "package_url": "file:///path/to/numpy-1.24.0-py310h12345_0.tar.bz2"
            })";
            }

            auto prefix_data = PrefixData::create(prefix_path, channel_context, true).value();

            auto yaml_contents = prefix_to_yaml_contents(prefix_data, ctx, "test_env");

            REQUIRE(yaml_contents.name == "test_env");
            REQUIRE(yaml_contents.channels.size() == 1);
            REQUIRE(yaml_contents.channels[0] == "conda-forge");
            REQUIRE(yaml_contents.dependencies.size() == 2);
            // Dependencies should contain python and numpy with channel prefix
            bool has_python = false;
            bool has_numpy = false;
            for (const auto& dep : yaml_contents.dependencies)
            {
                if (dep.find("python") != std::string::npos && dep.find("3.10.0") != std::string::npos)
                {
                    has_python = true;
                }
                if (dep.find("numpy") != std::string::npos && dep.find("1.24.0") != std::string::npos)
                {
                    has_numpy = true;
                }
            }
            REQUIRE(has_python);
            REQUIRE(has_numpy);
        }

        TEST_CASE("prefix_to_yaml_contents: with environment variables", "[api][environment-yaml]")
        {
            auto tmp_dir = TemporaryDirectory();
            auto prefix_path = tmp_dir.path() / "prefix";
            fs::create_directories(prefix_path);

            auto& ctx = mambatests::context();
            auto channel_context = ChannelContext::make_simple(ctx);

            // Create conda-meta directory
            auto conda_meta_dir = prefix_path / "conda-meta";
            fs::create_directories(conda_meta_dir);

            // Create state file with environment variables (UPPERCASE keys)
            auto state_file = conda_meta_dir / "state";
            {
                auto out = open_ofstream(state_file);
                out << R"({
                "env_vars": {
                    "TEST_VAR": "test_value",
                    "ANOTHER_VAR": "another_value"
                }
            })";
            }

            auto prefix_data = PrefixData::create(prefix_path, channel_context, true).value();

            auto yaml_contents = prefix_to_yaml_contents(prefix_data, ctx, "");

            // Variables should be converted to lowercase
            REQUIRE(yaml_contents.variables.size() == 2);
            REQUIRE(yaml_contents.variables.at("test_var") == "test_value");
            REQUIRE(yaml_contents.variables.at("another_var") == "another_value");
        }

        TEST_CASE("prefix_to_yaml_contents: no_builds option", "[api][environment-yaml]")
        {
            auto tmp_dir = TemporaryDirectory();
            auto prefix_path = tmp_dir.path() / "prefix";
            fs::create_directories(prefix_path);

            auto& ctx = mambatests::context();
            auto channel_context = ChannelContext::make_simple(ctx);

            auto conda_meta_dir = prefix_path / "conda-meta";
            fs::create_directories(conda_meta_dir);

            auto python_pkg_json = conda_meta_dir / "python-3.10.0-h12345_0.json";
            {
                auto out = open_ofstream(python_pkg_json);
                out << R"({
                "name": "python",
                "version": "3.10.0",
                "build_string": "h12345_0",
                "build_number": 0,
                "channel": "conda-forge",
                "platform": "linux-64",
                "package_url": "file:///path/to/python-3.10.0-h12345_0.tar.bz2"
            })";
            }

            auto prefix_data = PrefixData::create(prefix_path, channel_context, true).value();

            auto yaml_contents = prefix_to_yaml_contents(prefix_data, ctx, "", { .no_builds = true });

            // With no_builds=true, build string should not be included
            REQUIRE(yaml_contents.dependencies.size() == 1);
            REQUIRE(
                std::find(
                    yaml_contents.dependencies.begin(),
                    yaml_contents.dependencies.end(),
                    "conda-forge::python=3.10.0"
                )
                != yaml_contents.dependencies.end()
            );
        }

        TEST_CASE("prefix_to_yaml_contents: ignore_channels option", "[api][environment-yaml]")
        {
            auto tmp_dir = TemporaryDirectory();
            auto prefix_path = tmp_dir.path() / "prefix";
            fs::create_directories(prefix_path);

            auto& ctx = mambatests::context();
            auto channel_context = ChannelContext::make_simple(ctx);

            auto conda_meta_dir = prefix_path / "conda-meta";
            fs::create_directories(conda_meta_dir);

            auto python_pkg_json = conda_meta_dir / "python-3.10.0-h12345_0.json";
            {
                auto out = open_ofstream(python_pkg_json);
                out << R"({
                "name": "python",
                "version": "3.10.0",
                "build_string": "h12345_0",
                "build_number": 0,
                "channel": "conda-forge",
                "platform": "linux-64",
                "package_url": "file:///path/to/python-3.10.0-h12345_0.tar.bz2"
            })";
            }

            auto prefix_data = PrefixData::create(prefix_path, channel_context, true).value();

            auto yaml_contents = prefix_to_yaml_contents(
                prefix_data,
                ctx,
                "",
                { .ignore_channels = true }
            );

            // With ignore_channels=true, channels should be empty and dependencies should not have
            // channel prefix
            REQUIRE(yaml_contents.channels.empty());
            REQUIRE(yaml_contents.dependencies.size() == 1);
            bool has_python_no_channel = false;
            for (const auto& dep : yaml_contents.dependencies)
            {
                if (dep.find("python") != std::string::npos && dep.find("3.10.0") != std::string::npos
                    && dep.find("conda-forge::") == std::string::npos)
                {
                    has_python_no_channel = true;
                }
            }
            REQUIRE(has_python_no_channel);
        }

        TEST_CASE("round_trip: yaml_file_contents to file and back", "[api][environment-yaml]")
        {
            auto tmp_dir = TemporaryDirectory();
            auto yaml_file = tmp_dir.path() / "roundtrip.yaml";

            detail::yaml_file_contents original;
            original.name = "roundtrip_test";
            original.channels = { "conda-forge", "bioconda" };
            // Include "pip" in dependencies when pip dependencies exist (standard conda format)
            original.dependencies = { "python=3.10", "numpy", "pandas", "pip" };
            original.variables = { { "var1", "value1" }, { "var2", "value2" } };
            original.others_pkg_mgrs_specs.push_back(
                { "pip", { "pytest", "black" }, tmp_dir.path().string() }
            );

            // Write to file
            yaml_contents_to_file(original, yaml_file);

            // Read back
            const auto& context = mambatests::context();
            auto read_back = file_to_yaml_contents(context, yaml_file.string(), context.platform, false);

            // Verify all fields match
            REQUIRE(read_back.name == original.name);
            REQUIRE(read_back.channels == original.channels);
            REQUIRE(read_back.dependencies == original.dependencies);
            REQUIRE(read_back.variables == original.variables);
            REQUIRE(read_back.others_pkg_mgrs_specs.size() == original.others_pkg_mgrs_specs.size());
            if (!read_back.others_pkg_mgrs_specs.empty())
            {
                REQUIRE(
                    read_back.others_pkg_mgrs_specs[0].pkg_mgr
                    == original.others_pkg_mgrs_specs[0].pkg_mgr
                );
                REQUIRE(
                    read_back.others_pkg_mgrs_specs[0].deps == original.others_pkg_mgrs_specs[0].deps
                );
            }
        }

        TEST_CASE("environment_variables: case conversion round-trip", "[api][environment-yaml]")
        {
            auto tmp_dir = TemporaryDirectory();
            auto prefix_path = tmp_dir.path() / "prefix";
            fs::create_directories(prefix_path);

            auto& ctx = mambatests::context();
            auto channel_context = ChannelContext::make_simple(ctx);

            // Create conda-meta directory
            auto conda_meta_dir = prefix_path / "conda-meta";
            fs::create_directories(conda_meta_dir);

            // Create state file with UPPERCASE keys
            auto state_file = conda_meta_dir / "state";
            {
                auto out = open_ofstream(state_file);
                out << R"({
                "env_vars": {
                    "MY_VAR": "my_value",
                    "ANOTHER_VAR": "another_value"
                }
            })";
            }

            auto prefix_data = PrefixData::create(prefix_path, channel_context, true).value();

            // Export: prefix → yaml_file_contents (should convert UPPERCASE to lowercase)
            auto yaml_contents = prefix_to_yaml_contents(prefix_data, ctx, "");
            REQUIRE(yaml_contents.variables.at("my_var") == "my_value");
            REQUIRE(yaml_contents.variables.at("another_var") == "another_value");

            // Write to YAML file
            auto yaml_file = tmp_dir.path() / "env.yaml";
            yaml_contents_to_file(yaml_contents, yaml_file);

            // Read back from YAML (should have lowercase keys)
            auto read_back = file_to_yaml_contents(ctx, yaml_file.string(), ctx.platform, false);
            REQUIRE(read_back.variables.at("my_var") == "my_value");
            REQUIRE(read_back.variables.at("another_var") == "another_value");
        }

        TEST_CASE("environment_variables: missing state file", "[api][environment-yaml]")
        {
            auto tmp_dir = TemporaryDirectory();
            auto prefix_path = tmp_dir.path() / "prefix";
            fs::create_directories(prefix_path);

            auto& ctx = mambatests::context();
            auto channel_context = ChannelContext::make_simple(ctx);

            // Create conda-meta directory but no state file
            auto conda_meta_dir = prefix_path / "conda-meta";
            fs::create_directories(conda_meta_dir);

            auto prefix_data = PrefixData::create(prefix_path, channel_context, true).value();

            // Should not crash, should return empty variables
            auto yaml_contents = prefix_to_yaml_contents(prefix_data, ctx, "");
            REQUIRE(yaml_contents.variables.empty());
        }

        TEST_CASE("environment_variables: empty variables section", "[api][environment-yaml]")
        {
            auto tmp_dir = TemporaryDirectory();
            auto yaml_file = tmp_dir.path() / "test.yaml";

            detail::yaml_file_contents contents;
            contents.name = "test";
            contents.dependencies = { "python" };
            // No variables set

            yaml_contents_to_file(contents, yaml_file);

            const auto& context = mambatests::context();
            auto read_back = file_to_yaml_contents(context, yaml_file.string(), context.platform, false);

            REQUIRE(read_back.variables.empty());
        }
    }
}
