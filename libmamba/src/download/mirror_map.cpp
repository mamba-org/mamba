// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include "mamba/download/mirror.hpp"
#include "mamba/download/mirror_map.hpp"

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
        template <class It>
        bool contains(It first, It last, MirrorID id)
        {
            return std::find_if(first, last, [id](const auto& mirror) { return id == mirror->id(); })
                   != last;
        }
    }

    bool mirror_map::add_unique_mirror(std::string_view mirror_name, mirror_ptr mirror)
    {
        auto find_it = m_mirrors.find(std::string(mirror_name));
        if (find_it != m_mirrors.end())
        {
            auto& mirrors = find_it->second;
            if (contains(mirrors.begin(), mirrors.end(), mirror->id()))
            {
                return false;
            }
            mirrors.push_back(std::move(mirror));
        }
        else
        {
            m_mirrors[std::string(mirror_name)].push_back(std::move(mirror));
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
