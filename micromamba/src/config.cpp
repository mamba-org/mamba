// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <fstream>

#include <yaml-cpp/yaml.h>

#include "mamba/api/config.hpp"
#include "mamba/api/configuration.hpp"
#include "mamba/core/fsutil.hpp"
#include "mamba/core/util.hpp"
#include "mamba/util/build.hpp"

#include "common_options.hpp"

using namespace mamba;  // NOLINT(build/namespaces)

bool
is_valid_rc_key(const mamba::Configuration& config, const std::string& key)
{
    try
    {
        return config.config().at(key).rc_configurable();
    }
    catch (const std::out_of_range& /*e*/)
    {
        return false;
    }
}

bool
is_valid_rc_sequence(const mamba::Configuration& config, const std::string& key, const std::string& value)
{
    try
    {
        const auto& c = config.config().at(key);
        return c.is_valid_serialization(value) && c.rc_configurable() && c.is_sequence();
    }
    catch (const std::out_of_range& /*e*/)
    {
        return false;
    }
}

fs::u8path
get_system_path()
{
    return (util::on_mac || util::on_linux) ? fs::u8path("/etc/conda/.condarc")
                                            : fs::u8path("C:\\ProgramData\\conda\\.condarc");
}

fs::u8path
compute_config_path(Configuration& config, bool touch_if_not_exists)
{
    auto& ctx = Context::instance();

    auto& file_path = config.at("config_set_file_path");
    auto& env_path = config.at("config_set_env_path");
    auto& system_path = config.at("config_set_system_path");

    fs::u8path rc_source = env::expand_user(env::home_directory() / ".condarc");

    if (file_path.configured())
    {
        rc_source = env::expand_user(file_path.value<fs::u8path>()).string();
    }
    else if (env_path.configured())
    {
        rc_source = fs::u8path{ ctx.prefix_params.target_prefix / ".condarc" };
    }
    else if (system_path.configured())
    {
        rc_source = get_system_path();
    }

    if (!fs::exists(rc_source))
    {
        if (touch_if_not_exists)
        {
            path::touch(rc_source, true);
        }
        else
        {
            throw std::runtime_error("RC file does not exist at " + rc_source.string());
        }
    }

    return rc_source;
}

void
init_config_options(CLI::App* subcom, mamba::Configuration& config)
{
    init_general_options(subcom, config);
    init_prefix_options(subcom, config);
}

void
init_config_describe_options(CLI::App* subcom, mamba::Configuration& config)
{
    auto& specs = config.at("specs");
    subcom->add_option("configs", specs.get_cli_config<std::vector<std::string>>(), "Configuration keys");

    auto& show_long_descriptions = config.at("show_config_long_descriptions");
    subcom->add_flag(
        "-l,--long-descriptions",
        show_long_descriptions.get_cli_config<bool>(),
        show_long_descriptions.description()
    );

    auto& show_groups = config.at("show_config_groups");
    subcom->add_flag("-g,--groups", show_groups.get_cli_config<bool>(), show_groups.description());
}

void
init_config_list_options(CLI::App* subcom, mamba::Configuration& config)
{
    init_config_options(subcom, config);
    init_config_describe_options(subcom, config);

    auto& show_sources = config.at("show_config_sources");
    subcom->add_flag("-s,--sources", show_sources.get_cli_config<bool>(), show_sources.description());

    auto& show_all = config.at("show_all_rc_configs");
    subcom->add_flag("-a,--all", show_all.get_cli_config<bool>(), show_all.description());

    auto& show_descriptions = config.at("show_config_descriptions");
    subcom->add_flag(
        "-d,--descriptions",
        show_descriptions.get_cli_config<bool>(),
        show_descriptions.description()
    );
}

void
set_config_list_command(CLI::App* subcom, mamba::Configuration& config)
{
    init_config_list_options(subcom, config);

    subcom->callback(
        [&]()
        {
            config_list(config);
            return 0;
        }
    );
}

void
set_config_sources_command(CLI::App* subcom, mamba::Configuration& config)
{
    init_config_options(subcom, config);

    subcom->callback(
        [&]()
        {
            config_sources(config);
            return 0;
        }
    );
}

void
set_config_describe_command(CLI::App* subcom, mamba::Configuration& config)
{
    init_config_describe_options(subcom, config);

    subcom->callback(
        [&]
        {
            config_describe(config);
            return 0;
        }
    );
}

void
set_config_path_command(CLI::App* subcom, mamba::Configuration& config)
{
    auto& system_path = config.insert(
        Configurable("config_set_system_path", false)
            .group("cli")
            .description("Set configuration on system's rc file"),
        true
    );
    auto* system_flag = subcom->add_flag(
        "--system",
        system_path.get_cli_config<bool>(),
        system_path.description()
    );

    auto& env_path = config.insert(
        Configurable("config_set_env_path", false)
            .group("cli")
            .description("Set configuration on env's rc file"),
        true
    );
    auto* env_flag = subcom
                         ->add_flag("--env", env_path.get_cli_config<bool>(), env_path.description())
                         ->excludes(system_flag);

    auto& file_path = config.insert(
        Configurable("config_set_file_path", fs::u8path())
            .group("cli")
            .description("Set configuration on specified file"),
        true
    );
    subcom->add_option("--file", file_path.get_cli_config<fs::u8path>(), file_path.description())
        ->excludes(system_flag)
        ->excludes(env_flag);
}


enum class SequenceAddType
{
    kAppend = 0,
    kPrepend = 1
};

void
set_config_sequence_command(CLI::App* subcom, mamba::Configuration& config)
{
    set_config_path_command(subcom, config);

    using config_set_sequence_type = std::vector<std::pair<std::string, std::string>>;
    auto& specs = config.insert(
        Configurable("config_set_sequence_spec", config_set_sequence_type({}))
            .group("Output, Prompt and Flow Control")
            .description("Add value to a configurable sequence"),
        true
    );
    subcom
        ->add_option("specs", specs.get_cli_config<config_set_sequence_type>(), specs.description())
        ->required();
}

void
set_sequence_to_yaml(
    mamba::Configuration& config,
    YAML::Node& node,
    const std::string& key,
    const std::string& value,
    const SequenceAddType& opt
)
{
    if (!is_valid_rc_sequence(config, key, value))
    {
        if (!is_valid_rc_key(config, key))
        {
            LOG_ERROR << "Invalid key '" << key << "' or not rc configurable";
        }
        else
        {
            LOG_ERROR << "Invalid sequence key";
        }
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
                {
                    existing_values.erase(pos);
                }
            }
        }

        if (opt == SequenceAddType::kAppend)
        {
            existing_values.insert(existing_values.end(), values.begin(), values.end());
        }
        else
        {
            existing_values.insert(existing_values.begin(), values.begin(), values.end());
        }

        node[key] = existing_values;
    }
    else
    {
        node[key] = values;
    }
}

void
set_sequence_to_rc(mamba::Configuration& config, const SequenceAddType& opt)
{
    config.at("use_target_prefix_fallback").set_value(true);
    config.at("target_prefix_checks")
        .set_value(
            MAMBA_ALLOW_EXISTING_PREFIX | MAMBA_ALLOW_MISSING_PREFIX | MAMBA_ALLOW_NOT_ENV_PREFIX
            | MAMBA_NOT_EXPECT_EXISTING_PREFIX
        );
    config.load();

    auto specs = config.at("config_set_sequence_spec")
                     .value<std::vector<std::pair<std::string, std::string>>>();

    fs::u8path rc_source = compute_config_path(config, true);

    YAML::Node node = YAML::LoadFile(rc_source.string());
    for (auto& pair : specs)
    {
        set_sequence_to_yaml(config, node, pair.first, pair.second, opt);
    }

    std::ofstream rc_file = open_ofstream(rc_source, std::ofstream::in | std::ofstream::trunc);
    rc_file << node << std::endl;

    config.operation_teardown();
}

void
set_config_prepend_command(CLI::App* subcom, mamba::Configuration& config)
{
    set_config_sequence_command(subcom, config);
    subcom->get_option("specs")->description("Add value at the beginning of a configurable sequence");
    subcom->callback([&] { set_sequence_to_rc(config, SequenceAddType::kPrepend); });
}

void
set_config_append_command(CLI::App* subcom, mamba::Configuration& config)
{
    set_config_sequence_command(subcom, config);
    subcom->get_option("specs")->description("Add value at the end of a configurable sequence");
    subcom->callback([&] { set_sequence_to_rc(config, SequenceAddType::kAppend); });
}

void
set_config_remove_key_command(CLI::App* subcom, mamba::Configuration& config)
{
    set_config_path_command(subcom, config);

    auto& remove_key = config.insert(Configurable("remove_key", std::string(""))
                                         .group("Output, Prompt and Flow Control")
                                         .description("Remove a configuration key and its values"));
    subcom->add_option("remove_key", remove_key.get_cli_config<std::string>(), remove_key.description());

    subcom->callback(
        [&]()
        {
            config.at("use_target_prefix_fallback").set_value(true);
            config.at("target_prefix_checks")
                .set_value(
                    MAMBA_ALLOW_EXISTING_PREFIX | MAMBA_ALLOW_MISSING_PREFIX
                    | MAMBA_ALLOW_NOT_ENV_PREFIX | MAMBA_NOT_EXPECT_EXISTING_PREFIX
                );
            config.load();

            const fs::u8path rc_source = compute_config_path(config, false);

            bool key_removed = false;
            // convert rc file to YAML::Node
            YAML::Node rc_YAML = YAML::LoadFile(rc_source.string());

            // look for key to remove in file
            for (auto v : rc_YAML)
            {
                const std::string& rk = remove_key.value<std::string>();
                if (v.first.as<std::string>() == rk)
                {
                    rc_YAML.remove(rk);
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
        }
    );
}

void
set_config_remove_command(CLI::App* subcom, mamba::Configuration& config)
{
    using string_list = std::vector<std::string>;
    // have to check if a string is a vector
    set_config_path_command(subcom, config);

    auto& remove_vec_map = config.insert(
        Configurable("remove", std::vector<std::string>())
            .group("Output, Prompt and Flow Control")
            .description(
                "Remove a configuration value from a list key. This removes all instances of the value."
            )
    );
    subcom->add_option(
        "remove",
        remove_vec_map.get_cli_config<string_list>(),
        remove_vec_map.description()
    );

    subcom->callback(
        [&]
        {
            config.at("use_target_prefix_fallback").set_value(true);
            config.at("target_prefix_checks")
                .set_value(
                    MAMBA_ALLOW_EXISTING_PREFIX | MAMBA_ALLOW_MISSING_PREFIX
                    | MAMBA_ALLOW_NOT_ENV_PREFIX | MAMBA_NOT_EXPECT_EXISTING_PREFIX
                );
            config.load();

            const fs::u8path rc_source = compute_config_path(config, false);
            bool key_removed = false;

            const string_list& rvm = remove_vec_map.value<string_list>();
            std::string remove_vec_key = rvm.front();
            std::string remove_vec_value = rvm.at(1);

            if (rvm.size() > 2)
            {
                std::cout << "Only one value can be removed at a time" << std::endl;
                return;
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
        }
    );
}

void
set_config_set_command(CLI::App* subcom, mamba::Configuration& config)
{
    using string_list = std::vector<std::string>;
    set_config_path_command(subcom, config);

    auto& set_value = config.insert(Configurable("set_value", std::vector<std::string>({}))
                                        .group("Output, Prompt and Flow Control")
                                        .description("Set configuration value on rc file"));
    subcom->add_option("set_value", set_value.get_cli_config<string_list>(), set_value.description());


    subcom->callback(
        [&]
        {
            config.at("use_target_prefix_fallback").set_value(true);
            config.at("target_prefix_checks")
                .set_value(
                    MAMBA_ALLOW_EXISTING_PREFIX | MAMBA_ALLOW_MISSING_PREFIX
                    | MAMBA_ALLOW_NOT_ENV_PREFIX | MAMBA_NOT_EXPECT_EXISTING_PREFIX
                );
            config.load();

            const fs::u8path rc_source = compute_config_path(config, true);

            YAML::Node rc_YAML = YAML::LoadFile(rc_source.string());

            const string_list& sv = set_value.value<string_list>();
            if (is_valid_rc_key(config, sv.at(0)) && sv.size() < 3)
            {
                rc_YAML[sv.at(0)] = sv.at(1);
            }
            else
            {
                std::cout << "Key is invalid or more than one key was received" << std::endl;
            }

            // if the rc file is being modified, it's necessary to rewrite it
            std::ofstream rc_file = open_ofstream(rc_source, std::ofstream::in | std::ofstream::trunc);
            rc_file << rc_YAML << std::endl;

            config.operation_teardown();
        }
    );
}

void
set_config_get_command(CLI::App* subcom, mamba::Configuration& config)
{
    set_config_path_command(subcom, config);

    // TODO: get_value should be a vector of strings
    auto& get_value = config.insert(Configurable("get_value", std::string(""))
                                        .group("Output, Prompt and Flow Control")
                                        .description("Display configuration value from rc file"));
    subcom->add_option("get_value", get_value.get_cli_config<std::string>(), get_value.description());

    subcom->callback(
        [&]
        {
            config.at("use_target_prefix_fallback").set_value(true);
            config.at("target_prefix_checks")
                .set_value(
                    MAMBA_ALLOW_EXISTING_PREFIX | MAMBA_ALLOW_MISSING_PREFIX
                    | MAMBA_ALLOW_NOT_ENV_PREFIX | MAMBA_NOT_EXPECT_EXISTING_PREFIX
                );
            config.load();

            fs::u8path rc_source = compute_config_path(config, false);

            bool value_found = false;

            YAML::Node rc_YAML = YAML::LoadFile(rc_source.string());

            for (auto v : rc_YAML)
            {
                if (v.first.as<std::string>() == get_value.value<std::string>())
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
        }
    );
}

void
set_config_command(CLI::App* subcom, mamba::Configuration& config)
{
    init_config_options(subcom, config);

    auto list_subcom = subcom->add_subcommand("list", "List configuration values");
    set_config_list_command(list_subcom, config);

    auto sources_subcom = subcom->add_subcommand("sources", "Show configuration sources");
    set_config_sources_command(sources_subcom, config);

    auto describe_subcom = subcom->add_subcommand("describe", "Describe given configuration parameters");
    set_config_describe_command(describe_subcom, config);

    auto prepend_subcom = subcom->add_subcommand(
        "prepend",
        "Add one configuration value to the beginning of a list key"
    );
    set_config_prepend_command(prepend_subcom, config);

    auto append_subcom = subcom->add_subcommand(
        "append",
        "Add one configuration value to the end of a list key"
    );
    set_config_append_command(append_subcom, config);

    auto remove_key_subcom = subcom->add_subcommand(
        "remove-key",
        "Remove a configuration key and its values"
    );
    set_config_remove_key_command(remove_key_subcom, config);

    auto remove_subcom = subcom->add_subcommand(
        "remove",
        "Remove a configuration value from a list key. This removes all instances of the value."
    );
    set_config_remove_command(remove_subcom, config);

    auto set_subcom = subcom->add_subcommand("set", "Set a configuration value");
    set_config_set_command(set_subcom, config);

    auto get_subcom = subcom->add_subcommand("get", "Get a configuration value");
    set_config_get_command(get_subcom, config);
}
