// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include "mamba/specs/authentication_info.hpp"
#include "mamba/util/string.hpp"

namespace mamba::specs
{
    auto URLWeakener::make_first_key(std::string_view key) const -> std::string
    {
        if (util::ends_with(key, '/'))
        {
            return std::string(key);
        }
        return util::concat(key, '/');
    }

    auto URLWeakener::weaken_key(std::string_view key) const -> std::optional<std::string_view>
    {
        const auto pos = key.rfind('/');
        if (key.empty() || (pos == std::string::npos))
        {
            // Nothing else to try
            return std::nullopt;
        }
        else if ((pos + 1) == key.size())
        {
            // Try again without final '/'
            return { key.substr(0, pos) };
        }
        else
        {
            // Try again without final element
            return { key.substr(0, pos + 1) };
        }
    }
}
