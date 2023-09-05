// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_PACKAGE_PACKAGE_HPP
#define MAMBA_PACKAGE_PACKAGE_HPP

#include <CLI/CLI.hpp>

namespace mamba
{
    class Context;
}

void
set_package_command(CLI::App* com, mamba::Context& context);

#endif
