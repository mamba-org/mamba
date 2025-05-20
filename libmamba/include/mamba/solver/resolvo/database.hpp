// Copyright (c) 2024, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_SOLVER_RESOLVO_DATABASE_HPP
#define MAMBA_SOLVER_RESOLVO_DATABASE_HPP

#include <string>
#include <unordered_map>
#include <vector>

#include <resolvo/resolvo.h>
#include <resolvo/resolvo_dependency_provider.h>
#include <resolvo/resolvo_pool.h>

#include "mamba/solver/database.hpp"
#include "mamba/specs/match_spec.hpp"
#include "mamba/specs/package_info.hpp"
#include "mamba/specs/version.hpp"

namespace std
{
    template <>
    struct hash<::resolvo::NameId>
    {
        size_t operator()(const ::resolvo::NameId& id) const noexcept
        {
            return static_cast<size_t>(id.id);
        }
    };

    template <>
    struct hash<::resolvo::VersionSetId>
    {
        size_t operator()(const ::resolvo::VersionSetId& id) const noexcept
        {
            return static_cast<size_t>(id.id);
        }
    };

    template <>
    struct hash<::resolvo::SolvableId>
    {
        size_t operator()(const ::resolvo::SolvableId& id) const noexcept
        {
            return static_cast<size_t>(id.id);
        }
    };

    template <>
    struct hash<::resolvo::StringId>
    {
        size_t operator()(const ::resolvo::StringId& id) const noexcept
        {
            return static_cast<size_t>(id.id);
        }
    };
}

namespace mamba::solver::resolvo
{
    // Create a template Pool class that maps a key to a set of values
    template <typename ID, typename T>
    struct Mapping
    {
        /**
         * Adds the value to the Mapping and returns its associated id. If the
         * value is already in the Mapping, returns the id associated with it.
         */
        ID alloc(T value)
        {
            if (auto element = value_to_id.find(value); element != value_to_id.end())
            {
                return element->second;
            }
            auto id = ID{ static_cast<uint32_t>(id_to_value.size()) };
            id_to_value[id] = value;
            value_to_id[value] = id;
            return id;
        }

        /**
         * Returns the value associated with the given id.
         */
        T operator[](ID id)
        {
            return id_to_value[id];
        }

        /**
         * Returns the id associated with the given value.
         */
        ID operator[](T value)
        {
            return value_to_id[value];
        }

        // Iterator for the Mapping
        auto begin()
        {
            return id_to_value.begin();
        }

        auto end()
        {
            return id_to_value.end();
        }

        auto begin() const
        {
            return id_to_value.begin();
        }

        auto end() const
        {
            return id_to_value.end();
        }

        auto cbegin()
        {
            return id_to_value.cbegin();
        }

        auto cend()
        {
            return id_to_value.cend();
        }

        auto cbegin() const
        {
            return id_to_value.cbegin();
        }

        auto cend() const
        {
            return id_to_value.cend();
        }

        auto find(T value)
        {
            return value_to_id.find(value);
        }

        auto begin_ids()
        {
            return value_to_id.begin();
        }

        auto end_ids()
        {
            return value_to_id.end();
        }

        auto begin_ids() const
        {
            return value_to_id.begin();
        }

        auto end_ids() const
        {
            return value_to_id.end();
        }

        auto cbegin_ids()
        {
            return value_to_id.cbegin();
        }

        auto cend_ids()
        {
            return value_to_id.cend();
        }

        auto cbegin_ids() const
        {
            return value_to_id.cbegin();
        }

        auto cend_ids() const
        {
            return value_to_id.cend();
        }

        auto size() const
        {
            return id_to_value.size();
        }

    private:

        std::unordered_map<T, ID> value_to_id;
        std::unordered_map<ID, T> id_to_value;
    };

    class Database final
        : public mamba::solver::Database
        , public ::resolvo::DependencyProvider
    {
    public:

        explicit Database(specs::ChannelResolveParams channel_params);
        ~Database() override = default;

        [[nodiscard]] auto channel_params() const -> const specs::ChannelResolveParams&;

        // Implementation of mamba::solver::Database interface
        void add_repo_from_repodata_json(
            const fs::u8path& filename,
            const std::string& repo_url,
            const std::string& channel_id,
            bool verify_artifacts = false
        ) override;

        void add_repo_from_packages(
            const std::vector<specs::PackageInfo>& packages,
            const std::string& repo_name,
            bool pip_as_python_dependency = false
        ) override;

        void set_installed_repo(const std::string& repo_name) override;

        // Implementation of resolvo::DependencyProvider interface
        ::resolvo::String display_solvable(::resolvo::SolvableId solvable) override;
        ::resolvo::String display_solvable_name(::resolvo::SolvableId solvable) override;
        ::resolvo::String
        display_merged_solvables(::resolvo::Slice<::resolvo::SolvableId> solvable) override;
        ::resolvo::String display_name(::resolvo::NameId name) override;
        ::resolvo::String display_version_set(::resolvo::VersionSetId version_set) override;
        ::resolvo::String display_string(::resolvo::StringId string) override;
        ::resolvo::NameId version_set_name(::resolvo::VersionSetId version_set_id) override;
        ::resolvo::NameId solvable_name(::resolvo::SolvableId solvable_id) override;
        ::resolvo::Candidates get_candidates(::resolvo::NameId package) override;
        void sort_candidates(::resolvo::Slice<::resolvo::SolvableId> solvables) override;
        ::resolvo::Vector<::resolvo::SolvableId> filter_candidates(
            ::resolvo::Slice<::resolvo::SolvableId> candidates,
            ::resolvo::VersionSetId version_set_id,
            bool inverse
        ) override;
        ::resolvo::Dependencies get_dependencies(::resolvo::SolvableId solvable_id) override;

        // Public access to pools and helper methods
        ::resolvo::VersionSetId alloc_version_set(std::string_view raw_match_spec);
        ::resolvo::SolvableId alloc_solvable(specs::PackageInfo package_info);
        std::pair<specs::Version, size_t>
        find_highest_version(::resolvo::VersionSetId version_set_id);

        // Pools for mapping between resolvo IDs and mamba types
        Mapping<::resolvo::NameId, ::resolvo::String> name_pool;
        Mapping<::resolvo::StringId, ::resolvo::String> string_pool;
        Mapping<::resolvo::VersionSetId, specs::MatchSpec> version_set_pool;
        Mapping<::resolvo::SolvableId, specs::PackageInfo> solvable_pool;

        bool has_package(const specs::MatchSpec& spec) override
        {
            auto candidates = get_candidates(name_pool.alloc(resolvo::String(spec.name().to_string()))
            );
            return !candidates.candidates.empty();
        }

    private:

        // Maps for quick lookups
        std::unordered_map<::resolvo::NameId, ::resolvo::Vector<::resolvo::SolvableId>> name_to_solvable;
        std::unordered_map<::resolvo::VersionSetId, std::pair<specs::Version, size_t>>
            version_set_to_max_version_and_track_features_numbers;

        specs::ChannelResolveParams m_channel_params;
    };
}

#endif  // MAMBA_SOLVER_RESOLVO_DATABASE_HPP
