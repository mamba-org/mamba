// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include "mamba/api/configuration.hpp"
#include "mamba/api/shell.hpp"
#include "mamba/core/fsutil.hpp"
#include "mamba/core/run.hpp"

#include "common_options.hpp"
#include "umamba.hpp"


using namespace mamba;  // NOLINT(build/namespaces)

void
init_shell_parser(CLI::App* subcom)
{
    init_general_options(subcom);

    auto& config = Configuration::instance();

    auto& shell_type = config.insert(
        Configurable("shell_type", std::string("")).group("cli").description("A shell type")
    );
    subcom
        ->add_option("-s,--shell", shell_type.get_cli_config<std::string>(), shell_type.description())
        ->check(CLI::IsMember(std::set<std::string>(
            { "bash", "posix", "powershell", "cmd.exe", "xonsh", "zsh", "fish", "tcsh", "dash" }
        )));

    auto& stack = config.insert(Configurable("shell_stack", false)
                                    .group("cli")
                                    .description("Stack the environment being activated")
                                    .long_description(unindent(R"(
                       Stack the environment being activated on top of the
                       previous active environment, rather replacing the
                       current active environment with a new one.
                       Currently, only the PATH environment variable is stacked.
                       This may be enabled implicitly by the 'auto_stack'
                       configuration variable.)")));
    subcom->add_flag("--stack", stack.get_cli_config<bool>(), stack.description());

    auto& action = config.insert(
        Configurable("shell_action", std::string("")).group("cli").description("The action to complete")
    );
    subcom->add_option("action", action.get_cli_config<std::string>(), action.description())
        ->check(CLI::IsMember(std::vector<std::string>({ "init",
                                                         "deinit",
                                                         "reinit",
                                                         "hook",
                                                         "activate",
                                                         "deactivate",
                                                         "reactivate"
#ifdef _WIN32
                                                         ,
                                                         "enable-long-paths-support"
#endif
        })));

    auto& prefix_path = config.insert(
        Configurable("shell_prefix_path", std::string(""))
            .group("cli")
            .description("The root prefix path to configure (for init and hook), and the prefix "
                         "path to activate (for activate)")
    );

    auto& prefix_name = config.insert(
        Configurable("shell_prefix_name", std::string(""))
            .group("cli")
            .description("The root prefix name to configure (for init and hook), and the prefix "
                         "name to activate (for activate)")
    );

    subcom->add_option(
        "-p,--prefix",
        prefix_path.get_cli_config<std::string>(),
        prefix_path.description()
    );

    subcom->add_option(
        "-n,--name",
        prefix_name.get_cli_config<std::string>(),
        prefix_name.description()
    );
}


void
set_shell_command(CLI::App* subcom)
{
    init_shell_parser(subcom);

    subcom->callback(
        [&]()
        {
            auto& config = Configuration::instance();

            auto& prefix_path = config.at("shell_prefix_path").compute().value<std::string>();
            auto& prefix_name = config.at("shell_prefix_name").compute().value<std::string>();
            auto& action = config.at("shell_action").compute().value<std::string>();
            auto& shell = config.at("shell_type").compute().value<std::string>();
            auto& stack = config.at("shell_stack").compute().value<bool>();

            std::string prefix;
            if (!prefix_path.empty() && !prefix_name.empty())
            {
                LOG_ERROR << "Cannot set both prefix and env name";
                throw std::runtime_error("Aborting.");
            }
            else if (!prefix_path.empty())
            {
                prefix = prefix_path;
            }
            else if (!prefix_name.empty())
            {
                if (prefix_name.empty() || prefix_name == "base")
                {
                    prefix = prefix_name;
                }
                else
                {
                    prefix = Context::instance().root_prefix / "envs" / prefix_name;
                }
            }

            if (action.empty())
            {
                if (prefix.empty() || prefix == "base")
                {
                    Context::instance().target_prefix = Context::instance().root_prefix;
                }
                else
                {
                    Context::instance().target_prefix = prefix;
                }

                std::string default_shell = "bash";
                if (on_win)
                {
                    default_shell = "cmd.exe";
                }
                else if (on_mac)
                {
                    default_shell = "zsh";
                }

                auto env_shell = env::get("SHELL").value_or(default_shell);
                exit(mamba::run_in_environment(
                    { env_shell },
                    ".",
                    static_cast<int>(STREAM_OPTIONS::ALL_STREAMS),
                    false,
                    false,
                    {},
                    ""
                ));
            }
            else
            {
                mamba::shell(action, shell, prefix, stack);
                return 0;
            }
        }
    );
}
