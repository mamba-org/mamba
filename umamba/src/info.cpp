// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include "common_options.hpp"

#include "mamba/api/info.hpp"


using namespace mamba;  // NOLINT(build/namespaces)

void
init_info_parser(CLI::App* subcom)
{
    init_prefix_options(subcom);
}

void
set_info_command(CLI::App* subcom)
{
    init_info_parser(subcom);

    subcom->callback([&]() { info(); });
}
