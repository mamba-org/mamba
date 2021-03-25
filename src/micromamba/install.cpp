#include "install.hpp"
#include "info.hpp"
#include "common_options.hpp"

#include "mamba/configuration.hpp"
#include "mamba/install.hpp"


using namespace mamba;  // NOLINT(build/namespaces)


void
init_install_parser(CLI::App* subcom)
{
    init_general_options(subcom);
    init_prefix_options(subcom);
    init_network_parser(subcom);
    init_channel_parser(subcom);

    auto& config = Configuration::instance();

    auto& specs = config.insert(Configurable("specs", std::vector<std::string>({}))
                                    .group("cli")
                                    .rc_configurable(false)
                                    .description("Specs to install into the environment"));
    subcom->add_option("specs", specs.set_cli_config({}), specs.description());

    auto& file_specs = config.insert(Configurable("file_specs", std::vector<std::string>({}))
                                         .group("cli")
                                         .rc_configurable(false)
                                         .description("File (yaml, explicit or plain)"));
    subcom->add_option("-f,--file", file_specs.set_cli_config({}), file_specs.description())
        ->type_size(1)
        ->allow_extra_args(false);

    auto& no_pin = config.at("no_pin").get_wrapped<bool>();
    subcom->add_flag("--no-pin,!--pin", no_pin.set_cli_config(0), no_pin.description());

    auto& allow_softlinks = config.at("allow_softlinks").get_wrapped<bool>();
    subcom->add_flag("--allow-softlinks,!--no-allow-softlinks",
                     allow_softlinks.set_cli_config(0),
                     allow_softlinks.description());

    auto& always_softlink = config.at("always_softlink").get_wrapped<bool>();
    subcom->add_flag("--always-softlink,!--no-always-softlink",
                     always_softlink.set_cli_config(0),
                     always_softlink.description());

    auto& always_copy = config.at("always_copy").get_wrapped<bool>();
    subcom->add_flag("--always-copy,!--no-always-copy",
                     always_copy.set_cli_config(0),
                     always_copy.description());

    auto& extra_safety_checks = config.at("extra_safety_checks").get_wrapped<bool>();
    subcom->add_flag("--extra-safety-checks,!--no-extra-safety-checks",
                     extra_safety_checks.set_cli_config(0),
                     extra_safety_checks.description());

    auto& safety_checks = config.at("safety_checks").get_wrapped<VerificationLevel>();
    subcom->add_set("--safety-checks",
                    safety_checks.set_cli_config(""),
                    { "enabled", "warn", "disabled" },
                    safety_checks.description());
}


void
set_install_command(CLI::App* subcom)
{
    init_install_parser(subcom);

    subcom->callback([&]() {
        detail::parse_file_specs();

        auto& config = Configuration::instance();
        auto& specs = config.at("specs").value<std::vector<std::string>>();
        install(specs);
    });
}
