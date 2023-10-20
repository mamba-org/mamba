// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <stdexcept>

#include <fmt/format.h>

#include "mamba/specs/authentication_info.hpp"
#include "mamba/util/string.hpp"

namespace mamba::specs
{

    auto AuthenticationDataBase::at_compatible(const key_type& key) -> mapped_type&
    {
        if (auto it = find_compatible(key); it != end())
        {
            return it->second;
        }
        throw std::out_of_range(fmt::format(R"(No entry for key "{}")", key));
    }

    auto AuthenticationDataBase::at_compatible(const key_type& key) const -> const mapped_type&
    {
        return const_cast<AuthenticationDataBase*>(this)->at_compatible(key);
    }

    auto AuthenticationDataBase::find_compatible(const key_type& key) -> iterator
    {
        // First we start by adding a final '/' if absent, so that "mamba.org/" can
        // be find by query "mamba.org".
        std::string maybe_key = {};
        if (!util::ends_with(key, '/'))
        {
            maybe_key = util::concat(key, '/');
        }
        else
        {
            maybe_key = key;
        }

        // We look for successsive parent dirs as queries with and without trailing '/';
        // "mamba.org/channel/" > "mamba.org/channel" > "mamba.org/" > "mamba.org"
        auto it = Base::find(maybe_key);
        while (it == Base::end())
        {
            const auto pos = maybe_key.rfind('/');
            if (maybe_key.empty() || (pos == std::string::npos))
            {
                // Nothing else to try
                break;
            }
            else if ((pos + 1) == maybe_key.size())
            {
                // Try again without final '/'
                maybe_key.erase(pos);
                it = Base::find(maybe_key);
            }
            else
            {
                // Try again without final element
                maybe_key.erase(pos + 1);
                it = Base::find(maybe_key);
            }
        }
        return it;
    }

    auto AuthenticationDataBase::find_compatible(const key_type& key) const -> const_iterator
    {
        return static_cast<const_iterator>(
            const_cast<AuthenticationDataBase*>(this)->find_compatible(key)
        );
    }

    auto AuthenticationDataBase::contains(const key_type& key) const -> bool
    {
        return find(key) != end();
    }

    auto AuthenticationDataBase::contains_compatible(const key_type& key) const -> bool
    {
        return find_compatible(key) != end();
    }
}
