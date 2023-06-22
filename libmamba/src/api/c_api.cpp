// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <cassert>
#include <string>

#include "mamba/api/c_api.h"
#include "mamba/api/config.hpp"
#include "mamba/api/configuration.hpp"
#include "mamba/api/create.hpp"
#include "mamba/api/info.hpp"
#include "mamba/api/install.hpp"
#include "mamba/api/list.hpp"
#include "mamba/api/remove.hpp"
#include "mamba/api/update.hpp"
#include "mamba/core/context.hpp"


mamba::Context*
mamba_new_context()
{
    return new mamba::Context;
}

void
mamba_delete_context(mamba::Context* context)
{
    delete context;
}

mamba::Configuration*
mamba_new_configuration(mamba::Context* context)
{
    assert(context != nullptr);
    return new mamba::Configuration(*context);
}

void
mamba_delete_configuration(mamba::Configuration* config)
{
    delete config;
}

int
mamba_create(mamba::Configuration* config)
{
    assert(config != nullptr);
    try
    {
        create(*config);
        return 0;
    }
    catch (...)
    {
        config->operation_teardown();
        return 1;
    }
}

int
mamba_install(mamba::Configuration* config)
{
    assert(config != nullptr);
    try
    {
        install(*config);
        return 0;
    }
    catch (...)
    {
        config->operation_teardown();
        return 1;
    }
}

int
mamba_update(mamba::Configuration* config, int update_all)
{
    assert(config != nullptr);
    try
    {
        update(*config, update_all);
        return 0;
    }
    catch (...)
    {
        config->operation_teardown();
        return 1;
    }
}

int
mamba_remove(mamba::Configuration* config, int remove_all)
{
    assert(config != nullptr);
    try
    {
        remove(*config, remove_all);
        return 0;
    }
    catch (...)
    {
        config->operation_teardown();
        return 1;
    }
}

int
mamba_list(mamba::Configuration* config, const char* regex)
{
    assert(config != nullptr);
    try
    {
        list(*config, regex);
        return 0;
    }
    catch (...)
    {
        config->operation_teardown();
        return 1;
    }
}

int
mamba_info(mamba::Configuration* config)
{
    assert(config != nullptr);
    try
    {
        info(*config);
        return 0;
    }
    catch (...)
    {
        config->operation_teardown();
        return 1;
    }
}

int
mamba_config_list(mamba::Configuration* config)
{
    assert(config != nullptr);
    try
    {
        config_list(*config);
        return 0;
    }
    catch (...)
    {
        config->operation_teardown();
        return 1;
    }
}

int
mamba_set_cli_config(mamba::Configuration* config, const char* name, const char* value)
{
    assert(config != nullptr);
    try
    {
        config->at(name).set_cli_yaml_value(value);
        return 0;
    }
    catch (...)
    {
        config->operation_teardown();
        return 1;
    }
}

int
mamba_set_config(mamba::Configuration* config, const char* name, const char* value)
{
    assert(config != nullptr);
    try
    {
        config->at(name).set_yaml_value(value);
        return 0;
    }
    catch (...)
    {
        config->operation_teardown();
        return 1;
    }
}

int
mamba_clear_config(mamba::Configuration* config, const char* name)
{
    assert(config != nullptr);
    try
    {
        config->at(name).clear_values();
        return 0;
    }
    catch (...)
    {
        config->operation_teardown();
        return 1;
    }
}

int
mamba_use_conda_root_prefix(mamba::Configuration* config, int force)
{
    assert(config != nullptr);
    try
    {
        use_conda_root_prefix(*config, force);
        return 0;
    }
    catch (...)
    {
        config->operation_teardown();
        return 1;
    }
}
