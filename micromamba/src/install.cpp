#include "common_options.hpp"

#include "mamba/api/configuration.hpp"
#include "mamba/api/install.hpp"


using namespace mamba;  // NOLINT(build/namespaces)


void
set_install_command(CLI::App* subcom)
{
    init_install_options(subcom);

    auto& config = Configuration::instance();

    auto& freeze_installed = config.at("freeze_installed");
    subcom->add_flag("--freeze-installed",
                     freeze_installed.get_cli_config<bool>(),
                     freeze_installed.description());
    auto& force_reinstall = config.at("force_reinstall");
    subcom->add_flag(
        "--force-reinstall", force_reinstall.get_cli_config<bool>(), force_reinstall.description());

    subcom->callback([&]() { install(); });
}
