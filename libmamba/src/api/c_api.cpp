// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include "mamba/api/c_api.h"

#include <string>

#include "mamba/api/config.hpp"
#include "mamba/api/configuration.hpp"
#include "mamba/api/create.hpp"
#include "mamba/api/info.hpp"
#include "mamba/api/install.hpp"
#include "mamba/api/list.hpp"
#include "mamba/api/remove.hpp"
#include "mamba/api/update.hpp"

using namespace mamba;


int
mamba_create()
{
    try
    {
        create();
        return 0;
    }
    catch (...)
    {
        Configuration::instance().operation_teardown();
        return 1;
    }
}

int
mamba_install()
{
    try
    {
        install();
        return 0;
    }
    catch (...)
    {
        Configuration::instance().operation_teardown();
        return 1;
    }
}

int
mamba_update(int update_all)
{
    try
    {
        update(update_all);
        return 0;
    }
    catch (...)
    {
        Configuration::instance().operation_teardown();
        return 1;
    }
}

int
mamba_remove(int remove_all)
{
    try
    {
        remove(remove_all);
        return 0;
    }
    catch (...)
    {
        Configuration::instance().operation_teardown();
        return 1;
    }
}

int
mamba_list(const char* regex)
{
    try
    {
        list(regex);
        return 0;
    }
    catch (...)
    {
        Configuration::instance().operation_teardown();
        return 1;
    }
}

int
mamba_info()
{
    try
    {
        info();
        return 0;
    }
    catch (...)
    {
        Configuration::instance().operation_teardown();
        return 1;
    }
}

int
mamba_config_list()
{
    try
    {
        config_list();
        return 0;
    }
    catch (...)
    {
        Configuration::instance().operation_teardown();
        return 1;
    }
}

int
mamba_set_cli_config(const char* name, const char* value)
{
    try
    {
        Configuration::instance().at(name).set_cli_yaml_value(value);
        return 0;
    }
    catch (...)
    {
        Configuration::instance().operation_teardown();
        return 1;
    }
}

int
mamba_set_config(const char* name, const char* value)
{
    try
    {
        Configuration::instance().at(name).set_yaml_value(value);
        return 0;
    }
    catch (...)
    {
        Configuration::instance().operation_teardown();
        return 1;
    }
}

int
mamba_clear_config(const char* name)
{
    try
    {
        Configuration::instance().at(name).clear_values();
        return 0;
    }
    catch (...)
    {
        Configuration::instance().operation_teardown();
        return 1;
    }
}

int
mamba_use_conda_root_prefix(int force)
{
    try
    {
        use_conda_root_prefix(force);
        return 0;
    }
    catch (...)
    {
        Configuration::instance().operation_teardown();
        return 1;
    }
}
