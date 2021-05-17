// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <fstream>
#include <cstdlib>
#include <typeinfo>

#include "common_options.hpp"

#include "mamba/api/config.hpp"
#include "mamba/api/configuration.hpp"

#include <yaml-cpp/yaml.h>

using namespace mamba;  // NOLINT(build/namespaces)

bool
is_key_present(YAML::Node rc_YAML, std::string key)
{
    for (std::size_t j = 0; j < rc_YAML.size(); j++)
    {
        if (rc_YAML[j].as<std::string>() == key)
        {
            return true;
        }
    }
    return true;
}

bool
is_key_valid(std::string key)
{
    auto& config = Configuration::instance();

    for (auto& group_it : config.get_grouped_config())
    {
        auto& configs = group_it.second;
        for (auto c : configs)
        {
            if (key == c->name())
            {
                return true;
            }
        }
    }
    return false;
}

YAML::Node
insert_yaml_sequence(YAML::Node seq, std::string val)
{
    YAML::Node aux;
    aux.push_back(val);
    for (auto v : seq)
    {
        aux.push_back(v);
    }
    return aux;
}

void
init_config_options(CLI::App* subcom)
{
    init_general_options(subcom);
    init_prefix_options(subcom);
}

void
init_config_describe_options(CLI::App* subcom)
{
    auto& config = Configuration::instance();

    auto& specs = config.at("specs").get_wrapped<std::vector<std::string>>();
    subcom->add_option("specs", specs.set_cli_config({}), specs.description());

    auto& show_long_descriptions = config.at("show_config_long_descriptions").get_wrapped<bool>();
    subcom->add_flag("-l,--long-descriptions",
                     show_long_descriptions.set_cli_config(0),
                     show_long_descriptions.description());

    auto& show_groups = config.at("show_config_groups").get_wrapped<bool>();
    subcom->add_flag("-g,--groups", show_groups.set_cli_config(0), show_groups.description());
}

void
init_config_list_options(CLI::App* subcom)
{
    init_config_options(subcom);
    init_config_describe_options(subcom);

    auto& config = Configuration::instance();

    auto& show_sources = config.at("show_config_sources").get_wrapped<bool>();
    subcom->add_flag("-s,--sources", show_sources.set_cli_config(0), show_sources.description());

    auto& show_all = config.at("show_all_configs").get_wrapped<bool>();
    subcom->add_flag("-a,--all", show_all.set_cli_config(0), show_all.description());

    auto& show_descriptions = config.at("show_config_descriptions").get_wrapped<bool>();
    subcom->add_flag(
        "-d,--descriptions", show_descriptions.set_cli_config(0), show_descriptions.description());
}

void
set_config_list_command(CLI::App* subcom)
{
    init_config_list_options(subcom);

    subcom->callback([&]() {
        config_list();
        return 0;
    });
}

void
set_config_sources_command(CLI::App* subcom)
{
    init_config_options(subcom);

    subcom->callback([&]() {
        config_sources();
        return 0;
    });
}

void
set_config_describe_command(CLI::App* subcom)
{
    init_config_describe_options(subcom);

    subcom->callback([&]() {
        config_describe();
        return 0;
    });
}

void
set_config_prepend_command(CLI::App* subcom)
{
    auto& config = Configuration::instance();

    auto& prepend_map = config.insert(
        Configurable("prepend_map", std::vector<std::string>({ "" }))
            .group("Output, Prompt and Flow Control")
            .description("Add one configuration value to the beginning of a list key"));

    subcom->add_option(
        "prepend_map", prepend_map.set_cli_config({ "" }), prepend_map.description());

    subcom->callback([&]() {
        config.at("use_target_prefix_fallback").set_value(true);
        config.at("show_banner").set_value(false);
        config.at("target_prefix_checks")
            .set_value(MAMBA_ALLOW_EXISTING_PREFIX | MAMBA_ALLOW_MISSING_PREFIX
                       | MAMBA_ALLOW_NOT_ENV_PREFIX | MAMBA_NOT_EXPECT_EXISTING_PREFIX);
        config.load();

        auto valid_srcs = config.valid_sources();
        std::ofstream rc_file;

        std::string prepend_key = prepend_map.value().front();
        std::string prepend_value = prepend_map.value().back();

        for (auto s : valid_srcs)
        {
            // convert rc file to YAML::Node
            YAML::Node rc_YAML = YAML::LoadFile(env::expand_user(s).string());

            // look for append key in YAML
            for (auto v : rc_YAML)
            {
                if (v.first.as<std::string>() == prepend_key)
                {
                    // if key was found, look for the same value
                    for (std::size_t j = 0; j < rc_YAML[prepend_key].size(); j++)
                    {
                        if (rc_YAML[prepend_key][j].as<std::string>() == prepend_value)
                        {
                            // if value exists, remove it so it can be prepended to end of the list
                            // later on
                            rc_YAML[prepend_key].remove(j);
                            break;
                        }
                    }
                }
            }

            if (is_key_valid(prepend_key))
            {
                // prepend value to the end of the chosen config key list
                rc_YAML[prepend_key] = insert_yaml_sequence(rc_YAML[prepend_key], prepend_value);
            }
            else
            {
                std::cout << "Prepend key is invalid or is not present in the file" << std::endl;
            }
            // if the rc file is being modified, it's necessary to rewrite it
            rc_file.open(env::expand_user(s).string(), std::ofstream::in | std::ofstream::trunc);
            rc_file << rc_YAML << std::endl;
        }

        if (valid_srcs.empty())
        {
            std::cout << "No valid sources were found to apply these changes" << std::endl;
        }
        config.operation_teardown();
    });
}

void
set_config_append_command(CLI::App* subcom)
{
    auto& config = Configuration::instance();

    auto& append_map
        = config.insert(Configurable("append_map", std::vector<std::string>({ "" }))
                            .group("Output, Prompt and Flow Control")
                            .description("Add one configuration value to the end of a list key"));

    subcom->add_option("append_map", append_map.set_cli_config({ "" }), append_map.description());

    subcom->callback([&]() {
        config.at("use_target_prefix_fallback").set_value(true);
        config.at("show_banner").set_value(false);
        config.at("target_prefix_checks")
            .set_value(MAMBA_ALLOW_EXISTING_PREFIX | MAMBA_ALLOW_MISSING_PREFIX
                       | MAMBA_ALLOW_NOT_ENV_PREFIX | MAMBA_NOT_EXPECT_EXISTING_PREFIX);
        config.load();

        auto valid_srcs = config.valid_sources();
        std::ofstream rc_file;

        std::string append_key = append_map.value().front();
        std::string append_value = append_map.value().back();

        for (auto s : valid_srcs)
        {
            // convert rc file to YAML::Node
            YAML::Node rc_YAML = YAML::LoadFile(env::expand_user(s).string());

            // look for append key in YAML
            for (auto v : rc_YAML)
            {
                if (v.first.as<std::string>() == append_key)
                {
                    // if key was found, look for the same value
                    for (std::size_t j = 0; j < rc_YAML[append_key].size(); j++)
                    {
                        if (rc_YAML[append_key][j].as<std::string>() == append_value)
                        {
                            // if value exists, remove it so it can be appended to end of the list
                            // later on
                            rc_YAML[append_key].remove(j);
                            break;
                        }
                    }
                }
            }

            if (is_key_valid(append_key))
            {
                rc_YAML[append_key].push_back(append_value);
            }
            else
            {
                std::cout << "Append key is invalid or is not present in the file" << std::endl;
            }

            // if the rc file is being modified, it's necessary to rewrite it
            rc_file.open(env::expand_user(s).string(), std::ofstream::in | std::ofstream::trunc);
            rc_file << rc_YAML << std::endl;
        }
        if (valid_srcs.empty())
        {
            std::cout << "No valid sources were found to apply these changes" << std::endl;
        }
        config.operation_teardown();
    });
}

void
set_config_remove_key_command(CLI::App* subcom)
{
    auto& config = Configuration::instance();

    auto& remove_key = config.insert(Configurable("remove_key", std::string(""))
                                         .group("Output, Prompt and Flow Control")
                                         .description("Remove a configuration key and its values"));

    subcom->add_option("remove_key", remove_key.set_cli_config(""), remove_key.description());

    subcom->callback([&]() {
        config.at("use_target_prefix_fallback").set_value(true);
        config.at("show_banner").set_value(false);
        config.at("target_prefix_checks")
            .set_value(MAMBA_ALLOW_EXISTING_PREFIX | MAMBA_ALLOW_MISSING_PREFIX
                       | MAMBA_ALLOW_NOT_ENV_PREFIX | MAMBA_NOT_EXPECT_EXISTING_PREFIX);
        config.load();

        auto valid_srcs = config.valid_sources();
        std::ofstream rc_file;

        for (auto s : valid_srcs)
        {
            // convert rc file to YAML::Node
            YAML::Node rc_YAML = YAML::LoadFile(env::expand_user(s).string());

            // look for key to remove in file
            for (auto v : rc_YAML)
            {
                if (v.first.as<std::string>() == remove_key.value())
                {
                    rc_YAML.remove(remove_key.value());
                    break;
                }
            }

            // if the rc file is being modified, it's necessary to rewrite it
            rc_file.open(env::expand_user(s).string(), std::ofstream::in | std::ofstream::trunc);
            rc_file << rc_YAML << std::endl;
        }
        if (valid_srcs.empty())
        {
            std::cout << "No valid sources were found to apply these changes" << std::endl;
        }
        config.operation_teardown();
    });
}

void
set_config_set_command(CLI::App* subcom)
{
    auto& config = Configuration::instance();

    auto& set_key = config.insert(Configurable("set_key", std::vector<std::string>({}))
                                      .group("Output, Prompt and Flow Control")
                                      .description("Set configuration value on rc file"));

    subcom->add_option("set_key", set_key.set_cli_config({}), set_key.description());

    subcom->callback([&]() {
        config.at("use_target_prefix_fallback").set_value(true);
        config.at("show_banner").set_value(false);
        config.at("target_prefix_checks")
            .set_value(MAMBA_ALLOW_EXISTING_PREFIX | MAMBA_ALLOW_MISSING_PREFIX
                       | MAMBA_ALLOW_NOT_ENV_PREFIX | MAMBA_NOT_EXPECT_EXISTING_PREFIX);
        config.load();

        auto valid_srcs = config.valid_sources();
        std::ofstream rc_file;

        for (auto s : valid_srcs)
        {
            YAML::Node rc_YAML = YAML::LoadFile(env::expand_user(s).string());
            if (is_key_valid(set_key.value()[0]))
            {
                rc_YAML[set_key.value()[0]] = set_key.value()[1];
            }
            // if the rc file is being modified, it's necessary to rewrite it
            rc_file.open(env::expand_user(s).string(), std::ofstream::in | std::ofstream::trunc);
            rc_file << rc_YAML << std::endl;
        }
        if (valid_srcs.empty())
        {
            std::cout << "No valid sources were found to apply these changes" << std::endl;
        }
        config.operation_teardown();
    });
}

void
set_config_get_command(CLI::App* subcom)
{
    auto& config = Configuration::instance();

    auto& get_key = config.insert(Configurable("get_key", std::string(""))
                                      .group("Output, Prompt and Flow Control")
                                      .description("Display configuration value from rc file"));

    subcom->add_option("get_key", get_key.set_cli_config(""), get_key.description());

    subcom->callback([&]() {
        config.at("use_target_prefix_fallback").set_value(true);
        config.at("show_banner").set_value(false);
        config.at("target_prefix_checks")
            .set_value(MAMBA_ALLOW_EXISTING_PREFIX | MAMBA_ALLOW_MISSING_PREFIX
                       | MAMBA_ALLOW_NOT_ENV_PREFIX | MAMBA_NOT_EXPECT_EXISTING_PREFIX);
        config.load();

        std::cout << config.dump(MAMBA_SHOW_CONFIG_VALUES, { get_key.value() }) << std::endl;

        config.operation_teardown();
    });
}

void
set_config_command(CLI::App* subcom)
{
    init_config_options(subcom);

    auto list_subcom = subcom->add_subcommand("list", "List configuration values");
    set_config_list_command(list_subcom);

    auto sources_subcom = subcom->add_subcommand("sources", "Show configuration sources");
    set_config_sources_command(sources_subcom);

    auto describe_subcom
        = subcom->add_subcommand("describe", "Describe given configuration parameters");
    set_config_describe_command(describe_subcom);

    auto prepend_subcom = subcom->add_subcommand(
        "prepend", "Add one configuration value to the beginning of a list key");
    set_config_prepend_command(prepend_subcom);

    auto append_subcom
        = subcom->add_subcommand("append", "Add one configuration value to the end of a list key");
    set_config_append_command(append_subcom);

    auto remove_key_subcom
        = subcom->add_subcommand("remove-key", "Remove a configuration key and its values");
    set_config_remove_key_command(remove_key_subcom);

    auto set_subcom = subcom->add_subcommand("set", "Set a configuration value");
    set_config_set_command(set_subcom);

    auto get_subcom = subcom->add_subcommand("get", "Get a configuration value");
    set_config_get_command(get_subcom);
}
