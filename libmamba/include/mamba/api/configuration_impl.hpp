#ifndef MAMBA_API_CONFIGURATION_IMPL_HPP
#define MAMBA_API_CONFIGURATION_IMPL_HPP

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <yaml-cpp/yaml.h>

#include "mamba/core/context.hpp"
#include "mamba/fs/filesystem.hpp"

namespace mamba
{
    namespace detail
    {
        // Because CLI11 supports std::optional for options but not for flags...
        /**************
         * cli_config *
         **************/

        template <class T>
        struct cli_config
        {
            using storage_type = std::optional<T>;
            storage_type m_storage;

            cli_config() = default;

            cli_config(const T& value)
                : m_storage(value)
            {
            }

            storage_type& storage()
            {
                return m_storage;
            }

            bool has_value() const
            {
                return m_storage.has_value();
            }

            const T& value() const
            {
                return m_storage.value();
            }

            void reset()
            {
                m_storage.reset();
            }
        };

        /**********************
         * Source declaration *
         **********************/

        template <class T>
        struct Source
        {
            static std::vector<std::string> default_value(const T&)
            {
                return std::vector<std::string>({ "default" });
            };

            static void merge(
                const std::map<std::string, T>& values,
                const std::vector<std::string>& sources,
                T& value,
                std::vector<std::string>& source
            );

            static T deserialize(const std::string& value);

            static bool is_sequence();
        };

        template <class T>
        struct Source<std::vector<T>>
        {
            static std::vector<std::string> default_value(const std::vector<T>& init)
            {
                return std::vector<std::string>(std::max<size_t>(1, init.size()), "default");
            };

            static void merge(
                const std::map<std::string, std::vector<T>>& values,
                const std::vector<std::string>& sources,
                std::vector<T>& value,
                std::vector<std::string>& source
            );

            static std::vector<T> deserialize(const std::string& value);

            static bool is_sequence();
        };

        /*************************
         * Source implementation *
         *************************/

        template <class T>
        void Source<T>::merge(
            const std::map<std::string, T>& values,
            const std::vector<std::string>& sources,
            T& value,
            std::vector<std::string>& source
        )
        {
            source = sources;
            value = values.at(sources.front());
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
        void Source<std::vector<T>>::merge(
            const std::map<std::string, std::vector<T>>& values,
            const std::vector<std::string>& sources,
            std::vector<T>& value,
            std::vector<std::string>& source
        )
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

        // Map-shaped config (e.g. exclude_newer_package): stored as vector of pairs for
        // duplicate-key / order control, but configured as a YAML/JSON object — not a sequence.
        // Must not use Source<vector<T>>::deserialize, which wraps the value in `[...]`.
        template <>
        struct Source<std::vector<std::pair<std::string, std::string>>>
        {
            using value_type = std::vector<std::pair<std::string, std::string>>;

            static std::vector<std::string> default_value(const value_type& /* init */)
            {
                return { "default" };
            }

            static void merge(
                const std::map<std::string, value_type>& values,
                const std::vector<std::string>& sources,
                value_type& value,
                std::vector<std::string>& source
            )
            {
                source = sources;
                value = values.at(sources.front());
            }

            static value_type deserialize(const std::string& value)
            {
                if (value.empty())
                {
                    return {};
                }
                return YAML::Load(value).as<value_type>();
            }

            static bool is_sequence()
            {
                return false;
            }
        };
    }
}

/****************
 * YAML parsers *
 ****************/

namespace YAML
{
    template <class T>
    struct convert<std::optional<T>>
    {
        static Node encode(const T& rhs)
        {
            return Node(rhs.value());
        }

        static bool decode(const Node& node, std::optional<T>& rhs)
        {
            if (!node.IsScalar())
            {
                return false;
            }

            rhs = std::optional<T>(node.as<T>());
            return true;
        }
    };

    template <>
    struct convert<std::vector<std::pair<std::string, std::string>>>
    {
        static Node encode(const std::vector<std::pair<std::string, std::string>>& rhs)
        {
            // Empty default Node is Null; print-config / YAML consumers need an empty Map.
            Node node(NodeType::Map);
            node.SetStyle(YAML::EmitterStyle::Block);
            for (const auto& [key, value] : rhs)
            {
                node[key] = value;
            }
            return node;
        }

        static bool decode(const Node& node, std::vector<std::pair<std::string, std::string>>& rhs)
        {
            if (!node.IsMap())
            {
                return false;
            }
            rhs.clear();
            rhs.reserve(node.size());
            for (const auto& entry : node)
            {
                rhs.emplace_back(entry.first.as<std::string>(), entry.second.as<std::string>());
            }
            return true;
        }
    };

    template <>
    struct convert<mamba::VerificationLevel>
    {
        static Node encode(const mamba::VerificationLevel& rhs)
        {
            if (rhs == mamba::VerificationLevel::Disabled)
            {
                return Node("disabled");
            }
            else if (rhs == mamba::VerificationLevel::Warn)
            {
                return Node("warn");
            }
            else if (rhs == mamba::VerificationLevel::Enabled)
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
                rhs = mamba::VerificationLevel::Enabled;
            }
            else if (str == "warn")
            {
                rhs = mamba::VerificationLevel::Warn;
            }
            else if (str == "disabled")
            {
                rhs = mamba::VerificationLevel::Disabled;
            }
            else
            {
                throw std::runtime_error(
                    "Invalid 'VerificationLevel', should be in {'enabled', 'warn', 'disabled'}"
                );
            }

            return true;
        }
    };

    template <>
    struct convert<mamba::ChannelPriority>
    {
        static Node encode(const mamba::ChannelPriority& rhs)
        {
            if (rhs == mamba::ChannelPriority::Strict)
            {
                return Node("strict");
            }
            else if (rhs == mamba::ChannelPriority::Flexible)
            {
                return Node("flexible");
            }
            else if (rhs == mamba::ChannelPriority::Disabled)
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
                rhs = mamba::ChannelPriority::Strict;
            }
            else if ((str == "flexible") || (str == "true"))
            {
                rhs = mamba::ChannelPriority::Flexible;
            }
            else if (str == "disabled")
            {
                rhs = mamba::ChannelPriority::Disabled;
            }
            else
            {
                return false;
            }

            return true;
        }
    };

    template <>
    struct convert<mamba::fs::u8path>
    {
        static Node encode(const mamba::fs::u8path& rhs)
        {
            return Node(rhs.string());
        }

        static bool decode(const Node& node, mamba::fs::u8path& rhs)
        {
            if (!node.IsScalar())
            {
                return false;
            }

            rhs = mamba::fs::u8path(node.as<std::string>());
            return true;
        }
    };

    template <>
    struct convert<mamba::log_level>
    {
    private:

        inline static const std::array<std::string, 7> log_level_names = {
            "trace", "debug", "info", "warning", "error", "critical", "off"
        };

    public:

        static Node encode(const mamba::log_level& rhs)
        {
            return Node(log_level_names[static_cast<size_t>(rhs)]);
        }

        static bool decode(const Node& node, mamba::log_level& rhs)
        {
            auto name = node.as<std::string>();
            auto it = std::find(log_level_names.begin(), log_level_names.end(), name);
            if (it != log_level_names.end())
            {
                rhs = static_cast<mamba::log_level>(it - log_level_names.begin());
                return true;
            }

            LOG_ERROR << "Invalid log level, should be in {'critical', 'error', 'warning', 'info', 'debug', 'trace', 'off'} but is '"
                      << name << "'";
            return false;
        }
    };
}

#endif
