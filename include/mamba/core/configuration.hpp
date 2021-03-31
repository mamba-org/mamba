// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_CORE_CONFIGURATION_HPP
#define MAMBA_CORE_CONFIGURATION_HPP

#include "mamba/core/context.hpp"
#include "mamba/core/environment.hpp"
#include "mamba/core/mamba_fs.hpp"
#include "mamba/core/output.hpp"
#include "mamba/core/util.hpp"

#include <yaml-cpp/yaml.h>

#include <functional>


namespace YAML
{
    template <>
    struct convert<mamba::VerificationLevel>
    {
        static Node encode(const mamba::VerificationLevel& rhs)
        {
            if (rhs == mamba::VerificationLevel::kDisabled)
            {
                return Node("disabled");
            }
            else if (rhs == mamba::VerificationLevel::kWarn)
            {
                return Node("warn");
            }
            else if (rhs == mamba::VerificationLevel::kEnabled)
            {
                return Node("enabled");
            }
            else
            {
                return Node();
            }
        }

        static bool decode(const Node& node, mamba::VerificationLevel& rhs)
        {
            if (!node.IsScalar())
            {
                return false;
            }

            auto str = node.as<std::string>();

            if (str == "enabled")
            {
                rhs = mamba::VerificationLevel::kEnabled;
            }
            else if (str == "warn")
            {
                rhs = mamba::VerificationLevel::kWarn;
            }
            else if (str == "disabled")
            {
                rhs = mamba::VerificationLevel::kDisabled;
            }
            else
            {
                throw std::runtime_error(
                    "Invalid 'VerificationLevel', should be in {'enabled', 'warn', 'disabled'}");
            }

            return true;
        }
    };

    template <>
    struct convert<mamba::ChannelPriority>
    {
        static Node encode(const mamba::ChannelPriority& rhs)
        {
            if (rhs == mamba::ChannelPriority::kStrict)
            {
                return Node("strict");
            }
            else if (rhs == mamba::ChannelPriority::kFlexible)
            {
                return Node("flexible");
            }
            else if (rhs == mamba::ChannelPriority::kDisabled)
            {
                return Node("disabled");
            }
            else
            {
                return Node();
            }
        }

        static bool decode(const Node& node, mamba::ChannelPriority& rhs)
        {
            if (!node.IsScalar())
            {
                return false;
            }

            auto str = node.as<std::string>();

            if (str == "strict")
            {
                rhs = mamba::ChannelPriority::kStrict;
            }
            else if ((str == "flexible") || (str == "true"))
            {
                rhs = mamba::ChannelPriority::kFlexible;
            }
            else if (str == "disabled")
            {
                rhs = mamba::ChannelPriority::kDisabled;
            }
            else
            {
                return false;
            }

            return true;
        }
    };

    template <>
    struct convert<fs::path>
    {
        static Node encode(const fs::path& rhs)
        {
            return Node(rhs.string());
        }

        static bool decode(const Node& node, fs::path& rhs)
        {
            if (!node.IsScalar())
            {
                return false;
            }

            rhs = fs::path(node.as<std::string>());
            return true;
        }
    };
}


namespace mamba
{
    class ConfigurableInterface;

    namespace detail
    {
        template <class T>
        struct cli_config
        {
            using storage_type = T;

            cli_config(const storage_type& value)
                : m_value(value){};

            bool defined()
            {
                return true;
            };
            T value()
            {
                return m_value;
            };

            storage_type m_value;
        };


        template <>
        struct cli_config<bool>
        {
            using storage_type = int;

            cli_config(const int& value)
                : m_value(value){};

            bool defined()
            {
                return m_value != 0;
            };
            bool value()
            {
                if (m_value == 1)
                {
                    return true;
                }
                else if (m_value == -1)
                {
                    return false;
                }
                throw std::runtime_error(
                    "Invalid boolean storage, should be {'-1': false, '0': undefined, '1': true}");
            };

            int m_value = 0;
        };


        template <>
        struct cli_config<std::vector<std::string>>
        {
            using value_type = std::vector<std::string>;
            using storage_type = value_type;

            cli_config(const storage_type& value)
                : m_value(value){};

            bool defined()
            {
                return !m_value.empty();
            };
            value_type value()
            {
                return m_value;
            };

            storage_type m_value = {};
        };


        template <>
        struct cli_config<std::string>
        {
            using storage_type = std::string;

            cli_config(const std::string& value)
                : m_value(value){};

            bool defined()
            {
                return !m_value.empty();
            };
            std::string value()
            {
                return m_value;
            };

            std::string m_value = "";
        };


        template <>
        struct cli_config<fs::path>
        {
            using storage_type = fs::path;

            cli_config(const fs::path& value)
                : m_value(value){};

            bool defined()
            {
                return !m_value.empty();
            };
            fs::path value()
            {
                return m_value;
            };

            fs::path m_value = "";
        };


        template <>
        struct cli_config<std::size_t>
        {
            using storage_type = std::ptrdiff_t;

            cli_config(const storage_type& value)
                : m_value(value){};

            bool defined()
            {
                return m_value > -1;
            };
            std::size_t value()
            {
                return m_value;
            };

            storage_type m_value = -1;
        };


        template <>
        struct cli_config<VerificationLevel>
        {
            using storage_type = std::string;

            cli_config(const std::string& value)
                : m_value(value){};

            bool defined()
            {
                return !m_value.empty();
            };
            VerificationLevel value()
            {
                return YAML::Node(m_value).as<VerificationLevel>();
            };

            std::string m_value = "";
        };

        template <>
        struct cli_config<ChannelPriority>
        {
            using storage_type = std::string;

            cli_config(const std::string& value)
                : m_value(value){};

            bool defined()
            {
                return !m_value.empty();
            };
            ChannelPriority value()
            {
                return YAML::Node(m_value).as<ChannelPriority>();
            };

            std::string m_value = "";
        };

        bool has_config_name(const std::string& file);

        bool is_config_file(const fs::path& path);

        void override_config_scalar(const std::vector<YAML::Node>& configs,
                                    const std::string& key,
                                    const std::map<YAML::Node*, std::string>& sources_map,
                                    std::vector<YAML::Node>& values,
                                    std::vector<std::string>& sources);

        void print_configurable(YAML::Emitter& out,
                                const ConfigurableInterface& config,
                                bool show_source);
        void print_group_title(YAML::Emitter& out, const std::string& name);
        void print_scalar_node(YAML::Emitter&,
                               YAML::Node value,
                               YAML::Node source,
                               bool show_source);
        void print_seq_node(YAML::Emitter&, YAML::Node value, YAML::Node source, bool show_source);
        void print_map_node(YAML::Emitter&, YAML::Node value, YAML::Node source, bool show_source);
    }

    namespace detail
    {
        template <class T>
        struct Source
        {
            static std::vector<std::string> default_value(const T&)
            {
                return std::vector<std::string>({ "default" });
            };

            static void merge(const std::map<std::string, T>& values,
                              const std::vector<std::string>& sources,
                              T& value,
                              std::vector<std::string>& source);

            static T convert_env_var(const std::string& name);
        };

        template <class T>
        struct Source<std::vector<T>>
        {
            static std::vector<std::string> default_value(const std::vector<T>& init)
            {
                return std::vector<std::string>(init.size(), "default");
            };

            static void merge(const std::map<std::string, std::vector<T>>& values,
                              const std::vector<std::string>& sources,
                              std::vector<T>& value,
                              std::vector<std::string>& source);

            static std::vector<T> convert_env_var(const std::string& name);
        };

        template <class T>
        void Source<T>::merge(const std::map<std::string, T>& values,
                              const std::vector<std::string>& sources,
                              T& value,
                              std::vector<std::string>& source)
        {
            bool value_set = false;
            source.clear();

            for (auto& s : sources)
            {
                auto& val = values.at(s);

                if (!value_set)
                {
                    value = val;
                    value_set = true;
                }
                source.push_back(s);
            }
        }

        template <class T>
        T Source<T>::convert_env_var(const std::string& name)
        {
            return YAML::Load(env::get(name)).as<T>();
        }

        template <class T>
        void Source<std::vector<T>>::merge(const std::map<std::string, std::vector<T>>& values,
                                           const std::vector<std::string>& sources,
                                           std::vector<T>& value,
                                           std::vector<std::string>& source)
        {
            value.clear();
            source.clear();

            for (auto& s : sources)
            {
                auto& vec = values.at(s);
                for (auto& v : vec)
                {
                    auto find_v = std::find(value.begin(), value.end(), v);

                    if (find_v == value.end())
                    {
                        value.push_back(v);
                        source.push_back(s);
                    }
                }
            }
        }

        template <class T>
        std::vector<T> Source<std::vector<T>>::convert_env_var(const std::string& name)
        {
            return YAML::Load("[" + env::get(name) + "]").as<std::vector<T>>();
        }
    }


    enum class ConfigurationLevel
    {
        kNone = 0,
        kCli = 1,
        kEnvVar = 2,
        kFile = 3
    };


    template <class T>
    class Configurable
    {
        using hook_type = std::function<void(T&)>;

    public:
        using self_type = Configurable<T>;
        using cli_config_type = detail::cli_config<T>;
        using cli_config_storage_type = typename detail::cli_config<T>::storage_type;

        Configurable(const std::string& name, T* context);

        Configurable(const std::string& name, const T& init);

        const std::string& name() const;

        const std::string& group() const;

        const std::string& description() const;

        const std::string& long_description() const;

        const T& value();

        T cli_value();

        const std::vector<T>& values();

        YAML::Node yaml_value() const;

        const std::vector<std::string>& source();

        const std::vector<std::string>& sources();

        bool rc_configured() const;

        bool rc_configurable() const;

        bool cli_configured() const;

        bool env_var_configured() const;

        bool configured() const;

        self_type& add_rc_value(const T& value, const std::string& source);

        self_type& add_rc_values(const std::map<std::string, T>& mapped_values,
                                 const std::vector<std::string>& sources);

        self_type& set_env_var_name(const std::string& name = "MAMBA_");

        self_type& group(const std::string& group);

        self_type& rc_configurable(bool value);

        self_type& description(const std::string& desc);

        self_type& long_description(const std::string& desc);

        self_type& set_post_build_hook(hook_type hook);

        self_type& set_context();

        self_type& set_cli_value(const cli_config_storage_type& value);

        cli_config_storage_type& set_cli_config(const cli_config_storage_type& init);

        self_type& compute_config(const ConfigurationLevel& level = ConfigurationLevel::kFile,
                                  bool hook_enabled = true);

    private:
        std::string m_name;
        std::string m_group = "Default";
        std::string m_description = "No description provided";
        std::string m_long_description = "";
        std::string m_env_var = "";

        bool m_rc_configurable = true;
        bool m_rc_configured = false;

        std::map<std::string, T> m_rc_values, m_values;
        std::vector<std::string> m_rc_sources, m_sources;

        T m_value;
        std::vector<std::string> m_source;

        std::shared_ptr<cli_config_type> p_cli_config = 0;
        T* p_context = 0;
        hook_type p_hook;
    };

    /*********************
     * Configurable impl *
     *********************/

    template <class T>
    Configurable<T>::Configurable(const std::string& name, T* context)
        : m_name(name)
        , m_value(*context)
        , m_source(detail::Source<T>::default_value(*context))
        , p_context(context){};

    template <class T>
    Configurable<T>::Configurable(const std::string& name, const T& init)
        : m_name(name)
        , m_value(init)
        , m_source(detail::Source<T>::default_value(init)){};

    template <class T>
    const std::string& Configurable<T>::name() const
    {
        return m_name;
    };

    template <class T>
    const std::string& Configurable<T>::group() const
    {
        return m_group;
    };

    template <class T>
    const std::string& Configurable<T>::description() const
    {
        return m_description;
    };

    template <class T>
    const std::string& Configurable<T>::long_description() const
    {
        return m_long_description.empty() ? m_description : m_long_description;
    };

    template <class T>
    const T& Configurable<T>::value()
    {
        return m_value;
    };

    template <class T>
    T Configurable<T>::cli_value()
    {
        if (!cli_configured())
            throw std::runtime_error("Trying to get unset CLI value of '" + m_name + "'");

        return p_cli_config->value();
    };

    template <class T>
    const std::vector<T>& Configurable<T>::values()
    {
        return m_values;
    };

    template <class T>
    YAML::Node Configurable<T>::yaml_value() const
    {
        return YAML::Node(m_value);
    };

    template <class T>
    const std::vector<std::string>& Configurable<T>::source()
    {
        return m_source;
    };

    template <class T>
    const std::vector<std::string>& Configurable<T>::sources()
    {
        return m_sources;
    };

    template <class T>
    bool Configurable<T>::rc_configured() const
    {
        return m_rc_configured && !Context::instance().no_rc;
    };

    template <class T>
    bool Configurable<T>::rc_configurable() const
    {
        return m_rc_configurable;
    };

    template <class T>
    bool Configurable<T>::cli_configured() const
    {
        return (p_cli_config != NULL) && p_cli_config->defined();
    };

    template <class T>
    bool Configurable<T>::env_var_configured() const
    {
        return !m_env_var.empty() && !Context::instance().no_env && !env::get(m_env_var).empty();
    };

    template <class T>
    bool Configurable<T>::configured() const
    {
        return rc_configured() || cli_configured() || env_var_configured();
    };

    template <class T>
    auto Configurable<T>::set_context() -> self_type&
    {
        if (p_context != NULL)
        {
            *p_context = m_value;
        }
        return *this;
    };

    template <class T>
    auto Configurable<T>::set_cli_value(const cli_config_storage_type& value) -> self_type&
    {
        if (p_cli_config == NULL)
        {
            throw std::runtime_error("Configurable '" + m_name + "' does not have CLI set.");
        }
        p_cli_config->m_value = value;

        return *this;
    };

    template <class T>
    auto Configurable<T>::compute_config(const ConfigurationLevel& level, bool hook_enabled)
        -> self_type&
    {
        auto& ctx = Context::instance();
        m_sources.clear();
        m_values.clear();

        if (cli_configured() && (level >= ConfigurationLevel::kCli))
        {
            m_sources.push_back("CLI");
            m_values.insert({ "CLI", p_cli_config->value() });
        }

        if (env_var_configured() && !ctx.no_env && (level >= ConfigurationLevel::kEnvVar))
        {
            m_sources.push_back(m_env_var);
            m_values.insert({ m_env_var, detail::Source<T>::convert_env_var(m_env_var) });
        }

        if (rc_configured() && !ctx.no_rc && (level >= ConfigurationLevel::kFile))
        {
            m_sources.insert(m_sources.end(), m_rc_sources.begin(), m_rc_sources.end());
            m_values.insert(m_rc_values.begin(), m_rc_values.end());
        }

        if (!m_sources.empty())
            detail::Source<T>::merge(m_values, m_sources, m_value, m_source);

        if (hook_enabled && (p_hook != NULL))
            p_hook(m_value);

        return *this;
    }

    template <class T>
    auto Configurable<T>::set_env_var_name(const std::string& name) -> self_type&
    {
        if (name.compare("MAMBA_") == 0)
        {
            m_env_var = "MAMBA_" + to_upper(m_name);
        }
        else
        {
            m_env_var = name;
        }
        return *this;
    }

    template <class T>
    auto Configurable<T>::group(const std::string& group) -> self_type&
    {
        m_group = group;
        return *this;
    };

    template <class T>
    auto Configurable<T>::rc_configurable(bool value) -> self_type&
    {
        m_rc_configurable = value;
        return *this;
    };

    template <class T>
    auto Configurable<T>::description(const std::string& desc) -> self_type&
    {
        m_description = desc;
        return *this;
    };

    template <class T>
    auto Configurable<T>::long_description(const std::string& desc) -> self_type&
    {
        m_long_description = desc;
        return *this;
    };

    template <class T>
    auto Configurable<T>::add_rc_value(const T& value, const std::string& source) -> self_type&
    {
        m_rc_sources.push_back(source);
        m_rc_values[source] = value;
        m_rc_configured = true;
        return *this;
    };

    template <class T>
    auto Configurable<T>::add_rc_values(const std::map<std::string, T>& mapped_values,
                                        const std::vector<std::string>& sources) -> self_type&
    {
        assert(mapped_values.size() == sources.size());
        m_rc_sources.insert(m_rc_sources.end(), sources.begin(), sources.end());
        m_rc_values.insert(mapped_values.begin(), mapped_values.end());
        m_rc_configured = true;
        return *this;
    };

    template <class T>
    auto Configurable<T>::set_post_build_hook(hook_type hook) -> self_type&
    {
        p_hook = hook;
        return *this;
    }

    template <class T>
    auto Configurable<T>::set_cli_config(const cli_config_storage_type& init)
        -> cli_config_storage_type&
    {
        if (p_cli_config == NULL)
        {
            p_cli_config = std::make_shared<cli_config_type>(init);
        }
        else
        {
            p_cli_config->m_value = init;
        }
        return p_cli_config->m_value;
    }

    /********************************
     * Configuration interface impl *
     * ******************************/

    class ConfigurableInterface
    {
        class WrapperBase
        {
        public:
            virtual ~WrapperBase(){};

            virtual const std::string& name() const = 0;

            virtual const std::string& group() const = 0;

            virtual const std::string& description() const = 0;

            virtual const std::string& long_description() const = 0;

            virtual YAML::Node yaml_value() const = 0;

            virtual YAML::Node cli_yaml_value() const = 0;

            virtual YAML::Node source() const = 0;

            virtual bool configured() const = 0;

            virtual bool rc_configured() const = 0;

            virtual bool env_var_configured() const = 0;

            virtual bool cli_configured() const = 0;

            virtual bool rc_configurable() const = 0;

            virtual void add_rc_value(const YAML::Node& value, const std::string& source) = 0;

            virtual void add_rc_values(const std::map<std::string, YAML::Node>& values,
                                       const std::vector<std::string>& sources)
                = 0;

            virtual void set_cli_value(const YAML::Node& value) = 0;

            virtual void set_context() = 0;

            virtual void set_env_var_name(const std::string& name) = 0;

            virtual void group(const std::string& name) = 0;

            virtual void description(const std::string& desc) = 0;

            virtual void long_description(const std::string& desc) = 0;

            virtual void compute_config(const ConfigurationLevel& level, bool hook_enabled) = 0;
        };

        template <class T>
        class Wrapper : public WrapperBase
        {
        public:
            using wrapped_type = Configurable<T>;
            using cli_config_storage_type = typename detail::cli_config<T>::storage_type;

            Wrapper(std::unique_ptr<wrapped_type> config)
            {
                p_wrapped = std::move(config);
            };

            virtual ~Wrapper(){};

            wrapped_type& get_wrapped()
            {
                return *p_wrapped;
            };

            const std::string& name() const
            {
                return p_wrapped->name();
            };

            const std::string& group() const
            {
                return p_wrapped->group();
            };

            YAML::Node yaml_value() const
            {
                return p_wrapped->yaml_value();
            };

            YAML::Node cli_yaml_value() const
            {
                return YAML::Node(p_wrapped->cli_value());
            };

            YAML::Node source() const
            {
                return YAML::Node(p_wrapped->source());
            };

            const std::string& description() const
            {
                return p_wrapped->description();
            };

            const std::string& long_description() const
            {
                return p_wrapped->long_description();
            };

            bool configured() const
            {
                return p_wrapped->configured();
            };

            bool rc_configured() const
            {
                return p_wrapped->rc_configured();
            };

            bool env_var_configured() const
            {
                return p_wrapped->env_var_configured();
            };

            bool cli_configured() const
            {
                return p_wrapped->cli_configured();
            };

            bool rc_configurable() const
            {
                return p_wrapped->rc_configurable();
            };

            void add_rc_value(const YAML::Node& value, const std::string& source)
            {
                try
                {
                    p_wrapped->add_rc_value(value.as<T>(), source);
                }
                catch (const YAML::Exception& e)
                {
                    LOG_ERROR << "Bad conversion of configurable '" << name() << "' from source '"
                              << source << "'";
                }
            };

            void add_rc_values(const std::map<std::string, YAML::Node>& values,
                               const std::vector<std::string>& sources)
            {
                std::map<std::string, T> converted_values;
                for (auto& y : values)
                {
                    converted_values.insert({ y.first, y.second.as<T>() });
                }
                p_wrapped->add_rc_values(converted_values, sources);
            };

            void set_cli_value(const YAML::Node& value)
            {
                p_wrapped->set_cli_value(value.as<cli_config_storage_type>());
            };

            void set_context()
            {
                p_wrapped->set_context();
            };

            void set_env_var_name(const std::string& name)
            {
                p_wrapped->set_env_var_name(name);
            };

            void group(const std::string& name)
            {
                p_wrapped->group(name);
            };

            void description(const std::string& desc)
            {
                p_wrapped->description(desc);
            };

            void long_description(const std::string& desc)
            {
                p_wrapped->long_description(desc);
            };

            void compute_config(const ConfigurationLevel& level, bool hook_enabled)
            {
                p_wrapped->compute_config(level, hook_enabled);
            };

        private:
            std::unique_ptr<wrapped_type> p_wrapped;
        };

        std::unique_ptr<WrapperBase> p_impl;

    public:
        template <class T>
        ConfigurableInterface(Configurable<T> configurable)
            : p_impl(
                std::make_unique<Wrapper<T>>(std::make_unique<Configurable<T>>(configurable))){};

        template <class T>
        ConfigurableInterface(const std::string& name, T* context)
            : p_impl(
                std::make_unique<Wrapper<T>>(std::make_unique<Configurable<T>>(name, context))){};

        template <class T>
        ConfigurableInterface(const std::string& name, const T& init)
            : p_impl(std::make_unique<Wrapper<T>>(std::make_unique<Configurable<T>>(name, init))){};

        template <class T>
        Configurable<T>& get_wrapped()
        {
            auto& derived = dynamic_cast<Wrapper<T>&>(*p_impl);
            return derived.get_wrapped();
        };

        const std::string& name() const
        {
            return p_impl->name();
        };

        const std::string& group() const
        {
            return p_impl->group();
        };

        const std::string& description() const
        {
            return p_impl->description();
        };

        const std::string& long_description() const
        {
            return p_impl->long_description();
        };

        template <class T>
        const T& value()
        {
            return get_wrapped<T>().value();
        };

        YAML::Node yaml_value() const
        {
            return p_impl->yaml_value();
        };

        YAML::Node cli_yaml_value() const
        {
            return p_impl->cli_yaml_value();
        };

        YAML::Node source() const
        {
            return p_impl->source();
        };

        bool configured() const
        {
            return p_impl->configured();
        };

        bool rc_configured() const
        {
            return p_impl->rc_configured();
        };

        bool env_var_configured() const
        {
            return p_impl->env_var_configured();
        };

        bool cli_configured() const
        {
            return p_impl->cli_configured();
        };

        bool rc_configurable() const
        {
            return p_impl->rc_configurable();
        };

        void add_rc_value(const YAML::Node& value, const std::string& source)
        {
            p_impl->add_rc_value(value, source);
        };

        void add_rc_values(const std::map<std::string, YAML::Node>& values,
                           const std::vector<std::string>& sources)
        {
            p_impl->add_rc_values(values, sources);
        };

        void set_cli_value(const YAML::Node& value)
        {
            p_impl->set_cli_value(value);
        };

        ConfigurableInterface& set_context()
        {
            p_impl->set_context();
            return *this;
        };

        void set_env_var_name(const std::string& name = "MAMBA_")
        {
            p_impl->set_env_var_name(name);
        };

        ConfigurableInterface& group(const std::string& name)
        {
            p_impl->group(name);
            return *this;
        };

        ConfigurableInterface& description(const std::string& desc)
        {
            p_impl->description(desc);
            return *this;
        };

        ConfigurableInterface& long_description(const std::string& desc)
        {
            p_impl->long_description(desc);
            return *this;
        };

        ConfigurableInterface& compute_config(const ConfigurationLevel& level
                                              = ConfigurationLevel::kFile,
                                              bool hook_enabled = true)
        {
            p_impl->compute_config(level, hook_enabled);
            return *this;
        };
    };

    /*****************
     * Configuration *
     * ***************/

    class Configuration
    {
    public:
        static Configuration& instance();

        std::map<std::string, ConfigurableInterface>& config();
        ConfigurableInterface& at(const std::string& name);
        std::vector<std::pair<std::string, std::vector<ConfigurableInterface*>>>
        get_grouped_config();

        std::vector<fs::path> sources();
        std::vector<fs::path> valid_sources();

        void load();
        void load(fs::path source);
        void load(std::vector<fs::path> sources);

        std::string dump(bool show_values = true,
                         bool show_sources = false,
                         bool show_defaults = false,
                         bool show_sections = false,
                         bool show_desc = false,
                         bool long_desc = false,
                         std::vector<std::string> names = {});

        template <class T>
        typename detail::cli_config<T>::storage_type& link_cli_option(
            const std::string& name, const typename detail::cli_config<T>::storage_type& init);

        template <class T>
        Configurable<T>& insert(Configurable<T> configurable);

        void reset_configurables();

    protected:
        Configuration();
        ~Configuration() = default;

        void set_configurables();

        static YAML::Node load_rc_file(const fs::path& file);

        void set_possible_sources();
        void set_possible_sources(fs::path source);
        void set_possible_sources(std::vector<fs::path> sources);
        void update_sources();
        void load_config_files();

        std::vector<fs::path> m_sources;
        std::vector<fs::path> m_valid_sources;

        std::map<std::string, ConfigurableInterface> m_config;
        std::vector<std::string> m_config_order;
    };

    template <class T>
    Configurable<T>& Configuration::insert(Configurable<T> configurable)
    {
        std::string name = configurable.name();
        if (m_config.count(name) == 0)
        {
            m_config.insert({ name, ConfigurableInterface(std::move(configurable)) });
            m_config_order.push_back(name);
        }
        else
        {
            LOG_DEBUG << "Redefinition of '" << name << "' skipped";
        }

        return m_config.at(name).get_wrapped<T>();
    }

    template <class T>
    auto Configuration::link_cli_option(const std::string& name,
                                        const typename detail::cli_config<T>::storage_type& init) ->
        typename detail::cli_config<T>::storage_type&
    {
        auto& config = at(name).get_wrapped<T>();
        return config.set_cli_config(init);
    }
}

#endif  // MAMBA_CONFIG_HPP
