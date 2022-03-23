// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_CORE_OUTPUT_HPP
#define MAMBA_CORE_OUTPUT_HPP

#include "progress_bar.hpp"

#include "nlohmann/json.hpp"

#include "spdlog/spdlog.h"
#include "spdlog/sinks/stdout_color_sinks.h"

#include <chrono>
#include <iostream>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>


#define ENUM_FLAG_OPERATOR(T, X)                                                                   \
    inline T operator X(T lhs, T rhs)                                                              \
    {                                                                                              \
        return (T) (static_cast<std::underlying_type_t<T>>(lhs)                                    \
                        X static_cast<std::underlying_type_t<T>>(rhs));                            \
    }
#define ENUM_FLAGS(T)                                                                              \
    enum class T;                                                                                  \
    inline T operator~(T t)                                                                        \
    {                                                                                              \
        return (T) (~static_cast<std::underlying_type_t<T>>(t));                                   \
    }                                                                                              \
    ENUM_FLAG_OPERATOR(T, |)                                                                       \
    ENUM_FLAG_OPERATOR(T, ^)                                                                       \
    ENUM_FLAG_OPERATOR(T, &)                                                                       \
    enum class T

#define PREFIX_LENGTH 25

namespace mamba
{
    std::string cut_repo_name(const std::string& reponame);

    namespace printers
    {
        enum class format : std::size_t
        {
            none = 0,
            red = 1 << 1,
            green = 1 << 2,
            yellow = 1 << 3,
            bold_blue = 1 << 4
        };

        struct FormattedString
        {
            std::string s;
            format flag = format::none;

            FormattedString() = default;

            inline FormattedString(const std::string& i)
                : s(i)
            {
            }

            inline FormattedString(const char* i)
                : s(i)
            {
            }

            inline std::size_t size() const
            {
                return s.size();
            }
        };

        enum class alignment : std::size_t
        {
            left = 1 << 1,
            right = 1 << 2,
            fill = 1 << 3
        };

        class Table
        {
        public:
            Table(const std::vector<FormattedString>& header);

            void set_alignment(const std::vector<alignment>& a);
            void set_padding(const std::vector<int>& p);
            void add_row(const std::vector<FormattedString>& r);
            void add_rows(const std::string& header,
                          const std::vector<std::vector<FormattedString>>& rs);

            std::ostream& print(std::ostream& out);

        private:
            std::vector<FormattedString> m_header;
            std::vector<alignment> m_align;
            std::vector<int> m_padding;
            std::vector<std::vector<FormattedString>> m_table;
        };

        std::ostringstream table_like(const std::vector<std::string>& data, std::size_t max_width);
    }  // namespace printers

    // Todo: replace public inheritance with
    // private one + using directives
    class ConsoleStream : public std::stringstream
    {
    public:
        ConsoleStream();
        ~ConsoleStream();
    };

    class Console
    {
    public:
        Console(const Console&) = delete;
        Console& operator=(const Console&) = delete;

        Console(Console&&) = delete;
        Console& operator=(Console&&) = delete;

        static Console& instance();

        static ConsoleStream stream();
        static void print(const std::string_view& str, bool force_print = false);
        static bool prompt(const std::string_view& message,
                           char fallback = '_',
                           std::istream& input_stream = std::cin);

        ProgressProxy add_progress_bar(const std::string& name, size_t expected_total = 0);
        void clear_progress_bars();
        ProgressBarManager& init_progress_bar_manager(ProgressBarMode mode
                                                      = ProgressBarMode::multi);
        ProgressBarManager& progress_bar_manager();

        static std::string hide_secrets(const std::string_view& str);

        void json_print();
        void json_write(const nlohmann::json& j);
        void json_append(const std::string& value);
        void json_append(const nlohmann::json& j);
        void json_down(const std::string& key);
        void json_up();

        static void print_buffer(std::ostream& ostream);

    private:
        Console();
        ~Console() = default;

        void deactivate_progress_bar(std::size_t idx, const std::string_view& msg = "");

        std::mutex m_mutex;
        std::unique_ptr<ProgressBarManager> p_progress_bar_manager;

        std::string json_hier;
        unsigned int json_index;
        nlohmann::json json_log;

        static std::vector<std::string> m_buffer;

        friend class ProgressProxy;
    };

    class MessageLogger
    {
    public:
        MessageLogger(const char* file, int line, spdlog::level::level_enum level);
        ~MessageLogger();

        std::stringstream& stream();

        static void activate_buffer();
        static void deactivate_buffer();
        static void print_buffer(std::ostream& ostream);

    private:
        std::string m_file;
        int m_line;
        spdlog::level::level_enum m_level;
        std::stringstream m_stream;

        static std::mutex m_mutex;
        static bool use_buffer;
        static std::vector<std::pair<std::string, spdlog::level::level_enum>> m_buffer;

        static void emit(const std::string& msg, const spdlog::level::level_enum& level);
    };


    class Logger : public spdlog::logger
    {
    public:
        Logger(const std::string& name, const std::string& pattern, const std::string& eol);

        void dump_backtrace_no_guards();
    };
}  // namespace mamba

#undef LOG
#undef LOG_TRACE
#undef LOG_DEBUG
#undef LOG_INFO
#undef LOG_WARNING
#undef LOG_ERROR
#undef LOG_CRITICAL

#define LOG(severity) mamba::MessageLogger(__FILE__, __LINE__, severity).stream()
#define LOG_TRACE LOG(spdlog::level::trace)
#define LOG_DEBUG LOG(spdlog::level::debug)
#define LOG_INFO LOG(spdlog::level::info)
#define LOG_WARNING LOG(spdlog::level::warn)
#define LOG_ERROR LOG(spdlog::level::err)
#define LOG_CRITICAL LOG(spdlog::level::critical)

#endif  // MAMBA_OUTPUT_HPP
