// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_CORE_CHANNEL_HPP
#define MAMBA_CORE_CHANNEL_HPP

#include "mamba/core/validate.hpp"

#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace mamba
{
    void load_tokens();

    // accessors public
    class Channel
    {
    public:
        const std::string& scheme() const;
        const std::string& location() const;
        const std::string& name() const;
        const std::vector<std::string>& platforms() const;
        const std::optional<std::string>& auth() const;
        const std::optional<std::string>& token() const;
        const std::optional<std::string>& package_filename() const;
        const std::string& canonical_name() const;
        const validate::RepoChecker& repo_checker() const;

        std::string base_url() const;
        // The pairs consist of (platform,url)
        std::vector<std::pair<std::string, std::string>> platform_urls(bool with_credential
                                                                       = true) const;
        std::vector<std::string> urls(bool with_credential = true) const;

    protected:
        Channel(const std::string& scheme,
                const std::string& location,
                const std::string& name,
                const std::optional<std::string>& auth = {},
                const std::optional<std::string>& token = {},
                const std::optional<std::string>& package_filename = {},
                const std::optional<std::string>& canonical_name = {});

        std::string m_scheme;
        std::string m_location;
        std::string m_name;
        std::vector<std::string> m_platforms;
        std::optional<std::string> m_auth;
        std::optional<std::string> m_token;
        std::optional<std::string> m_package_filename;
        mutable std::optional<std::string> m_canonical_name;
        mutable validate::RepoChecker m_repo_checker;
    };

    // public
    const Channel& make_channel(const std::string& value);

    // public
    std::vector<std::string> get_channel_urls(const std::vector<std::string>& channel_names,
                                              bool with_credential = true);

    // XXX unused, but should be in python API according to docs
    void check_whitelist(const std::vector<std::string>& urls);
}  // namespace mamba

#endif
