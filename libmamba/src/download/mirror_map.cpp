// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include "mamba/download/mirror_map.hpp"

#include "mirror_impl.hpp"
#include "mamba/specs/channel.hpp"

namespace mamba::download
{
    mirror_map::mirror_map()
    {
        create_unique_mirror<PassThroughMirror>("");
    }

    std::size_t mirror_map::size() const
    {
        return m_mirrors.size();
    }

    bool mirror_map::has_mirrors(std::string_view mirror_name) const
    {
        auto find_it = m_mirrors.find(std::string(mirror_name));
        return find_it != m_mirrors.end() && !find_it->second.empty();
    }

    mirror_set_view mirror_map::get_mirrors(std::string_view mirror_name) const
    {
        auto find_it = m_mirrors.find(std::string(mirror_name));
        auto& mirrors = find_it != m_mirrors.end() ? find_it->second : m_empty_set;
        return util::view::all(const_cast<mirror_set&>(mirrors));
    }

    namespace
    {
        template <class It>
        bool contains(It first, It last, MirrorID id)
        {
            return std::find_if(first, last, [id](const auto& mirror) { return id == mirror->id(); })
                   != last;
        }
    }

    bool mirror_map::add_unique_mirror(
        std::string_view mirror_name,
        mirror_ptr mirror,
        specs::Channel::UrlPriorty priority
    )
    {
        auto insert_mirror = [&](auto& mirror_list) // assuming std::vector here
            {
                auto insert_it = priority == specs::Channel::UrlPriorty::high ? mirror_list.begin()
                                                                              : mirror_list.end();
                mirror_list.insert(insert_it, std::move(mirror));
            };

        auto find_it = m_mirrors.find(std::string(mirror_name));
        if (find_it != m_mirrors.end())
        {
            auto& mirrors = find_it->second;
            if (contains(mirrors.begin(), mirrors.end(), mirror->id()))
            {
                return false;
            }

            insert_mirror(mirrors);
        }
        else
        {
            insert_mirror(m_mirrors[std::string(mirror_name)]);
        }
        return true;
    }
}
