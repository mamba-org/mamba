// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <fstream>
#include <filesystem>

#include "common_options.hpp"

#include "mamba/api/config.hpp"
#include "mamba/api/configuration.hpp"
#include "mamba/core/fsutil.hpp"

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
set_config_file_command(CLI::App* subcom)
{
    auto& config = Configuration::instance();

    auto& file_path = config.insert(Configurable("config_set_file_path", fs::path())
                                        .group("cli")
                                        .description("File path to set configuration"),
                                    true);
    subcom->add_option("--file", file_path.set_cli_config(fs::path()), file_path.description());
}

void
set_config_prepend_command(CLI::App* subcom)
{
    set_config_file_command(subcom);

    auto& config = Configuration::instance();

    auto& prepend_map = config.insert(
        Configurable("prepend_map", std::vector<std::string>({ "" }))
            .group("Output, Prompt and Flow Control")
            .description("Add one configuration value to the beginning of a list key"));
    subcom->add_option(
        "prepend_map", prepend_map.set_cli_config({ "" }), prepend_map.description());

    auto& file_path = config.at("config_set_file_path").get_wrapped<fs::path>();

    subcom->callback([&]() {
        config.at("use_target_prefix_fallback").set_value(true);
        config.at("show_banner").set_value(false);
        config.at("target_prefix_checks")
            .set_value(MAMBA_ALLOW_EXISTING_PREFIX | MAMBA_ALLOW_MISSING_PREFIX
                       | MAMBA_ALLOW_NOT_ENV_PREFIX | MAMBA_NOT_EXPECT_EXISTING_PREFIX);
        config.load();

        fs::path rc_source = env::expand_user(env::home_directory() / ".condarc");
        std::ofstream rc_file;

        std::string prepend_key = prepend_map.value().front();
        std::string prepend_value = prepend_map.value().back();

        if (file_path.configured())
        {
            path::touch(file_path.value().string(), true);
            rc_source = env::expand_user(file_path.value()).string();
        }

        YAML::Node rc_YAML = YAML::LoadFile(rc_source.string());

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

        if (is_key_valid(prepend_key) && prepend_map.value().size() < 3)
        {
            // prepend value to the end of the chosen config key list
            rc_YAML[prepend_key] = insert_yaml_sequence(rc_YAML[prepend_key], prepend_value);
        }
        else
        {
            std::cout
                << "Prepend key is invalid or it's not present in file or more than one key was received"
                << std::endl;
        }
        // if the rc file is being modified, it's necessary to rewrite it
        rc_file.open(rc_source.string(), std::ofstream::in | std::ofstream::trunc);
        rc_file << rc_YAML << std::endl;

        config.operation_teardown();
    });
}

void
set_config_append_command(CLI::App* subcom)
{
    set_config_file_command(subcom);

    auto& config = Configuration::instance();

    auto& append_map
        = config.insert(Configurable("append_map", std::vector<std::string>({ "" }))
                            .group("Output, Prompt and Flow Control")
                            .description("Add one configuration value to the end of a list"));
    subcom->add_option("append_map", append_map.set_cli_config({ "" }), append_map.description());

    auto& file_path = config.at("config_set_file_path").get_wrapped<fs::path>();

    subcom->callback([&]() {
        config.at("use_target_prefix_fallback").set_value(true);
        config.at("show_banner").set_value(false);
        config.at("target_prefix_checks")
            .set_value(MAMBA_ALLOW_EXISTING_PREFIX | MAMBA_ALLOW_MISSING_PREFIX
                       | MAMBA_ALLOW_NOT_ENV_PREFIX | MAMBA_NOT_EXPECT_EXISTING_PREFIX);
        config.load();

        fs::path rc_source = env::expand_user(env::home_directory() / ".condarc");
        std::ofstream rc_file;

        std::string append_key = append_map.value().front();
        std::string append_value = append_map.value().back();

        if (file_path.configured())
        {
            path::touch(file_path.value().string(), true);
            rc_source = env::expand_user(file_path.value()).string();
        }

        YAML::Node rc_YAML = YAML::LoadFile(rc_source.string());

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

        if (is_key_valid(append_key) && append_map.value().size() < 3)
        {
            // std::cout << "append_value " << append_value << std::endl;
            rc_YAML[append_key].push_back(append_value);
        }
        else
        {
            std::cout
                << "Append key is invalid or it's not present in file or more than one key was received"
                << std::endl;
        }

        // if the rc file is being modified, it's necessary to rewrite it
        rc_file.open(rc_source.string(), std::ofstream::in | std::ofstream::trunc);
        rc_file << rc_YAML << std::endl;

        config.operation_teardown();
    });
}

void
set_config_remove_key_command(CLI::App* subcom)
{
    set_config_file_command(subcom);

    auto& config = Configuration::instance();

    auto& remove_key = config.insert(Configurable("remove_key", std::string(""))
                                         .group("Output, Prompt and Flow Control")
                                         .description("Remove a configuration key and its values"));
    subcom->add_option("remove_key", remove_key.set_cli_config(""), remove_key.description());

    auto& file_path = config.at("config_set_file_path").get_wrapped<fs::path>();

    subcom->callback([&]() {
        config.at("use_target_prefix_fallback").set_value(true);
        config.at("show_banner").set_value(false);
        config.at("target_prefix_checks")
            .set_value(MAMBA_ALLOW_EXISTING_PREFIX | MAMBA_ALLOW_MISSING_PREFIX
                       | MAMBA_ALLOW_NOT_ENV_PREFIX | MAMBA_NOT_EXPECT_EXISTING_PREFIX);
        config.load();

        fs::path rc_source = env::expand_user(env::home_directory() / ".condarc");
        std::ofstream rc_file;
        bool key_removed = false;

        if (file_path.configured())
        {
            path::touch(file_path.value().string(), true);
            rc_source = env::expand_user(file_path.value()).string();
        }

        // convert rc file to YAML::Node
        YAML::Node rc_YAML = YAML::LoadFile(rc_source.string());

        // look for key to remove in file
        for (auto v : rc_YAML)
        {
            if (v.first.as<std::string>() == remove_key.value())
            {
                rc_YAML.remove(remove_key.value());
                key_removed = true;
                break;
            }
        }

        if (!key_removed)
        {
            std::cout << "Key is not present in file" << std::endl;
        }

        // if the rc file is being modified, it's necessary to rewrite it
        rc_file.open(rc_source.string(), std::ofstream::in | std::ofstream::trunc);
        rc_file << rc_YAML << std::endl;

        config.operation_teardown();
    });
}

void
set_config_set_command(CLI::App* subcom)
{
    set_config_file_command(subcom);

    auto& config = Configuration::instance();

    auto& set_value = config.insert(Configurable("set_value", std::vector<std::string>({}))
                                        .group("Output, Prompt and Flow Control")
                                        .description("Set configuration value on rc file"));
    subcom->add_option("set_value", set_value.set_cli_config({}), set_value.description());

    auto& file_path = config.at("config_set_file_path").get_wrapped<fs::path>();

    subcom->callback([&]() {
        config.at("use_target_prefix_fallback").set_value(true);
        config.at("show_banner").set_value(false);
        config.at("target_prefix_checks")
            .set_value(MAMBA_ALLOW_EXISTING_PREFIX | MAMBA_ALLOW_MISSING_PREFIX
                       | MAMBA_ALLOW_NOT_ENV_PREFIX | MAMBA_NOT_EXPECT_EXISTING_PREFIX);
        config.load();

        fs::path rc_source = env::expand_user(env::home_directory() / ".condarc");
        std::ofstream rc_file;

        if (file_path.configured())
        {
            path::touch(file_path.value().string(), true);
            rc_source = env::expand_user(file_path.value().string());
        }

        YAML::Node rc_YAML = YAML::LoadFile(rc_source.string());

        if (is_key_valid(set_value.value().at(0)) && set_value.value().size() < 3)
        {
            rc_YAML[set_value.value().at(0)] = set_value.value().at(1);
        }
        else
        {
            std::cout << "Key is invalid or more than one key was received" << std::endl;
        }

        // if the rc file is being modified, it's necessary to rewrite it
        rc_file.open(rc_source.string(), std::ofstream::in | std::ofstream::trunc);
        rc_file << rc_YAML << std::endl;

        config.operation_teardown();
    });
}

void
set_config_get_command(CLI::App* subcom)
{
    set_config_file_command(subcom);

    auto& config = Configuration::instance();

    // TODO: get_value should be a vector of strings
    auto& get_value = config.insert(Configurable("get_value", std::string(""))
                                        .group("Output, Prompt and Flow Control")
                                        .description("Display configuration value from rc file"));
    subcom->add_option("get_value", get_value.set_cli_config(""), get_value.description());

    auto& file_path = config.at("config_set_file_path").get_wrapped<fs::path>();

    subcom->callback([&]() {
        config.at("use_target_prefix_fallback").set_value(true);
        config.at("show_banner").set_value(false);
        config.at("target_prefix_checks")
            .set_value(MAMBA_ALLOW_EXISTING_PREFIX | MAMBA_ALLOW_MISSING_PREFIX
                       | MAMBA_ALLOW_NOT_ENV_PREFIX | MAMBA_NOT_EXPECT_EXISTING_PREFIX);
        config.load();

        fs::path rc_source = env::expand_user(env::home_directory() / ".condarc");
        std::ofstream rc_file;

        bool value_found = false;

        if (file_path.configured())
        {
            rc_source = env::expand_user(file_path.value()).string();
        }

        YAML::Node rc_YAML = YAML::LoadFile(rc_source.string());

        for (auto v : rc_YAML)
        {
            if (v.first.as<std::string>() == get_value.value())
            {
                YAML::Node aux_rc_YAML;
                aux_rc_YAML[v.first] = v.second;
                std::cout << aux_rc_YAML << std::endl;
                value_found = true;
                break;
            }
        }

        if (!value_found)
        {
            std::cout << "Key is not present in file" << std::endl;
        }

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
