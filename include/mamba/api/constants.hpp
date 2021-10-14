#ifndef MAMBA_API_CONSTANTS_HPP
#define MAMBA_API_CONSTANTS_HPP

namespace mamba
{
    int const MAMBA_NO_PREFIX_CHECK = 1 << 0;
    int const MAMBA_ALLOW_EXISTING_PREFIX = 1 << 1;
    int const MAMBA_ALLOW_MISSING_PREFIX = 1 << 2;
    int const MAMBA_ALLOW_NOT_ENV_PREFIX = 1 << 3;
    int const MAMBA_EXPECT_EXISTING_PREFIX = 1 << 4;

    int const MAMBA_NOT_ALLOW_EXISTING_PREFIX = 0;
    int const MAMBA_NOT_ALLOW_MISSING_PREFIX = 0;
    int const MAMBA_NOT_ALLOW_NOT_ENV_PREFIX = 0;
    int const MAMBA_NOT_EXPECT_EXISTING_PREFIX = 0;

    int const MAMBA_CONF_FORCE_COMPUTE = 1 << 0;
    int const MAMBA_CONF_DISABLE_HOOK = 1 << 1;

    int const MAMBA_FORCE = 1 << 0;
    int const MAMBA_REMOVE_FORCE = MAMBA_FORCE;
    int const MAMBA_REMOVE_PRUNE = 1 << 1;
    int const MAMBA_REMOVE_ALL = 1 << 2;
}

#endif
