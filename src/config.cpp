#include "mamba/config.hpp"

#include <algorithm>
#include <stdexcept>

namespace mamba
{
    Configurable::Configurable()
    {
        update_sources();
        load_config();
    }

    Configurable::Configurable(std::string unique_source)
    {
        sources.clear();
        if (looks_config_file(unique_source))
        {
            sources.push_back(unique_source);
            LOG_DEBUG << "Configuration found at '" << unique_source << "'";
        }
        else
        {
            LOG_DEBUG << "Configuration not found at '" << unique_source << "'";
        }
        load_config();
    }

    std::vector<fs::path> Configurable::get_sources()
    {
        return sources;
    }

    std::vector<fs::path> Configurable::get_valid_sources()
    {
        return valid_sources;
    }

    const YAML::Node& Configurable::get_config()
    {
        return config;
    }

    void Configurable::load_config()
    {
        config = YAML::Node();
        config_sources = YAML::Node();
        load_config_files();
    }

    YAML::Node Configurable::load_rc_file(const fs::path& file)
    {
        YAML::Node config;
        try
        {
            config = YAML::LoadFile(file);
        }
        catch (...)
        {
            LOG_ERROR << "Error in file " << file << " (Skipped)";
        }
        return config;
    }

    bool Configurable::has_config_extension(const std::string& file)
    {
#ifdef _WIN32
        std::string sep = "\\";
#else
        std::string sep = "/";
#endif
        return rsplit(file, sep, 1).back() == ".condarc"
               || rsplit(file, sep, 1).back() == ".mambarc" || ends_with(file, ".yml")
               || ends_with(file, ".yaml");
    }

    bool Configurable::looks_config_file(const fs::path& path)
    {
        return fs::exists(path) && (!fs::is_directory(path)) && has_config_extension(path.string());
    }

    void Configurable::load_config_files()
    {
        std::vector<std::shared_ptr<YAML::Node>> nodes;
        std::map<std::shared_ptr<YAML::Node>, std::string> node2src;

        valid_sources.clear();
        for (auto s : sources)
        {
            auto node = load_rc_file(s);
            if (!node.IsNull())
            {
                auto node_ptr = std::make_shared<YAML::Node>(node);
                nodes.push_back(node_ptr);
                node2src.insert(std::pair<std::shared_ptr<YAML::Node>, std::string>(
                    node_ptr, env::shrink_user(s).string()));
                valid_sources.push_back(s);
            }
        }

        if (nodes.size() > 0)
        {
            std::vector<std::string> prepend_seq_config = { "channels", "default_channels" };
            for (auto key : prepend_seq_config)
            {
                build_prepend_seq(nodes, key, node2src, config, config_sources);
            }

            std::vector<std::string> override_config = { "ssl_verify",
                                                         "auto_activate_base",
                                                         "override_channels_enabled",
                                                         "channel_alias",
                                                         "channel_priority" };
            for (auto key : override_config)
            {
                build_override(nodes, key, node2src, config, config_sources);
            }
        }
    }

    void Configurable::update_sources()
    {
        fs::path condarc_env_var = std::getenv("CONDARC") ? std::getenv("CONDARC") : "";
        fs::path mambarc_env_var = std::getenv("MAMBARC") ? std::getenv("MAMBARC") : "";

        std::vector<fs::path> possible_sources = { ctx().root_prefix / ".condarc",
                                                   ctx().root_prefix / "condarc",
                                                   ctx().root_prefix / "condarc.d",
                                                   ctx().root_prefix / ".mambarc",
                                                   env::home_directory() / ".conda/.condarc",
                                                   env::home_directory() / ".conda/condarc",
                                                   env::home_directory() / ".conda/condarc.d",
                                                   env::home_directory() / ".condarc",
                                                   env::home_directory() / ".mambarc",
                                                   ctx().target_prefix / ".condarc",
                                                   ctx().target_prefix / "condarc",
                                                   ctx().target_prefix / "condarc.d",
                                                   ctx().target_prefix / ".mambarc",
                                                   condarc_env_var,
                                                   mambarc_env_var };

        sources.clear();

        for (const fs::path& l : possible_sources)
        {
            if (looks_config_file(l))
            {
                sources.push_back(l);
                LOG_DEBUG << "Configuration found at '" << l.string() << "'";
            }
            else if (fs::is_directory(l))
            {
                for (fs::path p : fs::directory_iterator(l))
                {
                    if (looks_config_file(p))
                    {
                        sources.push_back(p);
                        LOG_DEBUG << "Configuration found at '" << p.string() << "'";
                    }
                    else
                    {
                        LOG_DEBUG << "Configuration not found at '" << p.string() << "'";
                    }
                }
            }
            else
            {
                LOG_DEBUG << "Configuration not found at '" << l.string() << "'";
            }
        }
        // Sort by precedence
        std::reverse(sources.begin(), sources.end());
    }

    void Configurable::build_prepend_seq(
        const std::vector<std::shared_ptr<YAML::Node>>& configs,
        const std::string& key,
        const std::map<std::shared_ptr<YAML::Node>, std::string>& node2src,
        YAML::Node& result,
        YAML::Node& sources)
    {
        for (auto n : configs)
        {
            if (!(*n)[key])
            {
                continue;
            }
            if (!(*n)[key].IsSequence())
            {
                LOG_ERROR << "Error in '" << node2src.at(n) << "' at key '" << key << "' (Skipped)";
                continue;
            }

            for (auto c : (*n)[key].as<std::vector<std::string>>())
            {
                bool push_value = true;

                if (result[key])
                {
                    auto string_vector = result[key].as<std::vector<std::string>>();
                    auto find_c = std::find(string_vector.begin(), string_vector.end(), c);
                    push_value = find_c == string_vector.end();
                }

                if (push_value)
                {
                    result[key].push_back(c);
                    sources[key].push_back(node2src.at(n));
                }
            }
        }
    }

    void Configurable::build_override(
        const std::vector<std::shared_ptr<YAML::Node>>& configs,
        const std::string& key,
        const std::map<std::shared_ptr<YAML::Node>, std::string>& node2src,
        YAML::Node& result,
        YAML::Node& sources)
    {
        for (auto n : configs)
        {
            if ((*n)[key])
            {
                if (!(*n)[key].IsScalar())
                {
                    LOG_ERROR << "Error in '" << node2src.at(n) << "' at key '" << key
                              << "' (Skipped)";
                    continue;
                }

                result[key] = (*n)[key];
                sources[key] = node2src.at(n);
                break;
            }
        }
    }

    void Configurable::print_scalar_with_sources(YAML::Emitter& out,
                                                 YAML::Node node,
                                                 YAML::Node source)
    {
        if (!node.IsScalar() || !source.IsScalar())
        {
            throw std::runtime_error("Invalid node/source");
        }
        out << node;
        out << YAML::Comment(source.as<std::string>());
    }

    void Configurable::print_seq_with_sources(YAML::Emitter& out,
                                              YAML::Node node,
                                              YAML::Node source)
    {
        if (!node.IsSequence() || !source.IsSequence())
        {
            throw std::runtime_error("Invalid node/source");
        }

        out << YAML::BeginSeq;
        for (std::size_t n = 0; n < node.size(); ++n)
        {
            if (node[n].IsScalar())
            {
                print_scalar_with_sources(out, node[n], source[n]);
            }
            if (node[n].IsSequence())
            {
                print_seq_with_sources(out, node[n], source[n]);
            }
            if (node[n].IsMap())
            {
                print_map_with_sources(out, node[n], source[n]);
            }
        }
        out << YAML::EndSeq;
    }

    void Configurable::print_map_with_sources(YAML::Emitter& out,
                                              YAML::Node node,
                                              YAML::Node source)
    {
        if (!node.IsMap() || !source.IsMap())
        {
            throw std::runtime_error("Invalid node/source");
        }

        out << YAML::BeginMap;
        for (auto n : node)
        {
            auto key = n.first.as<std::string>();
            out << YAML::Key << n.first;
            out << YAML::Value;

            if (n.second.IsScalar())
            {
                print_scalar_with_sources(out, n.second, source[key]);
            }
            if (n.second.IsSequence())
            {
                print_seq_with_sources(out, n.second, source[key]);
            }
            if (n.second.IsMap())
            {
                print_map_with_sources(out, n.second, source[key]);
            }
        }
        out << YAML::EndMap;
    }

    std::string Configurable::dump(bool show_source)
    {
        YAML::Emitter out;

        if (config.IsMap() && config_sources.IsMap())
        {
            if (show_source)
            {
                print_map_with_sources(out, config, config_sources);
            }
            else
            {
                out << config;
            }
        }

        return out.c_str();
    }
}
