// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include "common_options.hpp"
#include "umamba.hpp"

#include "mamba/api/configuration.hpp"
#include "mamba/api/shell.hpp"


using namespace mamba;  // NOLINT(build/namespaces)

void
init_shell_parser(CLI::App* subcom)
{
    init_general_options(subcom);

    auto& config = Configuration::instance();

    auto& shell_type = config.insert(
        Configurable("shell_type", std::string("")).group("cli").description("A shell type"));
    subcom->add_option("-s,--shell", shell_type.set_cli_config(""), shell_type.description())
        ->check(CLI::IsMember(
            std::set<std::string>({ "bash", "posix", "powershell", "cmd.exe", "xonsh", "zsh" })));

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
    subcom->add_flag("--stack", stack.set_cli_config(false), stack.description());

    auto& action = config.insert(Configurable("shell_action", std::string(""))
                                     .group("cli")
                                     .description("The action to complete"));
    subcom->add_option("action", action.set_cli_config(""), action.description())
        ->check(CLI::IsMember(std::set<std::string>({ "init",
                                                      "activate",
                                                      "deactivate",
                                                      "hook",
                                                      "reactivate",
                                                      "deactivate",
                                                      "completion"
#ifdef _WIN32
                                                      ,
                                                      "enable-long-paths-support"
#endif
        })));

    auto& prefix = config.insert(
        Configurable("shell_prefix", std::string(""))
            .group("cli")
            .description("The root prefix to configure (for init and hook), and the prefix "
                         "to activate for activate, either by name or by path"));
    subcom->add_option("prefix,-p,--prefix", prefix.set_cli_config(""), prefix.description());

    auto& auto_activate_base = config.at("auto_activate_base").get_wrapped<bool>();
    subcom->add_flag("--auto-activate-base,!--no-auto-activate-base",
                     auto_activate_base.set_cli_config(0),
                     auto_activate_base.description());
}


void
set_shell_command(CLI::App* subcom)
{
    init_shell_parser(subcom);

    subcom->callback([&]() {
        auto& config = Configuration::instance();

        auto& action = config.at("shell_action").compute().value<std::string>();
        auto& prefix = config.at("shell_prefix").compute().value<std::string>();
        auto& shell = config.at("shell_type").compute().value<std::string>();
        auto& stack = config.at("shell_stack").compute().value<bool>();

        if (action == "completion")
        {
            detect_shell(shell);

            if (shell == "bash")
            {
                fs::path home = env::home_directory();
                fs::path bashrc_path
                    = (on_mac || on_win) ? home / ".bash_profile" : home / ".bashrc";
                std::regex completion_re("# >>> umamba completion >>>(?:\n|\r\n)?"
                                         "([\\s\\S]*?)"
                                         "# <<< umamba completion <<<(?:\n|\r\n)?");

                std::string rc_content = read_contents(bashrc_path, std::ios::in);
                std::string result = std::regex_replace(
                    rc_content, completion_re, unindent(umamba_bash_completion));

                if (result.find("# >>> umamba completion >>>") == result.npos)
                {
                    std::ofstream rc_file(bashrc_path, std::ios::app | std::ios::binary);
                    rc_file << std::endl << unindent(umamba_bash_completion);
                }
                else
                {
                    std::ofstream rc_file(bashrc_path, std::ios::out | std::ios::binary);
                    rc_file << result;
                }
            }
            else
            {
                LOG_ERROR << "Shell auto-completion is not supported in '" << shell << "'";
                throw std::runtime_error("Shell auto-completion is not supported in '" + shell
                                         + "'");
            }
        }
        else
            mamba::shell(action, shell, prefix, stack);
    });
}
