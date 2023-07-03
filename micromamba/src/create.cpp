// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include "mamba/api/create.hpp"

#include "common_options.hpp"


using namespace mamba;  // NOLINT(build/namespaces)

void
set_create_command(CLI::App* subcom, Configuration& config)
{
    init_install_options(subcom, config);

    subcom->callback([&] { return mamba::create(config); });
}
