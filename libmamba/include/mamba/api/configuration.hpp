// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_API_CONFIGURATION_HPP
#define MAMBA_API_CONFIGURATION_HPP

#include "mamba/core/context.hpp"
#include "mamba/core/environment.hpp"
#include "mamba/core/mamba_fs.hpp"
#include "mamba/core/output.hpp"
#include "mamba/core/util.hpp"
#include "mamba/api/constants.hpp"

#include <yaml-cpp/yaml.h>

#include "spdlog/spdlog.h"

#include <functional>


#define CONTEXT_DEBUGGING                                                                          \
    if (Configuration::instance().at("print_context_only").value<bool>())                          \
    {                                                                                              \
        Context::instance().debug_print();                                                         \
        exit(0);                                                                                   \
    }
#define CONFIG_DEBUGGING                                                                           \
    if (Configuration::instance().at("print_config_only").value<bool>())                           \
    {                                                                                              \
        int dump_opts                                                                              \
            = MAMBA_SHOW_CONFIG_VALUES | MAMBA_SHOW_CONFIG_SRCS | MAMBA_SHOW_ALL_CONFIGS;          \
        std::cout << Configuration::instance().dump(dump_opts) << std::endl;                       \
        exit(0);                                                                                   \
    }

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

    template <>
    struct convert<spdlog::level::level_enum>
    {
        static Node encode(const spdlog::level::level_enum& rhs)
        {
            using namespace spdlog::level;

            return Node(to_string_view(rhs).data());
        }

        static bool decode(const Node& node, spdlog::level::level_enum& rhs)
        {
            using namespace spdlog::level;

            auto name = node.as<std::string>();
            auto it = std::find(std::begin(level_string_views), std::end(level_string_views), name);
            if (it != std::end(level_string_views))
            {
                rhs = static_cast<level_enum>(it - std::begin(level_string_views));
                return true;
            }

            LOG_ERROR
                << "Invalid log level, should be in {'critical', 'error', 'warning', 'info', 'debug', 'trace', 'off'} but is '"
                << name << "'";
            return false;
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

        template <>
        struct cli_config<spdlog::level::level_enum>
        {
            using value_type = spdlog::level::level_enum;
            using storage_type = std::string;

            cli_config(const storage_type& value)
                : m_value(value){};

            bool defined()
            {
                return !m_value.empty();
            };
            value_type value()
            {
                return YAML::Node(m_value).as<spdlog::level::level_enum>();
            };

            storage_type m_value = "";
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

            static T deserialize(const std::string& value);

            static bool is_sequence();
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

            static std::vector<T> deserialize(const std::string& value);

            static bool is_sequence();
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
        T Source<T>::deserialize(const std::string& value)
        {
            if (value.empty())
            {
                return YAML::Node("").as<T>();
            }
            else
            {
                return YAML::Load(value).as<T>();
            }
        }

        template <class T>
        bool Source<T>::is_sequence()
        {
            return false;
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
        std::vector<T> Source<std::vector<T>>::deserialize(const std::string& value)
        {
            return YAML::Load("[" + value + "]").as<std::vector<T>>();
        }

        template <class T>
        bool Source<std::vector<T>>::is_sequence()
        {
            return true;
        }
    }


    enum class ConfigurationLevel
    {
        kApi = 0,
        kCli = 1,
        kEnvVar = 2,
        kFile = 3,
        kDefault = 4
    };


    enum class RCConfigLevel
    {
        kSystemDir = 0,
        kRootPrefix = 1,
        kHomeDir = 2,
        kTargetPrefix = 3
    };
}  // mamba

namespace YAML
{
    template <>
    struct convert<mamba::RCConfigLevel>
    {
        static Node encode(const mamba::RCConfigLevel& rhs)
        {
            using namespace spdlog::level;

            switch (rhs)
            {
                case mamba::RCConfigLevel::kHomeDir:
                    return Node("HomeDir");
                case mamba::RCConfigLevel::kRootPrefix:
                    return Node("RootPrefix");
                case mamba::RCConfigLevel::kSystemDir:
                    return Node("SystemDir");
                case mamba::RCConfigLevel::kTargetPrefix:
                    return Node("TargetPrefix");
                default:
                    break;
            }
            return Node();
        }

        static bool decode(const Node& node, mamba::RCConfigLevel& rhs)
        {
            if (!node.IsScalar())
                return false;

            auto str = node.as<std::string>();

            if (str == "HomeDir")
                rhs = mamba::RCConfigLevel::kHomeDir;
            else if (str == "RootPrefix")
                rhs = mamba::RCConfigLevel::kRootPrefix;
            else if (str == "SystemDir")
                rhs = mamba::RCConfigLevel::kSystemDir;
            else if (str == "TargetPrefix")
                rhs = mamba::RCConfigLevel::kTargetPrefix;
            else
                return false;

            return true;
        }
    };
}  // YAML

namespace mamba
{
    template <class T>
    class Configurable
    {
        using value_hook_type = std::function<T()>;
        using post_merge_hook_type = std::function<void(T&)>;
        using post_context_hook_type = std::function<void()>;

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

        const T& value() const;

        T& value();

        T cli_value();

        const std::vector<T>& values();

        YAML::Node yaml_value() const;

        const std::vector<std::string>& source();

        const std::vector<std::string>& sources();

        const std::set<std::string>& needed() const;

        const std::set<std::string>& implied() const;

        bool rc_configurable() const;

        RCConfigLevel rc_configurable_level() const;

        bool rc_configured() const;

        bool env_var_configured() const;

        bool cli_configured() const;

        bool api_configured() const;

        bool configured() const;

        bool env_var_active() const;

        bool has_single_op_lifetime() const;

        self_type& set_rc_value(const T& value, const std::string& source);

        self_type& set_rc_values(const std::map<std::string, T>& mapped_values,
                                 const std::vector<std::string>& sources);

        self_type& set_value(const T& value);

        self_type& set_default_value(const T& value);

        self_type& clear_rc_values();

        self_type& clear_env_values();

        self_type& clear_cli_value();

        self_type& clear_api_value();

        self_type& clear_values();

        self_type& set_single_op_lifetime();

        self_type& set_env_var_names(const std::vector<std::string>& names = {});

        self_type& group(const std::string& group);

        self_type& needs(const std::set<std::string>& names);

        self_type& implies(const std::set<std::string>& names);

        self_type& set_rc_configurable(RCConfigLevel level = RCConfigLevel::kTargetPrefix);

        self_type& description(const std::string& desc);

        self_type& long_description(const std::string& desc);

        self_type& set_default_value_hook(value_hook_type hook);

        self_type& set_fallback_value_hook(value_hook_type hook);

        self_type& set_post_merge_hook(post_merge_hook_type hook);

        self_type& set_post_context_hook(post_context_hook_type hook);

        self_type& set_cli_value(const cli_config_storage_type& value);

        cli_config_storage_type& set_cli_config(const cli_config_storage_type& init);

        self_type& compute(const int options = 0,
                           const ConfigurationLevel& level = ConfigurationLevel::kDefault);

        bool is_valid_serialization(const std::string& value) const;

        bool is_sequence() const;

        void reset_compute_counter();

        void lock();

        void free();

        bool locked();

    private:
        std::string m_name;
        std::string m_group = "Default";
        std::string m_description = "No description provided";
        std::string m_long_description = "";
        std::vector<std::string> m_env_var_names = {};

        bool m_rc_configurable = false;
        RCConfigLevel m_rc_configurable_policy = RCConfigLevel::kTargetPrefix;
        bool m_rc_configured = false;
        bool m_api_configured = false;

        bool m_lock = false;
        int m_compute_counter = 0;

        bool m_single_op_lifetime = false;

        std::set<std::string> m_needed_configs, m_implied_configs;

        std::map<std::string, T> m_rc_values, m_values;
        std::vector<std::string> m_rc_sources, m_sources;

        T m_value, m_default_value;
        std::vector<std::string> m_source;

        std::shared_ptr<cli_config_type> p_cli_config = 0;
        T* p_context = 0;

        value_hook_type p_default_value_hook, p_fallback_value_hook;
        post_merge_hook_type p_post_merge_hook;
        post_context_hook_type p_post_ctx_hook;

        self_type& set_context();
    };

    /*********************
     * Configurable impl *
     *********************/

    template <class T>
    Configurable<T>::Configurable(const std::string& name, T* context)
        : m_name(name)
        , m_value(*context)
        , m_default_value(*context)
        , m_source(detail::Source<T>::default_value(*context))
        , p_context(context){};

    template <class T>
    Configurable<T>::Configurable(const std::string& name, const T& init)
        : m_name(name)
        , m_value(init)
        , m_default_value(init)
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
    bool Configurable<T>::rc_configurable() const
    {
        return m_rc_configurable;
    };

    template <class T>
    RCConfigLevel Configurable<T>::rc_configurable_level() const
    {
        return m_rc_configurable_policy;
    };

    template <class T>
    bool Configurable<T>::rc_configured() const
    {
        return m_rc_configured && !Context::instance().no_rc;
    };

    template <class T>
    bool Configurable<T>::env_var_configured() const
    {
        if (Context::instance().no_env)
            return false;

        for (const auto& env_var : m_env_var_names)
            if (!env::get(env_var).empty())
                return true;

        return false;
    };

    template <class T>
    bool Configurable<T>::cli_configured() const
    {
        return (p_cli_config != NULL) && p_cli_config->defined();
    };

    template <class T>
    bool Configurable<T>::api_configured() const
    {
        return m_api_configured;
    };

    template <class T>
    bool Configurable<T>::configured() const
    {
        return rc_configured() || env_var_configured() || cli_configured() || api_configured();
    };

    template <class T>
    bool Configurable<T>::env_var_active() const
    {
        return !Context::instance().no_env || (name() == "no_env");
    };

    template <class T>
    bool Configurable<T>::has_single_op_lifetime() const
    {
        return m_single_op_lifetime;
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
    auto Configurable<T>::set_rc_value(const T& value, const std::string& source) -> self_type&
    {
        m_rc_sources.push_back(source);
        m_rc_values[source] = value;
        m_rc_configured = true;
        return *this;
    };

    template <class T>
    auto Configurable<T>::set_rc_values(const std::map<std::string, T>& mapped_values,
                                        const std::vector<std::string>& sources) -> self_type&
    {
        assert(mapped_values.size() == sources.size());
        m_rc_sources.insert(m_rc_sources.end(), sources.begin(), sources.end());
        m_rc_values.insert(mapped_values.begin(), mapped_values.end());
        m_rc_configured = true;
        return *this;
    };

    template <class T>
    auto Configurable<T>::set_cli_value(const cli_config_storage_type& value) -> self_type&
    {
        if (p_cli_config == NULL)
        {
            p_cli_config = std::make_shared<cli_config_type>(value);
        }
        else
        {
            p_cli_config->m_value = value;
        }

        return *this;
    };

    template <class T>
    auto Configurable<T>::set_value(const T& value) -> self_type&
    {
        m_value = value;
        m_api_configured = true;
        return *this;
    };

    template <class T>
    auto Configurable<T>::set_default_value(const T& value) -> self_type&
    {
        m_value = m_default_value;
        return *this;
    };

    template <class T>
    void Configurable<T>::reset_compute_counter()
    {
        m_compute_counter = 0;
    }

    template <class T>
    auto Configurable<T>::set_single_op_lifetime() -> self_type&
    {
        m_single_op_lifetime = true;
        return *this;
    }

    template <class T>
    bool Configurable<T>::is_valid_serialization(const std::string& value) const
    {
        try
        {
            detail::Source<T>::deserialize(value);
            return true;
        }
        catch (...)
        {
            return false;
        }
    }

    template <class T>
    bool Configurable<T>::is_sequence() const
    {
        return detail::Source<T>::is_sequence();
    }

    template <class T>
    void Configurable<T>::lock()
    {
        m_lock = true;
    }

    template <class T>
    void Configurable<T>::free()
    {
        m_lock = false;
    }

    template <class T>
    bool Configurable<T>::locked()
    {
        return m_lock;
    }

    template <class T>
    auto Configurable<T>::set_env_var_names(const std::vector<std::string>& names) -> self_type&
    {
        if (names.empty())
            m_env_var_names = { "MAMBA_" + to_upper(m_name) };
        else
            m_env_var_names = names;

        if (name() != "no_env")
            m_needed_configs.insert("no_env");

        return *this;
    }

    template <class T>
    auto Configurable<T>::clear_rc_values() -> self_type&
    {
        m_rc_sources.clear();
        m_rc_values.clear();
        m_rc_configured = false;
        return *this;
    };

    template <class T>
    auto Configurable<T>::clear_env_values() -> self_type&
    {
        if (env_var_configured())
            for (const auto& ev : m_env_var_names)
                env::set(ev, "");
        return *this;
    };

    template <class T>
    auto Configurable<T>::clear_cli_value() -> self_type&
    {
        p_cli_config = nullptr;
        return *this;
    };

    template <class T>
    auto Configurable<T>::clear_api_value() -> self_type&
    {
        m_api_configured = false;
        return *this;
    };

    template <class T>
    auto Configurable<T>::clear_values() -> self_type&
    {
        clear_rc_values();
        clear_env_values();
        clear_cli_value();
        clear_api_value();
        m_value = m_default_value;

        return *this;
    };

    template <class T>
    auto Configurable<T>::group(const std::string& group) -> self_type&
    {
        m_group = group;
        return *this;
    };

    template <class T>
    auto Configurable<T>::set_rc_configurable(RCConfigLevel level) -> self_type&
    {
        m_rc_configurable = true;
        m_rc_configurable_policy = level;

        if (level == RCConfigLevel::kTargetPrefix)
            m_needed_configs.insert("target_prefix");
        else
            m_needed_configs.insert("root_prefix");

        return *this;
    };

    template <class T>
    auto Configurable<T>::description(const std::string& desc) -> self_type&
    {
        m_description = desc;
        return *this;
    };

    template <class T>
    auto Configurable<T>::needs(const std::set<std::string>& names) -> self_type&
    {
        m_needed_configs.insert(names.cbegin(), names.cend());
        return *this;
    };

    template <class T>
    auto Configurable<T>::implies(const std::set<std::string>& names) -> self_type&
    {
        m_implied_configs.insert(names.cbegin(), names.cend());
        return *this;
    };

    template <class T>
    const std::set<std::string>& Configurable<T>::needed() const
    {
        return m_needed_configs;
    };

    template <class T>
    const std::set<std::string>& Configurable<T>::implied() const
    {
        return m_implied_configs;
    };

    template <class T>
    auto Configurable<T>::long_description(const std::string& desc) -> self_type&
    {
        m_long_description = desc;
        return *this;
    };

    template <class T>
    auto Configurable<T>::set_default_value_hook(value_hook_type hook) -> self_type&
    {
        p_default_value_hook = hook;
        return *this;
    }

    template <class T>
    auto Configurable<T>::set_fallback_value_hook(value_hook_type hook) -> self_type&
    {
        p_fallback_value_hook = hook;
        return *this;
    }

    template <class T>
    auto Configurable<T>::set_post_merge_hook(post_merge_hook_type hook) -> self_type&
    {
        p_post_merge_hook = hook;
        return *this;
    }

    template <class T>
    auto Configurable<T>::set_post_context_hook(post_context_hook_type hook) -> self_type&
    {
        p_post_ctx_hook = hook;
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

            virtual bool api_configured() const = 0;

            virtual bool rc_configurable() const = 0;

            virtual RCConfigLevel rc_configurable_level() const = 0;

            virtual const std::set<std::string>& needed() const = 0;

            virtual const std::set<std::string>& implied() const = 0;

            virtual bool has_single_op_lifetime() const = 0;

            virtual bool is_valid_serialization(const std::string& value) const = 0;

            virtual bool is_sequence() const = 0;

            virtual bool locked() = 0;

            virtual void reset_compute_counter() = 0;

            virtual void lock() = 0;

            virtual void free() = 0;

            virtual void set_rc_yaml_value(const YAML::Node& value, const std::string& source) = 0;

            virtual void set_rc_yaml_values(const std::map<std::string, YAML::Node>& values,
                                            const std::vector<std::string>& sources)
                = 0;

            virtual void set_cli_yaml_value(const YAML::Node& value) = 0;

            virtual void set_cli_yaml_value(const std::string& value) = 0;

            virtual void set_yaml_value(const YAML::Node& value) = 0;

            virtual void set_yaml_value(const std::string& value) = 0;

            virtual void clear_rc_values() = 0;

            virtual void clear_env_values() = 0;

            virtual void clear_cli_value() = 0;

            virtual void clear_api_value() = 0;

            virtual void clear_values() = 0;

            virtual void set_env_var_names(const std::vector<std::string>& names) = 0;

            virtual void group(const std::string& name) = 0;

            virtual void description(const std::string& desc) = 0;

            virtual void long_description(const std::string& desc) = 0;

            virtual void compute(const int options, const ConfigurationLevel& level) = 0;
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

            bool api_configured() const
            {
                return p_wrapped->api_configured();
            };

            bool rc_configurable() const
            {
                return p_wrapped->rc_configurable();
            };

            RCConfigLevel rc_configurable_level() const
            {
                return p_wrapped->rc_configurable_level();
            };

            const std::set<std::string>& needed() const
            {
                return p_wrapped->needed();
            }

            const std::set<std::string>& implied() const
            {
                return p_wrapped->implied();
            }

            bool has_single_op_lifetime() const
            {
                return p_wrapped->has_single_op_lifetime();
            }

            bool locked()
            {
                return p_wrapped->locked();
            }

            bool is_valid_serialization(const std::string& value) const
            {
                return p_wrapped->is_valid_serialization(value);
            }

            bool is_sequence() const
            {
                return p_wrapped->is_sequence();
            }

            void reset_compute_counter()
            {
                p_wrapped->reset_compute_counter();
            }

            void lock()
            {
                p_wrapped->lock();
            }

            void free()
            {
                p_wrapped->free();
            }

            void set_rc_yaml_value(const YAML::Node& value, const std::string& source)
            {
                try
                {
                    p_wrapped->set_rc_value(value.as<T>(), source);
                }
                catch (const YAML::Exception& e)
                {
                    LOG_ERROR << "Bad conversion of configurable '" << name() << "' from source '"
                              << source << "'";
                }
            };

            void set_rc_yaml_values(const std::map<std::string, YAML::Node>& values,
                                    const std::vector<std::string>& sources)
            {
                std::map<std::string, T> converted_values;
                for (auto& y : values)
                {
                    converted_values.insert({ y.first, y.second.as<T>() });
                }
                p_wrapped->set_rc_values(converted_values, sources);
            };

            void set_cli_yaml_value(const YAML::Node& value)
            {
                p_wrapped->set_cli_value(value.as<cli_config_storage_type>());
            };

            void set_cli_yaml_value(const std::string& value)
            {
                p_wrapped->set_cli_value(
                    detail::Source<cli_config_storage_type>::deserialize(value));
            };

            void set_yaml_value(const YAML::Node& value)
            {
                p_wrapped->set_value(value.as<T>());
            };

            void set_yaml_value(const std::string& value)
            {
                try
                {
                    p_wrapped->set_value(detail::Source<T>::deserialize(value));
                }
                catch (const YAML::Exception& e)
                {
                    LOG_ERROR << "Bad conversion of configurable '" << name() << "' with value '"
                              << value << "'";
                    throw e;
                }
            };

            void clear_rc_values()
            {
                p_wrapped->clear_rc_values();
            };

            void clear_env_values()
            {
                p_wrapped->clear_env_values();
            };

            void clear_cli_value()
            {
                p_wrapped->clear_cli_value();
            };

            void clear_api_value()
            {
                p_wrapped->clear_api_value();
            };

            void clear_values()
            {
                p_wrapped->clear_values();
            };

            void set_env_var_names(const std::vector<std::string>& names)
            {
                p_wrapped->set_env_var_names(names);
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

            void compute(const int options, const ConfigurationLevel& level)
            {
                p_wrapped->compute(options, level);
            };

        private:
            std::unique_ptr<wrapped_type> p_wrapped;
        };

        std::unique_ptr<WrapperBase> p_impl;

    public:
        using self_type = ConfigurableInterface;

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
            try
            {
                auto& derived = dynamic_cast<Wrapper<T>&>(*p_impl);
                return derived.get_wrapped();
            }
            catch (const std::bad_cast& e)
            {
                LOG_ERROR << "Bad cast of Configurable '" << name() << "'";
                throw e;
            }
        };

        template <class T>
        Configurable<T>& as()
        {
            return get_wrapped<T>();
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
        const T& value() const
        {
            return get_wrapped<T>().value();
        };

        template <class T>
        T& value()
        {
            return get_wrapped<T>().value();
        };

        YAML::Node yaml_value() const
        {
            return p_impl->yaml_value();
        };

        template <class T>
        T cli_value()
        {
            return get_wrapped<T>().cli_value();
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

        bool api_configured() const
        {
            return p_impl->api_configured();
        };

        bool rc_configurable() const
        {
            return p_impl->rc_configurable();
        };

        RCConfigLevel rc_configurable_level() const
        {
            return p_impl->rc_configurable_level();
        };

        const std::set<std::string>& needed() const
        {
            return p_impl->needed();
        }

        const std::set<std::string>& implied() const
        {
            return p_impl->implied();
        }
        void reset_compute_counter()
        {
            p_impl->reset_compute_counter();
        }

        void lock()
        {
            p_impl->lock();
        }

        void free()
        {
            p_impl->free();
        }

        bool has_single_op_lifetime() const
        {
            return p_impl->has_single_op_lifetime();
        }

        bool locked()
        {
            return p_impl->locked();
        }

        bool is_valid_serialization(const std::string& value) const
        {
            return p_impl->is_valid_serialization(value);
        }

        bool is_sequence() const
        {
            return p_impl->is_sequence();
        }

        self_type& set_rc_yaml_value(const YAML::Node& value, const std::string& source)
        {
            p_impl->set_rc_yaml_value(value, source);
            return *this;
        };

        self_type& set_rc_yaml_values(const std::map<std::string, YAML::Node>& values,
                                      const std::vector<std::string>& sources)
        {
            p_impl->set_rc_yaml_values(values, sources);
            return *this;
        };

        self_type& set_cli_yaml_value(const YAML::Node& value)
        {
            p_impl->set_cli_yaml_value(value);
            return *this;
        };

        self_type& set_cli_yaml_value(const std::string& value)
        {
            p_impl->set_cli_yaml_value(value);
            return *this;
        };

        template <class T>
        self_type& set_cli_value(const typename detail::cli_config<T>::storage_type& value)
        {
            get_wrapped<T>().set_cli_value(value);
            return *this;
        };

        self_type& set_yaml_value(const YAML::Node& value)
        {
            p_impl->set_yaml_value(value);
            return *this;
        };

        self_type& set_yaml_value(const std::string& value)
        {
            p_impl->set_yaml_value(value);
            return *this;
        };

        template <class T>
        self_type& set_value(const T& value)
        {
            get_wrapped<T>().set_value(value);
            return *this;
        };

        self_type& clear_rc_values()
        {
            p_impl->clear_rc_values();
            return *this;
        };

        self_type& clear_env_values()
        {
            p_impl->clear_env_values();
            return *this;
        };

        self_type& clear_cli_value()
        {
            p_impl->clear_cli_value();
            return *this;
        };

        self_type& clear_api_value()
        {
            p_impl->clear_api_value();
            return *this;
        };

        self_type& clear_values()
        {
            p_impl->clear_values();
            return *this;
        };

        self_type& set_env_var_names(const std::vector<std::string>& names = {})
        {
            p_impl->set_env_var_names(names);
            return *this;
        };

        self_type& group(const std::string& name)
        {
            p_impl->group(name);
            return *this;
        };

        self_type& description(const std::string& desc)
        {
            p_impl->description(desc);
            return *this;
        };

        self_type& long_description(const std::string& desc)
        {
            p_impl->long_description(desc);
            return *this;
        };

        self_type& compute(const int options = 0,
                           const ConfigurationLevel& level = ConfigurationLevel::kDefault)
        {
            p_impl->compute(options, level);
            return *this;
        };
    };

    namespace detail
    {
        void check_target_prefix(int options);
    }


    int const MAMBA_SHOW_CONFIG_VALUES = 1 << 0;
    int const MAMBA_SHOW_CONFIG_SRCS = 1 << 1;
    int const MAMBA_SHOW_CONFIG_DESCS = 1 << 2;
    int const MAMBA_SHOW_CONFIG_LONG_DESCS = 1 << 3;
    int const MAMBA_SHOW_CONFIG_GROUPS = 1 << 4;
    int const MAMBA_SHOW_ALL_CONFIGS = 1 << 5;
    int const MAMBA_SHOW_ALL_RC_CONFIGS = 1 << 6;


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

        void set_rc_values(std::vector<fs::path> possible_rc_paths, const RCConfigLevel& level);

        void load();

        bool is_loading();

        void clear_rc_values();
        void clear_cli_values();
        void clear_values();

        /**
         * Pop values that should have a single operation lifetime to avoid memroy effect
         * between multiple operations.
         * It corresponds to CLI values in most of the cases, but may also include API
         * values if the `Configurable::has_single_op_lifetime` method returns true.
         * RC files and environment variables are always overriden when loading the
         * configuration.
         */
        void operation_teardown();

        std::string dump(int opts = MAMBA_SHOW_CONFIG_VALUES, std::vector<std::string> names = {});

        template <class T>
        typename detail::cli_config<T>::storage_type& link_cli_option(
            const std::string& name, const typename detail::cli_config<T>::storage_type& init);

        template <class T>
        Configurable<T>& insert(Configurable<T> configurable, bool allow_redefinition = false);

        void reset_configurables();

    protected:
        Configuration();
        ~Configuration() = default;

        void set_configurables();

        void reset_compute_counters();

        void compute_loading_sequence();

        void clear_rc_sources();

        void add_to_loading_sequence(std::vector<std::string>& seq,
                                     const std::string& name,
                                     std::vector<std::string>&);

        static YAML::Node load_rc_file(const fs::path& file);

        static std::vector<fs::path> compute_default_rc_sources(const RCConfigLevel& level);

        std::vector<fs::path> get_existing_rc_sources(
            const std::vector<fs::path>& possible_rc_paths);

        std::vector<fs::path> m_sources;
        std::vector<fs::path> m_valid_sources;
        std::map<fs::path, YAML::Node> m_rc_yaml_nodes_cache;

        bool m_load_lock = false;

        std::map<std::string, ConfigurableInterface> m_config;
        std::vector<std::string> m_config_order, m_loading_sequence;
    };

    template <class T>
    Configurable<T>& Configuration::insert(Configurable<T> configurable, bool allow_redefinition)
    {
        std::string name = configurable.name();
        if (m_config.count(name) == 0)
        {
            m_config.insert({ name, ConfigurableInterface(std::move(configurable)) });
            m_config_order.push_back(name);
        }
        else
        {
            if (!allow_redefinition)
            {
                throw std::runtime_error("Redefinition of configurable '" + name
                                         + "' not allowed.");
            }
        }

        return m_config.at(name).get_wrapped<T>();
    }

    template <class T>
    auto Configurable<T>::compute(const int options, const ConfigurationLevel& level) -> self_type&
    {
        bool hook_disabled = options & MAMBA_CONF_DISABLE_HOOK;
        bool force_compute = options & MAMBA_CONF_FORCE_COMPUTE;

        if (force_compute)
            LOG_TRACE << "Update configurable '" << name() << "'";
        else
            LOG_TRACE << "Compute configurable '" << name() << "'";

        if (!force_compute && (Configuration::instance().is_loading() && (m_compute_counter > 0)))
            throw std::runtime_error("Multiple computation of '" + m_name
                                     + "' detected during loading sequence.");

        auto& ctx = Context::instance();
        m_sources.clear();
        m_values.clear();

        if (api_configured() && (level >= ConfigurationLevel::kApi))
        {
            m_sources.push_back("API");
            m_values.insert({ "API", m_value });
        }

        if (cli_configured() && (level >= ConfigurationLevel::kCli))
        {
            m_sources.push_back("CLI");
            m_values.insert({ "CLI", p_cli_config->value() });
        }

        if (env_var_configured() && env_var_active() && (level >= ConfigurationLevel::kEnvVar))
        {
            for (const auto& env_var : m_env_var_names)
            {
                auto env_var_value = env::get(env_var);
                if (!env_var_value.empty())
                {
                    try
                    {
                        m_values.insert({ env_var, detail::Source<T>::deserialize(env_var_value) });
                        m_sources.push_back(env_var);
                    }
                    catch (const YAML::Exception& e)
                    {
                        LOG_ERROR << "Bad conversion of configurable '" << name()
                                  << "' from environment variable '" << env_var << "' with value '"
                                  << env_var_value << "'";
                        throw e;
                    }
                }
            }
        }

        if (rc_configured() && !ctx.no_rc && (level >= ConfigurationLevel::kFile))
        {
            m_sources.insert(m_sources.end(), m_rc_sources.begin(), m_rc_sources.end());
            m_values.insert(m_rc_values.begin(), m_rc_values.end());
        }

        if ((p_default_value_hook != NULL) && (level >= ConfigurationLevel::kDefault))
        {
            m_sources.push_back("default");
            m_values.insert({ "default", p_default_value_hook() });
        }

        if (m_sources.empty() && (p_fallback_value_hook != NULL))
        {
            m_sources.push_back("fallback");
            m_values.insert({ "fallback", p_fallback_value_hook() });
        }

        if (!m_sources.empty())
            detail::Source<T>::merge(m_values, m_sources, m_value, m_source);
        else
        {
            m_value = m_default_value;
            m_source = detail::Source<T>::default_value(m_default_value);
        }

        if (!hook_disabled && (p_post_merge_hook != NULL))
            p_post_merge_hook(m_value);

        ++m_compute_counter;
        set_context();

        if (p_post_ctx_hook != NULL)
            p_post_ctx_hook();

        return *this;
    }

    template <class T>
    const T& Configurable<T>::value() const
    {
        if (Configuration::instance().is_loading() && m_compute_counter == 0)
            throw std::runtime_error("Using '" + m_name + "' value without previous computation.");
        return m_value;
    };

    template <class T>
    T& Configurable<T>::value()
    {
        if (Configuration::instance().is_loading() && m_compute_counter == 0)
            throw std::runtime_error("Using '" + m_name + "' value without previous computation.");
        return m_value;
    };

    template <class T>
    auto Configuration::link_cli_option(const std::string& name,
                                        const typename detail::cli_config<T>::storage_type& init) ->
        typename detail::cli_config<T>::storage_type&
    {
        auto& config = at(name).get_wrapped<T>();
        return config.set_cli_config(init);
    }

    void use_conda_root_prefix(bool force = false);
}

#endif  // MAMBA_CONFIG_HPP
