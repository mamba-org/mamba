#ifndef MAMBA_API_CONFIGURATION_IMPL_HPP
#define MAMBA_API_CONFIGURATION_IMPL_HPP

#include <optional>
#include <string>
#include <vector>

#include "spdlog/spdlog.h"
#include <yaml-cpp/yaml.h>

#include "mamba/core/context.hpp"
#include "mamba/core/mamba_fs.hpp"

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
            cli_config(const T& value) : m_storage(value) {}
            storage_type& storage() { return m_storage; }
            bool has_value() const { return m_storage.has_value(); }
            const T& value() const { return m_storage.value(); }
            void reset() { m_storage.reset(); }
        };

        template <>
        struct cli_config<bool>
        {
            using storage_type = int;
            storage_type m_storage;

            cli_config() = default;
            cli_config(int value) : m_storage(value) {}
            storage_type& storage() { return m_storage; }
            bool has_value() const { return m_storage != 0; }
            bool value() const
            {
                if (m_storage == 1)
                    return true;
                else if (m_storage == -1)
                    return false;
                else
                    throw std::runtime_error(
                    "Invalid boolean storage, should be {'-1': false, '0': undefined, '1': true}");
            }
            void reset() { m_storage = 0; }
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

        /*************************
         * Source implementation *
         *************************/

        template <class T>
        void Source<T>::merge(const std::map<std::string, T>& values,
                              const std::vector<std::string>& sources,
                              T& value,
                              std::vector<std::string>& source)
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

#endif
