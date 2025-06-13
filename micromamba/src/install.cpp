#include <optional>

#include "mamba/api/configuration.hpp"
#include "mamba/api/install.hpp"

#include "common_options.hpp"


using namespace mamba;  // NOLINT(build/namespaces)

void
set_install_command(CLI::App* subcom, Configuration& config)
{
    init_install_options(subcom, config);

    auto& freeze_installed = config.at("freeze_installed");
    subcom->add_flag(
        "--freeze-installed",
        freeze_installed.get_cli_config<bool>(),
        freeze_installed.description()
    );
    auto& force_reinstall = config.at("force_reinstall");
    subcom->add_flag(
        "--force-reinstall",
        force_reinstall.get_cli_config<bool>(),
        force_reinstall.description()
    );

    static std::optional<std::size_t> revision = std::nullopt;
    subcom->callback(
        [&]
        {
            if (revision.has_value())
            {
                return mamba::install_revision(config, revision.value());
            }
            return mamba::install(config);
        }
    );
    subcom->add_option("--revision", revision, "Revert to the specified revision.");
}
