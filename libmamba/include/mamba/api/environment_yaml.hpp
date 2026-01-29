// Copyright (c) 2026, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_API_ENVIRONMENT_YAML_HPP
#define MAMBA_API_ENVIRONMENT_YAML_HPP

#include <iosfwd>
#include <map>
#include <string>
#include <vector>

#include "mamba/fs/filesystem.hpp"

// Include install.hpp for other_pkg_mgr_spec definition (needed by yaml_file_contents)
#include "mamba/api/install.hpp"

namespace mamba
{
    class Context;
    class PrefixData;

    /** Options for converting prefix data to YAML export contents */
    struct PrefixToYamlOptions
    {
        bool no_builds = false;
        bool ignore_channels = false;
        bool include_md5 = false;
    };

    namespace detail
    {
        // yaml_file_contents is now defined here instead of install.hpp
        struct yaml_file_contents
        {
            std::string name;
            std::string prefix;
            std::vector<std::string> dependencies, channels;
            std::map<std::string, std::string> variables;
            std::vector<other_pkg_mgr_spec> others_pkg_mgrs_specs;
        };
    }

    // Convert PrefixData to yaml_file_contents
    detail::yaml_file_contents prefix_to_yaml_contents(
        const PrefixData& prefix_data,
        const Context& ctx,
        const std::string& env_name = "",
        const PrefixToYamlOptions& options = {}
    );

    // Write yaml_file_contents to output stream
    void yaml_contents_to_stream(const detail::yaml_file_contents& contents, std::ostream& out);

    // Write yaml_file_contents to YAML file
    void
    yaml_contents_to_file(const detail::yaml_file_contents& contents, const fs::u8path& yaml_file_path);

    // Read YAML file to yaml_file_contents
    detail::yaml_file_contents file_to_yaml_contents(
        const Context& ctx,
        const std::string& yaml_file,
        const std::string& platform,
        bool use_uv = false
    );
}

#endif
