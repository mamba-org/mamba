// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include "mamba/configuration.hpp"

#include <algorithm>
#include <stdexcept>

namespace mamba
{
    namespace detail
    {
        void ssl_verify_hook(std::string& value)
        {
            if ((value == "false") || (value == "0") || (value == "<false>"))
            {
                value = "<false>";
                return;
            }

            auto& cacert
                = Configuration::instance().at("cacert_path").template get_wrapped<std::string>();
            if (!cacert.value().empty())
            {
                value = cacert.value();
                return;
            }
            else
            {
                if ((value == "true") || (value == "1") || (value == "<true>"))
                {
                    if (on_linux)
                    {
                        std::array<std::string, 6> cert_locations{
                            "/etc/ssl/certs/ca-certificates.crt",  // Debian/Ubuntu/Gentoo etc.
                            "/etc/pki/tls/certs/ca-bundle.crt",    // Fedora/RHEL 6
                            "/etc/ssl/ca-bundle.pem",              // OpenSUSE
                            "/etc/pki/tls/cacert.pem",             // OpenELEC
                            "/etc/pki/ca-trust/extracted/pem/tls-ca-bundle.pem",  // CentOS/RHEL 7
                            "/etc/ssl/cert.pem",                                  // Alpine Linux
                        };

                        for (const auto& loc : cert_locations)
                        {
                            if (fs::exists(loc))
                            {
                                value = loc;
                                return;
                            }
                        }

                        LOG_ERROR << "ssl_verify is enabled but no ca certificates found";
                        exit(1);
                    }
                    else
                    {
                        value = "<system>";
                        return;
                    }
                }
            }
        };

        void always_link_hook(bool&)
        {
            auto& config = Configuration::instance();
            auto& always_softlink = config.at("always_softlink")
                                        .compute_config(ConfigurationLevel::kFile, false)
                                        .value<bool>();
            auto& always_copy = config.at("always_copy")
                                    .compute_config(ConfigurationLevel::kFile, false)
                                    .value<bool>();

            if (always_softlink && always_copy)
            {
                throw std::runtime_error(
                    "'always-softlink' and 'always-copy' options are exclusive.");
            }
        }
    }

    Configuration::Configuration()
    {
        set_configurables();
    }

    Configuration& Configuration::instance()
    {
        static Configuration config;
        return config;
    }

    void Configuration::set_configurables()
    {
        auto& ctx = Context::instance();

        // Channels
        insert(Configurable("channels", &ctx.channels)
                   .group("Channels")
                   .set_env_var_name("CONDA_CHANNELS")
                   .description("Define the list of channels")
                   .long_description(unindent(R"(
                    The list of channels where the packages will be searched for.
                    See also 'channel_priority'.)")));

        insert(Configurable("channel_alias", &ctx.channel_alias)
                   .group("Channels")
                   .set_env_var_name()
                   .description("The prepended url location to associate with channel names"));

        insert(Configurable("default_channels", &ctx.default_channels)
                   .group("Channels")
                   .set_env_var_name()
                   .description("Default channels used")
                   .long_description(unindent(R"(
                    The list of channel names and/or urls used for the 'defaults'
                    multichannel.)")));

        insert(Configurable("override_channels_enabled", &ctx.override_channels_enabled)
                   .group("Channels")
                   .set_env_var_name()
                   .description("Permit use of the --overide-channels command-line flag"));

        // Network
        insert(Configurable("local_repodata_ttl", &ctx.local_repodata_ttl)
                   .group("Network")
                   .description("Repodata time-to-live")
                   .long_description(unindent(R"(
                        For a value of 0, always fetch remote repodata (HTTP 304
                        responses respected).
                        For a value of 1, respect the HTTP Cache-Control max-age header.
                        Any other positive integer values is the number of seconds to
                        locally cache repodata before checking the remote server for
                        an update.)")));

        insert(Configurable("ssl_verify", &ctx.ssl_verify)
                   .group("Network")
                   .set_env_var_name()
                   .description("Verify SSL certificates for HTTPS requests")
                   .long_description(unindent(R"(
                        'ssl_verify' can be either an empty string (regular SSL verification),
                        the string "<false>" to indicate no SSL verification, or a path to
                        a directory with cert files, or a cert file..)"))
                   .set_post_build_hook(detail::ssl_verify_hook));

        insert(Configurable("cacert_path", std::string(""))
                   .group("Network")
                   .set_env_var_name()
                   .description("Path (file or directory) SSL certificate(s)")
                   .long_description(unindent(R"(
                        Path (file or directory) SSL certificate(s) to use whe
                        'ssl_verify' in turned on but not set with path to certs.
                        WARNING: overrides 'ssl_verify' if provided and 'ssl_verify'
                        also contains a path to SSL certificates.)")));

        insert(Configurable("ssl_no_revoke", &ctx.ssl_no_revoke)
                   .group("Network")
                   .set_env_var_name()
                   .description("SSL certificate revocation checks")
                   .long_description(unindent(R"(
                        This option tells curl to disable certificate revocation checks.
                        It's only working for Windows back-end.
                        WARNING: this option loosens the SSL security.)")));
        // Solver
        insert(Configurable("channel_priority", &ctx.channel_priority)
                   .group("Solver")
                   .set_env_var_name()
                   .description("Define the channel priority ('strict' or 'disabled')")
                   .long_description(unindent(R"(
                    Accepts values of 'strict' and 'disabled'. The default
                    value is 'strict'. With strict channel priority, packages in lower
                    priority channels are not considered if a package with the same name
                    appears in a higher priority channel.
                    With channel priority disabled, package version takes precedence, and the
                    configured priority of channels is used only to break ties. In
                    previous versions of conda, this parameter was configured as either
                    True or False. True is now an alias to 'flexible'.)")));

        insert(Configurable("pinned_packages", &ctx.pinned_packages)
                   .group("Solver")
                   .set_env_var_name()
                   .description("A list of package specs to pin for every environment resolution"));

        // Link & Install
        insert(Configurable("allow_softlinks", &ctx.allow_softlinks)
                   .group("Link & Install")
                   .set_env_var_name()
                   .description("Allow to use soft-links when hard-links are not possible")
                   .long_description(unindent(R"(
                    Allow to use soft-links (symlinks) when hard-links are not possible,
                    such as when installing on a different filesystem than the one that
                    the package cache is on.)")));

        insert(Configurable("always_copy", &ctx.always_copy)
                   .group("Link & Install")
                   .set_env_var_name()
                   .set_post_build_hook(detail::always_link_hook)
                   .description("Use copy instead of hard-link")
                   .long_description(unindent(R"(
                    Register a preference that files be copied into a prefix during
                    install rather than hard-linked.)")));

        insert(Configurable("always_softlink", &ctx.always_softlink)
                   .group("Link & Install")
                   .set_env_var_name()
                   .description("Use soft-link instead of hard-link")
                   .long_description(unindent(R"(
                    Register a preference that files be soft-linked (symlinked) into a
                    prefix during install rather than hard-linked. The link source is the
                    package cache from where the package is being linked.
                    !WARNING: Using this option can result in corruption of long-lived
                    environments due to broken links (deleted cache).)")));

        insert(Configurable("safety_checks", &ctx.safety_checks)
                   .group("Link & Install")
                   .set_env_var_name()
                   .description("Safety checks policy ('enabled', 'warn', or 'disabled')")
                   .long_description(unindent(R"(
                    Enforce available safety guarantees during package installation. The
                    value must be one of 'enabled', 'warn', or 'disabled'.)")));

        insert(Configurable("extra_safety_checks", &ctx.extra_safety_checks)
                   .group("Link & Install")
                   .set_env_var_name()
                   .description("Run extra verifications on packages")
                   .long_description(unindent(R"(
                    Spend extra time validating package contents. Currently, runs sha256
                    verification on every file within each package during installation.)")));

        // Output, Prompt and Flow
        insert(Configurable("auto_activate_base", &ctx.auto_activate_base)
                   .group("Output, Prompt and Flow Control")
                   .set_env_var_name()
                   .description("Automatically activate the base env")
                   .long_description(unindent(R"(
                    Automatically activate the base environment during shell
                    initialization.)")));
    }

    void Configuration::reset_configurables()
    {
        m_config.clear();
        m_config_order.clear();
        set_configurables();
    }

    std::vector<std::pair<std::string, std::vector<ConfigurableInterface*>>>
    Configuration::get_grouped_config()
    {
        std::map<std::string, std::vector<ConfigurableInterface*>> map;
        std::vector<std::pair<std::string, std::vector<ConfigurableInterface*>>> res;
        std::vector<std::string> group_order;

        for (auto& name : m_config_order)
        {
            auto& c = m_config.at(name);

            if (map.count(c.group()) == 0)
            {
                group_order.push_back(c.group());
            }
            map[c.group()].push_back(&c);
        }

        for (auto& g : group_order)
        {
            res.push_back({ g, map.at(g) });
        }

        return res;
    }

    void Configuration::set_possible_sources()
    {
        auto& ctx = Context::instance();

        fs::path condarc_env_var = std::getenv("CONDARC") ? std::getenv("CONDARC") : "";
        fs::path mambarc_env_var = std::getenv("MAMBARC") ? std::getenv("MAMBARC") : "";

        if (on_mac || on_linux)
        {
            m_sources = { "/etc/conda/.condarc",       "/etc/conda/condarc",
                          "/etc/conda/condarc.d/",     "/etc/conda/.mambarc",
                          "/var/lib/conda/.condarc",   "/var/lib/conda/condarc",
                          "/var/lib/conda/condarc.d/", "/var/lib/conda/.mambarc" };
        }
        else
        {
            m_sources = { "C:\\ProgramData\\conda\\.condarc",
                          "C:\\ProgramData\\conda\\condarc",
                          "C:\\ProgramData\\conda\\condarc.d",
                          "C:\\ProgramData\\conda\\.mambarc" };
        }

        std::vector<std::string> not_os_spe = { ctx.root_prefix / ".condarc",
                                                ctx.root_prefix / "condarc",
                                                ctx.root_prefix / "condarc.d",
                                                ctx.root_prefix / ".mambarc",
                                                env::home_directory() / ".conda/.condarc",
                                                env::home_directory() / ".conda/condarc",
                                                env::home_directory() / ".conda/condarc.d",
                                                env::home_directory() / ".condarc",
                                                env::home_directory() / ".mambarc",
                                                ctx.target_prefix / ".condarc",
                                                ctx.target_prefix / "condarc",
                                                ctx.target_prefix / "condarc.d",
                                                ctx.target_prefix / ".mambarc",
                                                condarc_env_var,
                                                mambarc_env_var };

        m_sources.insert(m_sources.end(), not_os_spe.begin(), not_os_spe.end());

        // Sort by precedence
        std::reverse(m_sources.begin(), m_sources.end());
    }

    void Configuration::set_possible_sources(fs::path source)
    {
        m_sources = std::vector<fs::path>({ source });
    }

    void Configuration::set_possible_sources(std::vector<fs::path> sources)
    {
        m_sources = sources;
    }

    void Configuration::load()
    {
        set_possible_sources();
        update_sources();
        load_config_files();
        for (auto& c : m_config)
        {
            c.second.compute_config();
            c.second.set_context();
        }
    }

    void Configuration::load(fs::path unique_source)
    {
        if (!fs::exists(unique_source))
        {
            LOG_ERROR << "Unique RC file specified does not exist: " << unique_source.string();
            exit(1);
        }
        set_possible_sources(unique_source);
        update_sources();
        load_config_files();
        for (auto& c : m_config)
        {
            c.second.compute_config();
            c.second.set_context();
        }
    }

    void Configuration::load(std::vector<fs::path> sources)
    {
        set_possible_sources(sources);
        update_sources();
        load_config_files();
        for (auto& c : m_config)
        {
            c.second.compute_config();
            c.second.set_context();
        }
    }

    std::vector<fs::path> Configuration::sources()
    {
        return m_sources;
    }

    std::vector<fs::path> Configuration::valid_sources()
    {
        return m_valid_sources;
    }

    std::map<std::string, ConfigurableInterface>& Configuration::config()
    {
        return m_config;
    }

    ConfigurableInterface& Configuration::at(const std::string& name)
    {
        try
        {
            return m_config.at(name);
        }
        catch (const std::out_of_range& e)
        {
            LOG_FATAL << "Configurable '" << name << "' does not exists";
            throw e;
        }
    }

    YAML::Node Configuration::load_rc_file(const fs::path& file)
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

    void Configuration::load_config_files()
    {
        std::vector<std::shared_ptr<YAML::Node>> rc_yaml_values;
        std::map<std::shared_ptr<YAML::Node>, std::string> sources_map;

        m_valid_sources.clear();
        for (auto s : m_sources)
        {
            auto node = load_rc_file(s);
            if (!node.IsNull())
            {
                auto node_ptr = std::make_shared<YAML::Node>(node);
                rc_yaml_values.push_back(node_ptr);
                sources_map.insert({ node_ptr, env::shrink_user(s).string() });
                m_valid_sources.push_back(s);
            }
        }

        if (rc_yaml_values.size() > 0)
        {
            for (auto& it : m_config)
            {
                auto& key = it.first;
                auto& c = it.second;

                for (auto s : rc_yaml_values)
                {
                    if (!(*s)[key] || (*s)[key].IsNull())
                    {
                        continue;
                    }
                    c.add_rc_value((*s)[key], sources_map.at(s));
                }
            }
        }
    }

    void Configuration::update_sources()
    {
        std::vector<fs::path> sources;

        for (const fs::path& l : m_sources)
        {
            if (detail::is_config_file(l))
            {
                sources.push_back(l);
                LOG_DEBUG << "Configuration found at '" << l.string() << "'";
            }
            else if (fs::is_directory(l))
            {
                for (fs::path p : fs::directory_iterator(l))
                {
                    if (detail::is_config_file(p))
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
                if (!l.empty())
                    LOG_DEBUG << "Configuration not found at '" << l.string() << "'";
            }
        }

        m_sources = sources;
    }

    std::string Configuration::dump(bool show_values,
                                    bool show_source,
                                    bool show_defaults,
                                    bool show_sections,
                                    bool show_desc,
                                    bool long_desc,
                                    std::vector<std::string> names)
    {
        bool first_config = true;
        YAML::Emitter out;
        // out.SetNullFormat(YAML::EMITTER_MANIP::LowerNull); // TODO: switch from ~ to null

        for (auto& group_it : get_grouped_config())
        {
            auto& group_name = group_it.first;
            auto& configs = group_it.second;
            bool first_group_config = true;

            for (auto& c : configs)
            {
                auto is_required = std::find(names.begin(), names.end(), c->name()) != names.end();
                if (!names.empty() && !is_required)
                {
                    continue;
                }

                if ((c->rc_configurable() && c->configured()) || is_required || show_defaults)
                {
                    if (show_desc || long_desc)
                    {
                        if (show_sections && first_group_config)
                        {
                            if (!first_config)
                                out << YAML::Newline << YAML::Newline;
                            detail::print_group_title(out, group_name);
                        }

                        if (!first_config || (first_config && show_sections))
                            out << YAML::Newline << YAML::Newline;

                        out << YAML::Comment(c->name()) << YAML::Newline;
                        if (long_desc)
                        {
                            out << YAML::Comment(prepend(c->long_description(), "  ", "  "));
                        }
                        else
                        {
                            out << YAML::Comment(prepend(c->description(), "  ", "  "));
                        }
                    }

                    if (show_values)
                    {
                        if (first_config)
                            out << YAML::BeginMap;
                        out << YAML::Key << c->name();
                        out << YAML::Value;
                        detail::print_configurable(out, *c, show_source);
                    }

                    first_config = false;
                    first_group_config = false;
                }
            }
        }
        if (show_values && !first_config)
            out << YAML::EndMap;

        return out.c_str();
    }

    namespace detail
    {
        bool has_config_name(const std::string& file)
        {
            return fs::path(file).filename() == ".condarc" || fs::path(file).filename() == "condarc"
                   || fs::path(file).filename() == ".mambarc"
                   || fs::path(file).filename() == "mambarc" || ends_with(file, ".yml")
                   || ends_with(file, ".yaml");
        }

        bool is_config_file(const fs::path& path)
        {
            return fs::exists(path) && (!fs::is_directory(path)) && has_config_name(path.string());
        }

        void print_scalar_node(YAML::Emitter& out,
                               YAML::Node value,
                               YAML::Node source,
                               bool show_source)
        {
            if (!value.IsScalar())
            {
                throw std::runtime_error("Invalid scalar value");
            }
            out << value;

            if (show_source)
            {
                if (source.IsScalar())
                {
                    out << YAML::Comment("'" + source.as<std::string>() + "'");
                }
                else
                {
                    auto srcs = source.as<std::vector<std::string>>();
                    std::string comment = "'" + srcs.at(0) + "'";
                    for (std::size_t i = 1; i < srcs.size(); ++i)
                    {
                        comment += " > '" + srcs.at(i) + "'";
                    }
                    out << YAML::Comment(comment);
                }
            }
        }

        void print_seq_node(YAML::Emitter& out,
                            YAML::Node value,
                            YAML::Node source,
                            bool show_source)
        {
            if (!value.IsSequence())
            {
                throw std::runtime_error("Invalid sequence value");
            }

            if (value.size() > 0)
            {
                out << YAML::BeginSeq;
                for (std::size_t n = 0; n < value.size(); ++n)
                {
                    if (value[n].IsScalar())
                    {
                        print_scalar_node(out, value[n], source[n], show_source);
                    }
                    if (value[n].IsSequence())
                    {
                        print_seq_node(out, value[n], source[n], show_source);
                    }
                    if (value[n].IsMap())
                    {
                        print_map_node(out, value[n], source[n], show_source);
                    }
                }
                out << YAML::EndSeq;
            }
            else
            {
                out << YAML::_Null();
                if (show_source)
                    out << YAML::Comment("'default'");
            }
        }

        void print_map_node(YAML::Emitter& out,
                            YAML::Node value,
                            YAML::Node source,
                            bool show_source)
        {
            if (!value.IsMap())
            {
                throw std::runtime_error("Invalid map value");
            }

            out << YAML::BeginMap;
            for (auto n : value)
            {
                auto key = n.first.as<std::string>();
                out << YAML::Key << n.first;
                out << YAML::Value;

                if (n.second.IsScalar())
                {
                    print_scalar_node(out, n.second, source[key], show_source);
                }
                if (n.second.IsSequence())
                {
                    print_seq_node(out, n.second, source[key], show_source);
                }
                if (n.second.IsMap())
                {
                    print_map_node(out, n.second, source[key], show_source);
                }
            }
            out << YAML::EndMap;
        }

        void print_configurable(YAML::Emitter& out,
                                const ConfigurableInterface& config,
                                bool show_source)
        {
            auto value = config.yaml_value();
            auto source = config.source();

            if (value.IsScalar())
            {
                print_scalar_node(out, value, source, show_source);
            }
            if (value.IsSequence())
            {
                print_seq_node(out, value, source, show_source);
            }
            if (value.IsMap())
            {
                print_map_node(out, value, source, show_source);
            }
        }

        void print_group_title(YAML::Emitter& out, const std::string& name)
        {
            auto group_title = name + " Configuration";
            auto blk_size = 52 - group_title.size();
            int prepend_blk = blk_size / 2;
            int append_blk = blk_size - prepend_blk;

            out << YAML::Comment(std::string(54, '#')) << YAML::Newline;
            out << YAML::Comment("#" + std::string(prepend_blk, ' ') + group_title
                                 + std::string(append_blk, ' ') + "#")
                << YAML::Newline;
            out << YAML::Comment(std::string(54, '#'));
        }
    }
}
