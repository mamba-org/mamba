// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <fstream>

#include "common_options.hpp"

#include "mamba/api/config.hpp"
#include "mamba/api/configuration.hpp"
#include "mamba/core/fsutil.hpp"

#include <yaml-cpp/yaml.h>

using namespace mamba;  // NOLINT(build/namespaces)

bool
is_valid_rc_key(const std::string& key)
{
    auto& config = Configuration::instance();
    try
    {
        return config.config().at(key).rc_configurable();
    }
    catch (const std::out_of_range& e)
    {
        return false;
    }
}

bool
is_valid_rc_sequence(const std::string& key, const std::string& value)
{
    auto& config = Configuration::instance();
    try
    {
        auto& c = config.config().at(key);
        return c.is_valid_serialization(value) && c.rc_configurable() && c.is_sequence();
    }
    catch (const std::out_of_range& e)
    {
        return false;
    }
}

void
system_path_setup(fs::path& rc_source)
{
    std::string path = "";

    if (on_mac || on_linux)
    {
        path = fs::path("/etc/conda/.condarc");
    }
    else
    {
        path = fs::path("C:\\ProgramData\\conda\\.condarc");
    }
    path::touch(path, true);
    rc_source = env::expand_user(path).string();
}

bool
system_path_exists()
{
    if (fs::exists(fs::path("C:\\ProgramData\\conda\\.condarc"))
        || fs::exists(fs::path("/etc/conda/.condarc")))
    {
        return true;
    }
    std::cout << "File doesn't exist or is not valid." << std::endl;
    return false;
}

void
env_path_setup(fs::path path, fs::path& rc_source)
{
    path::touch(path, true);
    rc_source = env::expand_user(path).string();
}

bool
env_path_exists(fs::path path)
{
    if (fs::exists(path))
    {
        return true;
    }
    std::cout << "File doesn't exist or it's not valid." << std::endl;
    return false;
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
    subcom->add_option("configs", specs.set_cli_config({}), "Configuration keys");

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

    auto& show_all = config.at("show_all_rc_configs").get_wrapped<bool>();
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
set_config_path_command(CLI::App* subcom)
{
    auto& config = Configuration::instance();

    auto& system_path = config.insert(Configurable("config_set_system_path", false)
                                          .group("cli")
                                          .description("Set configuration on system's rc file"),
                                      true);
    auto* system_flag
        = subcom->add_flag("--system", system_path.set_cli_config(0), system_path.description());

    auto& env_path = config.insert(Configurable("config_set_env_path", false)
                                       .group("cli")
                                       .description("Set configuration on env's rc file"),
                                   true);
    auto* env_flag = subcom->add_flag("--env", env_path.set_cli_config(0), env_path.description())
                         ->excludes(system_flag);

    auto& file_path = config.insert(Configurable("config_set_file_path", fs::path())
                                        .group("cli")
                                        .description("Set configuration on system's rc file"),
                                    true);
    subcom->add_option("--file", file_path.set_cli_config(fs::path()), file_path.description())
        ->excludes(system_flag)
        ->excludes(env_flag);
}


enum class SequenceAddType
{
    kAppend = 0,
    kPrepend = 1
};

void
set_config_sequence_command(CLI::App* subcom)
{
    set_config_path_command(subcom);

    auto& config = Configuration::instance();
    auto& specs = config.insert(Configurable("config_set_sequence_spec",
                                             std::vector<std::pair<std::string, std::string>>({}))
                                    .group("Output, Prompt and Flow Control")
                                    .description("Add value to a configurable sequence"),
                                true);
    subcom->add_option("specs", specs.set_cli_config({}), specs.description())->required();
}

void
set_sequence_to_yaml(YAML::Node& node,
                     const std::string& key,
                     const std::string& value,
                     const SequenceAddType& opt)
{
    if (!is_valid_rc_sequence(key, value))
    {
        if (!is_valid_rc_key(key))
            LOG_ERROR << "Invalid key '" << key << "' or not rc configurable";
        else
            LOG_ERROR << "Invalid sequence key";
        throw std::runtime_error("Aborting.");
    }

    auto values = detail::Source<std::vector<std::string>>::deserialize(value);

    // remove any already defined value to respect precedence order
    if (node[key])
    {
        auto existing_values = node[key].as<std::vector<std::string>>();
        for (auto& v : values)
        {
            auto pos = existing_values.end();
            while (existing_values.begin() <= --pos)
            {
                if (*pos == v)
                    existing_values.erase(pos);
            }
        }

        if (opt == SequenceAddType::kAppend)
            existing_values.insert(existing_values.end(), values.begin(), values.end());
        else
            existing_values.insert(existing_values.begin(), values.begin(), values.end());

        node[key] = existing_values;
    }
    else
    {
        node[key] = values;
    }
}

void
set_sequence_to_rc(const SequenceAddType& opt)
{
    auto& config = Configuration::instance();
    auto& ctx = Context::instance();

    config.at("use_target_prefix_fallback").set_value(true);
    config.at("show_banner").set_value(false);
    config.at("target_prefix_checks")
        .set_value(MAMBA_ALLOW_EXISTING_PREFIX | MAMBA_ALLOW_MISSING_PREFIX
                   | MAMBA_ALLOW_NOT_ENV_PREFIX | MAMBA_NOT_EXPECT_EXISTING_PREFIX);
    config.load();

    auto& file_path = config.at("config_set_file_path").get_wrapped<fs::path>();
    auto& env_path = config.at("config_set_env_path").get_wrapped<bool>();
    auto& system_path = config.at("config_set_system_path").get_wrapped<bool>();
    auto specs = config.at("config_set_sequence_spec")
                     .value<std::vector<std::pair<std::string, std::string>>>();

    fs::path rc_source = env::expand_user(env::home_directory() / ".condarc");

    if (file_path.configured())
    {
        path::touch(file_path.value().string(), true);
        rc_source = env::expand_user(file_path.value()).string();
    }
    else if (env_path.configured())
    {
        std::string path = fs::path(ctx.target_prefix / ".condarc");
        env_path_setup(path, rc_source);
    }
    else if (system_path.configured())
    {
        system_path_setup(rc_source);
    }

    YAML::Node node = YAML::LoadFile(rc_source.string());
    for (auto& pair : specs)
        set_sequence_to_yaml(node, pair.first, pair.second, opt);

    std::ofstream rc_file = open_ofstream(rc_source, std::ofstream::in | std::ofstream::trunc);
    rc_file << node << std::endl;

    config.operation_teardown();
}

void
set_config_prepend_command(CLI::App* subcom)
{
    set_config_sequence_command(subcom);
    subcom->get_option("specs")->description(
        "Add value at the beginning of a configurable sequence");
    subcom->callback([&]() { set_sequence_to_rc(SequenceAddType::kPrepend); });
}

void
set_config_append_command(CLI::App* subcom)
{
    set_config_sequence_command(subcom);
    subcom->get_option("specs")->description("Add value at the end of a configurable sequence");
    subcom->callback([&]() { set_sequence_to_rc(SequenceAddType::kAppend); });
}

void
set_config_remove_key_command(CLI::App* subcom)
{
    set_config_path_command(subcom);

    auto& config = Configuration::instance();
    auto& ctx = Context::instance();

    auto& remove_key = config.insert(Configurable("remove_key", std::string(""))
                                         .group("Output, Prompt and Flow Control")
                                         .description("Remove a configuration key and its values"));
    subcom->add_option("remove_key", remove_key.set_cli_config(""), remove_key.description());

    auto& file_path = config.at("config_set_file_path").get_wrapped<fs::path>();
    auto& env_path = config.at("config_set_env_path").get_wrapped<bool>();
    auto& system_path = config.at("config_set_system_path").get_wrapped<bool>();

    subcom->callback([&]() {
        config.at("use_target_prefix_fallback").set_value(true);
        config.at("show_banner").set_value(false);
        config.at("target_prefix_checks")
            .set_value(MAMBA_ALLOW_EXISTING_PREFIX | MAMBA_ALLOW_MISSING_PREFIX
                       | MAMBA_ALLOW_NOT_ENV_PREFIX | MAMBA_NOT_EXPECT_EXISTING_PREFIX);
        config.load();

        fs::path rc_source = env::expand_user(env::home_directory() / ".condarc");
        bool key_removed = false;

        if (file_path.configured())
        {
            path::touch(file_path.value().string(), true);
            rc_source = env::expand_user(file_path.value()).string();
        }
        else if (env_path.configured())
        {
            std::string path = fs::path(ctx.target_prefix / ".condarc");
            if (!env_path_exists(path))
            {
                return;
            }
            env_path_setup(path, rc_source);
        }
        else if (system_path.configured())
        {
            if (!system_path_exists())
            {
                return;
            }
            system_path_setup(rc_source);
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
        std::ofstream rc_file = open_ofstream(rc_source, std::ofstream::in | std::ofstream::trunc);
        rc_file << rc_YAML << std::endl;

        config.operation_teardown();
    });
}

void
set_config_remove_command(CLI::App* subcom)
{
    // have to check if a string is a vector
    set_config_path_command(subcom);

    auto& config = Configuration::instance();
    auto& ctx = Context::instance();

    auto& remove_vec_map = config.insert(
        Configurable("remove", std::vector<std::string>())
            .group("Output, Prompt and Flow Control")
            .description(
                "Remove a configuration value from a list key. This removes all instances of the value."));
    subcom->add_option(
        "remove", remove_vec_map.set_cli_config({ "" }), remove_vec_map.description());

    auto& file_path = config.at("config_set_file_path").get_wrapped<fs::path>();
    auto& env_path = config.at("config_set_env_path").get_wrapped<bool>();
    auto& system_path = config.at("config_set_system_path").get_wrapped<bool>();

    subcom->callback([&]() {
        config.at("use_target_prefix_fallback").set_value(true);
        config.at("show_banner").set_value(false);
        config.at("target_prefix_checks")
            .set_value(MAMBA_ALLOW_EXISTING_PREFIX | MAMBA_ALLOW_MISSING_PREFIX
                       | MAMBA_ALLOW_NOT_ENV_PREFIX | MAMBA_NOT_EXPECT_EXISTING_PREFIX);
        config.load();

        fs::path rc_source = env::expand_user(env::home_directory() / ".condarc");
        bool key_removed = false;
        std::string remove_vec_key = remove_vec_map.value().front();
        std::string remove_vec_value = remove_vec_map.value().at(1);

        if (remove_vec_map.value().size() > 2)
        {
            std::cout << "Only one value can be removed at a time" << std::endl;
            return;
        }
        else if (file_path.configured())
        {
            path::touch(file_path.value().string(), true);
            rc_source = env::expand_user(file_path.value()).string();
        }
        else if (env_path.configured())
        {
            std::string path = fs::path(ctx.target_prefix / ".condarc");
            if (!env_path_exists(path))
            {
                return;
            }
            env_path_setup(path, rc_source);
        }
        else if (system_path.configured())
        {
            if (!system_path_exists())
            {
                return;
            }
            system_path_setup(rc_source);
        }

        // convert rc file to YAML::Node
        YAML::Node rc_YAML = YAML::LoadFile(rc_source.string());

        // look for key to remove in file
        for (auto v : rc_YAML)
        {
            if (v.first.as<std::string>() == remove_vec_key)
            {
                for (std::size_t i = 0; i < v.second.size(); ++i)
                {
                    if (v.second.size() == 1 && v.second[i].as<std::string>() == remove_vec_value)
                    {
                        rc_YAML.remove(remove_vec_key);
                        key_removed = true;
                        break;
                    }
                    else if (v.second[i].as<std::string>() == remove_vec_value)
                    {
                        rc_YAML[remove_vec_key].remove(i);
                        key_removed = true;
                        break;
                    }
                }
                break;
            }
        }

        if (!key_removed)
        {
            std::cout << "Key is not present in file" << std::endl;
        }

        // if the rc file is being modified, it's necessary to rewrite it
        std::ofstream rc_file = open_ofstream(rc_source, std::ofstream::in | std::ofstream::trunc);
        rc_file << rc_YAML << std::endl;

        config.operation_teardown();
    });
}

void
set_config_set_command(CLI::App* subcom)
{
    set_config_path_command(subcom);

    auto& config = Configuration::instance();
    auto& ctx = Context::instance();

    auto& set_value = config.insert(Configurable("set_value", std::vector<std::string>({}))
                                        .group("Output, Prompt and Flow Control")
                                        .description("Set configuration value on rc file"));
    subcom->add_option("set_value", set_value.set_cli_config({}), set_value.description());

    auto& file_path = config.at("config_set_file_path").get_wrapped<fs::path>();
    auto& env_path = config.at("config_set_env_path").get_wrapped<bool>();
    auto& system_path = config.at("config_set_system_path").get_wrapped<bool>();

    subcom->callback([&]() {
        config.at("use_target_prefix_fallback").set_value(true);
        config.at("show_banner").set_value(false);
        config.at("target_prefix_checks")
            .set_value(MAMBA_ALLOW_EXISTING_PREFIX | MAMBA_ALLOW_MISSING_PREFIX
                       | MAMBA_ALLOW_NOT_ENV_PREFIX | MAMBA_NOT_EXPECT_EXISTING_PREFIX);
        config.load();

        fs::path rc_source = env::expand_user(env::home_directory() / ".condarc");

        if (file_path.configured())
        {
            path::touch(file_path.value().string(), true);
            rc_source = env::expand_user(file_path.value().string());
        }
        else if (env_path.configured())
        {
            std::string path = fs::path(ctx.target_prefix / ".condarc");
            env_path_setup(path, rc_source);
        }
        else if (system_path.configured())
        {
            system_path_setup(rc_source);
        }

        YAML::Node rc_YAML = YAML::LoadFile(rc_source.string());

        if (is_valid_rc_key(set_value.value().at(0)) && set_value.value().size() < 3)
        {
            rc_YAML[set_value.value().at(0)] = set_value.value().at(1);
        }
        else
        {
            std::cout << "Key is invalid or more than one key was received" << std::endl;
        }

        // if the rc file is being modified, it's necessary to rewrite it
        std::ofstream rc_file = open_ofstream(rc_source, std::ofstream::in | std::ofstream::trunc);
        rc_file << rc_YAML << std::endl;

        config.operation_teardown();
    });
}

void
set_config_get_command(CLI::App* subcom)
{
    set_config_path_command(subcom);

    auto& config = Configuration::instance();
    auto& ctx = Context::instance();

    // TODO: get_value should be a vector of strings
    auto& get_value = config.insert(Configurable("get_value", std::string(""))
                                        .group("Output, Prompt and Flow Control")
                                        .description("Display configuration value from rc file"));
    subcom->add_option("get_value", get_value.set_cli_config(""), get_value.description());

    auto& file_path = config.at("config_set_file_path").get_wrapped<fs::path>();
    auto& env_path = config.at("config_set_env_path").get_wrapped<bool>();
    auto& system_path = config.at("config_set_system_path").get_wrapped<bool>();

    subcom->callback([&]() {
        config.at("use_target_prefix_fallback").set_value(true);
        config.at("show_banner").set_value(false);
        config.at("target_prefix_checks")
            .set_value(MAMBA_ALLOW_EXISTING_PREFIX | MAMBA_ALLOW_MISSING_PREFIX
                       | MAMBA_ALLOW_NOT_ENV_PREFIX | MAMBA_NOT_EXPECT_EXISTING_PREFIX);
        config.load();

        fs::path rc_source = env::expand_user(env::home_directory() / ".condarc");

        bool value_found = false;

        if (file_path.configured())
        {
            rc_source = env::expand_user(file_path.value()).string();
        }
        else if (env_path.configured())
        {
            std::string path = fs::path(ctx.target_prefix / ".condarc");
            if (!env_path_exists(path))
            {
                return;
            }
            env_path_setup(path, rc_source);
        }
        else if (system_path.configured())
        {
            if (!system_path_exists())
            {
                return;
            }
            system_path_setup(rc_source);
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

    auto remove_subcom = subcom->add_subcommand(
        "remove",
        "Remove a configuration value from a list key. This removes all instances of the value.");
    set_config_remove_command(remove_subcom);

    auto set_subcom = subcom->add_subcommand("set", "Set a configuration value");
    set_config_set_command(set_subcom);

    auto get_subcom = subcom->add_subcommand("get", "Get a configuration value");
    set_config_get_command(get_subcom);
}
