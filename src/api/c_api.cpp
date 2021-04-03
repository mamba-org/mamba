// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include "mamba/api/c_api.h"
#include "mamba/api/config.hpp"
#include "mamba/api/configuration.hpp"
#include "mamba/api/create.hpp"
#include "mamba/api/info.hpp"
#include "mamba/api/install.hpp"
#include "mamba/api/list.hpp"
#include "mamba/api/remove.hpp"
#include "mamba/api/update.hpp"

#include <stdio.h>
#include <string>

using namespace mamba;


void
mamba_create()
{
    create();
}

void
mamba_install()
{
    install();
}

void
mamba_update(int update_all)
{
    update(update_all);
}

void
mamba_remove(int remove_all)
{
    remove(remove_all);
}

void
mamba_list(const char* regex)
{
    list(regex);
}

void
mamba_info()
{
    info();
}

void
mamba_config_list()
{
    config_list();
}

void
mamba_set_config(const char* name, const char* value)
{
    Configuration::instance().at(name).set_value(value);
}

void
mamba_clear_config(const char* name)
{
    Configuration::instance().at(name).clear_values();
}
