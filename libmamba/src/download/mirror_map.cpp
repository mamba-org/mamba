// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include "mamba/download/mirror.hpp"
#include "mamba/download/mirror_map.hpp"
#include "mamba/specs/channel.hpp"

#include "mirror_impl.hpp"

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
        template <std::ranges::input_range R>
        bool contains_mirror(const R& r, const MirrorID& id)
        {
            return std::ranges::any_of(r, [&](const auto& mirror) { return mirror->id() == id; });
        }
    }

    bool mirror_map::add_unique_mirror(
        std::string_view mirror_name,
        mirror_ptr mirror,
        specs::Channel::UrlPriority priority
    )
    {
        auto insert_mirror = [&](mirror_set& mirror_list)
        {
            auto insert_it = priority == specs::Channel::UrlPriority::high ? mirror_list.begin()
                                                                           : mirror_list.end();
            mirror_list.insert(insert_it, std::move(mirror));
        };

        auto find_it = m_mirrors.find(std::string(mirror_name));
        if (find_it != m_mirrors.end())
        {
            auto& mirrors = find_it->second;
            if (contains_mirror(mirrors, mirror->id()))
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

    void mirror_map::add_mirrors_from(const mirror_map& other, std::string_view mirror_name)
    {
        for (const auto& mirror : other.get_mirrors(mirror_name))
        {
            auto new_mirror = make_mirror(mirror->id().to_string());
            if (new_mirror)
            {
                add_unique_mirror(mirror_name, std::move(new_mirror));
            }
        }
    }
}
