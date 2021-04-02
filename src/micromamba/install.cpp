#include "common_options.hpp"

#include "mamba/api/install.hpp"


using namespace mamba;  // NOLINT(build/namespaces)


void
set_install_command(CLI::App* subcom)
{
    init_install_options(subcom);

    subcom->callback([&]() { install(); });
}
