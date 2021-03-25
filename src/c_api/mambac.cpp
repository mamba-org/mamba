// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include "mamba/c_api/mambac.h"
#include "mamba/config.hpp"
#include "mamba/configuration.hpp"
#include "mamba/create.hpp"
#include "mamba/info.hpp"
#include "mamba/install.hpp"
#include "mamba/shell.hpp"
#include "mamba/list.hpp"
#include "mamba/update.hpp"
#include "mamba/fetch.hpp"

#include <stdio.h>
#include <string>

using namespace mamba;


void
mamba_create(const char* const* specs, const char* prefix)
{
    std::vector<std::string> specifications;
    for (const char* c = *specs; c; c = *++specs)
    {
        specifications.push_back(c);
    }

    create(specifications, prefix);
}

void
mamba_install(const char* const* specs, const char* prefix)
{
    std::vector<std::string> specifications;
    for (const char* c = *specs; c; c = *++specs)
    {
        specifications.push_back(c);
    }

    install(specifications, prefix);
}

void
mamba_update(const char* const* specs, const char* prefix)
{
    std::vector<std::string> specifications;
    for (const char* c = *specs; c; c = *++specs)
    {
        specifications.push_back(c);
    }

    update(specifications, prefix);
}

void
mamba_list(const char* regex, const char* prefix)
{
    list(regex, prefix);
}

void
mamba_info(const char* prefix)
{
    info(prefix);
}

void
mamba_shell(const char* action, const char* shell_type, const char* prefix)
{
    shell(action, shell_type, prefix);
}

void
mamba_config_list()
{
    config_list();
}

void
mamba_set_config(const char* name, const char* value)
{
    try
    {
        Configuration::instance().at(name).set_value(value);
    }
    catch (const std::out_of_range& e)
    {
    }
}

void
mamba_clear_config(const char* name)
{
    Configuration::instance().at(name).clear_values();
}
