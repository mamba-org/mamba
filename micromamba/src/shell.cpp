// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include "mamba/api/configuration.hpp"
#include "mamba/api/shell.hpp"
#include "mamba/core/fsutil.hpp"
#include "mamba/core/run.hpp"
#include "mamba/core/shell_init.hpp"

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

    auto& prefix = config.insert(
        Configurable("shell_prefix", std::string(""))
            .group("cli")
            .description("The root prefix to configure (for init and hook), and the prefix "
                         "to activate for activate, either by name or by path")
    );
    subcom->add_option(
        "prefix,-p,--prefix,-n,--name",
        prefix.get_cli_config<std::string>(),
        prefix.description()
    );
}

namespace
{
    auto consolidate_shell(std::string_view shell_type) -> std::string
    {
        if (!shell_type.empty())
        {
            return std::string{ shell_type };
        }

        LOG_DEBUG << "No shell type provided";

        if (std::string guessed_shell = guess_shell(); !guessed_shell.empty())
        {
            LOG_DEBUG << "Guessed shell: '" << guessed_shell << "'";
            return guessed_shell;
        }

        LOG_ERROR << "Please provide a shell type.\n"
                     "Run with --help for more information.\n";
        throw std::runtime_error("Unknown shell type. Aborting.");
    }

    void
    shell(const std::string& action, std::string shell_type, const std::string& prefix, bool stack)
    {
        auto& config = Configuration::instance();

        config.at("show_banner").set_value(false);
        config.at("use_target_prefix_fallback").set_value(false);
        config.at("target_prefix_checks").set_value(MAMBA_NO_PREFIX_CHECK);
        config.load();

        shell_type = consolidate_shell(shell_type);

        if (action == "init")
        {
            shell_init(shell_type, prefix);
        }
        else if (action == "deinit")
        {
            shell_deinit(shell_type, prefix);
        }
        else if (action == "reinit")
        {
            shell_reinit(prefix);
        }
        else if (action == "hook")
        {
            shell_hook(shell_type);
        }
        else if (action == "activate")
        {
            shell_activate(prefix, shell_type, stack);
        }
        else if (action == "reactivate")
        {
            shell_reactivate(shell_type);
        }
        else if (action == "deactivate")
        {
            shell_deactivate(shell_type);
        }
        else if (action == "enable-long-paths-support")
        {
            shell_enable_long_path_support();
        }
        else
        {
            throw std::runtime_error("Need an action {init, hook, activate, deactivate, reactivate}");
        }

        config.operation_teardown();
    }
}


void
set_shell_command(CLI::App* subcom)
{
    init_shell_parser(subcom);

    subcom->callback(
        []
        {
            auto& config = Configuration::instance();

            auto& prefix = config.at("shell_prefix").compute().value<std::string>();
            auto& action = config.at("shell_action").compute().value<std::string>();
            auto& shell = config.at("shell_type").compute().value<std::string>();
            auto& stack = config.at("shell_stack").compute().value<bool>();

            if (action.empty())
            {
                if (prefix.empty() || prefix == "base")
                {
                    Context::instance().prefix_params.target_prefix = Context::instance()
                                                                          .prefix_params.root_prefix;
                }
                else
                {
                    Context::instance().prefix_params.target_prefix = Context::instance()
                                                                          .prefix_params.root_prefix
                                                                      / "envs" / prefix;
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
                ::shell(action, shell, prefix, stack);
                return 0;
            }
        }
    );
}
