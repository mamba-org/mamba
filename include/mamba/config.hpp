// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_CONFIG_HPP
#define MAMBA_CONFIG_HPP

#include "mamba/context.hpp"
#include "environment.hpp"
#include "mamba/mamba_fs.hpp"
#include "mamba/output.hpp"
#include "mamba/util.hpp"

#include <yaml-cpp/yaml.h>


namespace mamba
{
    class Configurable
    {
    public:
        Configurable();

        const YAML::Node& get_config();
        std::vector<fs::path> get_sources();
        std::vector<fs::path> get_valid_sources();

        std::string dump(bool show_sources = false);

    protected:
        Configurable(std::string unique_source);

        static Context& ctx()
        {
            return Context::instance();
        };

        static bool has_config_extension(const std::string& file);
        static bool is_config_file(const fs::path& path);

        static YAML::Node load_rc_file(const fs::path& file);

        void load_config();
        void load_config_files();
        void update_sources();

        void build_prepend_seq(const std::vector<std::shared_ptr<YAML::Node>>& configs,
                               const std::string& key,
                               const std::map<std::shared_ptr<YAML::Node>, std::string>& node2src,
                               YAML::Node& result,
                               YAML::Node& sources);

        void build_override(const std::vector<std::shared_ptr<YAML::Node>>& configs,
                            const std::string& key,
                            const std::map<std::shared_ptr<YAML::Node>, std::string>& node2src,
                            YAML::Node& result,
                            YAML::Node& sources);

        static void print_scalar_with_sources(YAML::Emitter&, YAML::Node node, YAML::Node source);
        static void print_seq_with_sources(YAML::Emitter&, YAML::Node node, YAML::Node source);
        static void print_map_with_sources(YAML::Emitter&, YAML::Node node, YAML::Node source);

        std::vector<fs::path> sources;
        std::vector<fs::path> valid_sources;

        YAML::Node config;
        YAML::Node config_sources;
    };
}

#endif  // MAMBA_CONFIG_HPP
