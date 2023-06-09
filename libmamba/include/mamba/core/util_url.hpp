// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_CORE_UTIL_URL_HPP
#define MAMBA_CORE_UTIL_URL_HPP

#include <optional>
#include <string>
#include <vector>

namespace mamba
{
    std::string concat_scheme_url(const std::string& scheme, const std::string& location);
    std::string build_url(
        const std::optional<std::string>& auth,
        const std::string& scheme,
        const std::string& base,
        bool with_credential
    );
    void split_platform(
        const std::vector<std::string>& known_platforms,
        const std::string& url,
        const std::string& context_platform,
        std::string& cleaned_url,
        std::string& platform
    );

}  // namespace mamba

#endif  // MAMBA_CORE_UTIL_URL_HPP
