// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_VALIDATION_REPO_CHECKER_HPP
#define MAMBA_VALIDATION_REPO_CHECKER_HPP

#include <memory>
#include <string>
#include <string_view>

#include <nlohmann/json_fwd.hpp>

#include "mamba/fs/filesystem.hpp"

namespace mamba
{
    class Context;
}

namespace mamba::validation
{
    class RepoIndexChecker;
    class RootRole;
    class TimeRef;

    /**
     * Perform security check against a repository package index using cryptographic signatures.
     *
     * Relies on multiple roles defined in The Update Framework specification.
     */
    class RepoChecker
    {
    public:

        /**
         * Constructor.
         *
         * @param base_url Repository base URL
         * @param ref_path Path to the reference directory, hosting trusted root metadata
         * @param cache_path Path to the cache directory
         */
        RepoChecker(
            const Context& context,
            std::string base_url,
            fs::u8path ref_path,
            fs::u8path cache_path = ""
        );
        RepoChecker(RepoChecker&&) noexcept;
        ~RepoChecker();

        auto operator=(RepoChecker&&) noexcept -> RepoChecker&;

        // Forwarding to a ``RepoIndexChecker`` implementation
        void verify_index(const nlohmann::json& j) const;
        void verify_index(const fs::u8path& p) const;
        void
        verify_package(const nlohmann::json& signed_data, const nlohmann::json& signatures) const;
        void verify_package(const nlohmann::json& signed_data, std::string_view signatures) const;

        void generate_index_checker();

        auto cache_path() const -> const fs::u8path&;
        auto root_version() const -> std::size_t;

    private:

        std::unique_ptr<RepoIndexChecker> p_index_checker;
        std::reference_wrapper<const Context> m_context;

        std::string m_base_url;
        fs::u8path m_ref_path;
        fs::u8path m_cache_path;

        std::size_t m_root_version;

        auto ref_root() const -> fs::u8path;
        auto cached_root() const -> fs::u8path;

        auto initial_trusted_root() const -> fs::u8path;

        void persist_file(const fs::u8path& file_path);

        auto get_root_role(const TimeRef& time_reference) -> std::unique_ptr<RootRole>;
    };
}
#endif
