// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_API_INSTALL_HPP
#define MAMBA_API_INSTALL_HPP

#include <iosfwd>
#include <map>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>
#include <yaml-cpp/yaml.h>

#include "mamba/core/history.hpp"
#include "mamba/core/util.hpp"
#include "mamba/fs/filesystem.hpp"
#include "mamba/solver/request.hpp"

namespace mamba
{
    class Context;
    class ChannelContext;
    class Configuration;
    class PrefixData;
    class MultiPackageCache;

    namespace specs
    {
        class PackageInfo;
    }

    void install(Configuration& config);

    void install_revision(Configuration& config, std::size_t revision);

    void install_specs(
        Context& ctx,
        ChannelContext& channel_context,
        const Configuration& config,
        const std::vector<std::string>& specs,
        bool create_env = false,
        bool remove_prefix_on_failure = false
    );

    auto create_install_request(  //
        PrefixData& prefix_data,
        std::vector<std::string> specs,
        bool freeze_installed
    ) -> solver::Request;

    void add_pins_to_request(
        solver::Request& request,
        const Context& ctx,
        PrefixData& prefix_data,
        std::vector<std::string> specs,
        bool no_pin,
        bool no_py_pin
    );

    void print_request_pins_to(const solver::Request& request, std::ostream& out);

    void install_explicit_specs(
        Context& ctx,
        ChannelContext& channel_context,
        const std::vector<std::string>& specs,
        bool create_env = false,
        bool remove_prefix_on_failure = false
    );
    void install_lockfile_specs(
        Context& ctx,
        ChannelContext& channel_context,
        const std::string& lockfile_specs,
        const std::vector<std::string>& categories,
        bool create_env = false,
        bool remove_prefix_on_failure = false
    );

    namespace detail
    {
        void create_target_directory(const Context& context, const fs::u8path prefix);

        void create_empty_target(
            const Context& context,
            const fs::u8path& prefix,
            const std::map<std::string, std::string>& env_vars,
            bool no_env
        );

        void populate_state_file(
            const fs::u8path& prefix,
            const std::map<std::string, std::string>& env_vars,
            bool no_env
        );

        void file_specs_hook(Configuration& config, std::vector<std::string>& file_specs);

        void channels_hook(Configuration& config, std::vector<std::string>& channels);

        bool
        download_explicit(const std::vector<specs::PackageInfo>& pkgs, MultiPackageCache& pkg_caches);

        struct other_pkg_mgr_spec
        {
            std::string pkg_mgr;
            std::vector<std::string> deps;
            std::string cwd;
        };

        bool operator==(const other_pkg_mgr_spec& s1, const other_pkg_mgr_spec& s2);

        struct yaml_file_contents
        {
            std::string name;
            std::vector<std::string> dependencies, channels;
            std::map<std::string, std::string> variables;
            std::vector<other_pkg_mgr_spec> others_pkg_mgrs_specs;
        };

        bool eval_selector(const std::string& selector, const std::string& platform);

        yaml_file_contents read_yaml_file(
            const Context& ctx,
            const std::string& yaml_file,
            const std::string& platform,
            bool use_uv
        );

        inline void to_json(nlohmann::json&, const other_pkg_mgr_spec&)
        {
        }

        void install_revision(Context& ctx, ChannelContext& channel_context, std::size_t revision);
    }

}

namespace YAML
{
    template <>
    struct convert<mamba::detail::other_pkg_mgr_spec>
    {
        static Node encode(const mamba::detail::other_pkg_mgr_spec& /*rhs*/)
        {
            return Node();
        }

        static bool decode(const Node& /*node*/, mamba::detail::other_pkg_mgr_spec& /*rhs*/)
        {
            return true;
        }
    };
}

#endif
