#include "common_options.hpp"

#include "mamba/api/configuration.hpp"
#include "mamba/api/install.hpp"


using namespace mamba;  // NOLINT(build/namespaces)


void
set_install_command(CLI::App* subcom)
{
    init_install_options(subcom);

    auto& config = Configuration::instance();

    auto& freeze_installed = config.at("freeze_installed").get_wrapped<bool>();
    subcom->add_flag(
        "--freeze-installed", freeze_installed.set_cli_config(0), freeze_installed.description());

    subcom->callback([&]() { install(); });
}
