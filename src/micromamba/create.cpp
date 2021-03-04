// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include "create.hpp"
#include "install.hpp"
#include "options.hpp"
#include "parsers.hpp"


using namespace mamba;  // NOLINT(build/namespaces)

void
set_create_command(CLI::App* subcom)
{
    init_install_parser(subcom);

    subcom->callback([&]() {
        auto& ctx = Context::instance();

        parse_file_options();
        load_install_options(ctx);

        if (!create_options.specs.empty() && !ctx.target_prefix.empty())
        {
            if (ctx.target_prefix == ctx.root_prefix)
            {
                if (Console::prompt(
                        "It's not possible to overwrite 'base' env. Install in 'base' instead?",
                        'n'))
                {
                    install_specs(create_options.specs, false);
                }
                else
                {
                    exit(1);
                }
            }
            catch_existing_target_prefix(ctx);
            install_specs(create_options.specs, true);
        }
        else
        {
            Console::print("Nothing to do.");
        }

        return 0;
    });
}
