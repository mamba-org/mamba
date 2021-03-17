#include "list.hpp"
#include "common_options.hpp"

#include "mamba/configuration.hpp"
#include "mamba/install.hpp"
#include "mamba/list.hpp"


using namespace mamba;  // NOLINT(build/namespaces)

void
init_list_parser(CLI::App* subcom)
{
    init_general_options(subcom);
    init_prefix_options(subcom);

    auto& config = Configuration::instance();

    auto& regex
        = config.insert(Configurable("list_regex", std::string(""))
                            .group("cli")
                            .rc_configurable(false)
                            .description("List only packages matching a regular expression"));
    subcom->add_option("regex", regex.set_cli_config(""), regex.description());
}


void
set_list_command(CLI::App* subcom)
{
    init_list_parser(subcom);

    subcom->callback([]() {
        load_configuration(MAMBA_ALLOW_ROOT_PREFIX | MAMBA_ALLOW_FALLBACK_PREFIX
                               | MAMBA_ALLOW_EXISTING_PREFIX,
                           false);
        auto& config = Configuration::instance();
        auto& regex = config.at("list_regex").value<std::string>();

        using namespace detail;
        list_packages(regex);
    });
}
