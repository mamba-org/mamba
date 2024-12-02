// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_DOWNLOAD_MIRROR_MAP_HPP
#define MAMBA_DOWNLOAD_MIRROR_MAP_HPP

#include <algorithm>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>

#include "mamba/download/mirror.hpp"
#include "mamba/util/iterator.hpp"

namespace mamba::download
{
    using mirror_ptr = std::unique_ptr<Mirror>;
    using mirror_set = std::vector<mirror_ptr>;
    using mirror_set_view = util::view::range_all<mirror_set>;

    class mirror_map
    {
    public:

        mirror_map();
        ~mirror_map() = default;

        mirror_map(const mirror_map&) = delete;
        mirror_map& operator=(const mirror_map&) = delete;

        mirror_map(mirror_map&&) = default;
        mirror_map& operator=(mirror_map&&) = default;

        std::size_t size() const;

        // Returns true if there are registered mirrors stored here, false if none are.
        bool has_mirrors(std::string_view mirror_name) const;

        // Get a list of unique mirrors if existing for the provided mirror name, or an empty list
        // otherwise.
        mirror_set_view get_mirrors(std::string_view mirror_name) const;

        // Stores a provided Mirror IFF no other mirror is already registered with the same id for
        // the specified mirror name. Returns true if the mirror has been stored, false otherwise.
        bool add_unique_mirror(std::string_view mirror_name, mirror_ptr mirror);

        // Creates, stores and returns a new instance of `MirrorType` created with `args` IFF no
        // other mirror is already registered with the same id for the specified mirror name,
        // returns null otherwise.
        template <class MirrorType, class... Args>
        auto create_unique_mirror(const std::string& mirror_name, Args&&... args) -> MirrorType&;

    private:

        using map_type = std::unordered_map<std::string, mirror_set>;

        mirror_set m_empty_set;
        map_type m_mirrors;
    };

    template <class MirrorType, class... Args>
    auto mirror_map::create_unique_mirror(const std::string& mirror_name, Args&&... args)
        -> MirrorType&
    {
        static_assert(std::is_base_of_v<Mirror, MirrorType>);

        const auto new_id = MirrorType::make_id(args...);

        auto& mirrors = m_mirrors[mirror_name];
        auto iter = std::find_if(
            mirrors.begin(),
            mirrors.end(),
            [new_id](auto& mirror) { return new_id == mirror->id(); }
        );
        if (iter != mirrors.end())
        {
            return dynamic_cast<MirrorType&>(**iter);
        }

        auto mirror = std::make_unique<MirrorType>(std::forward<Args>(args)...);
        mirrors.push_back(std::move(mirror));
        return dynamic_cast<MirrorType&>(*(mirrors.back()));
    }
}

#endif
