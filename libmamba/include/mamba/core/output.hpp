// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_CORE_OUTPUT_HPP
#define MAMBA_CORE_OUTPUT_HPP

#include "progress_bar.hpp"

#include "nlohmann/json.hpp"

#include <iosfwd>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "mamba/core/common_types.hpp"

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

    class ProgressBarManager;
    class ConsoleData;

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
        static bool prompt(const std::string_view& message, char fallback = '_');
        static bool prompt(const std::string_view& message,
                           char fallback,
                           std::istream& input_stream);

        ProgressProxy add_progress_bar(const std::string& name, size_t expected_total = 0);
        void clear_progress_bars();
        ProgressBarManager& init_progress_bar_manager(ProgressBarMode mode
                                                      = ProgressBarMode::multi);
        ProgressBarManager& progress_bar_manager();

        static std::string hide_secrets(const std::string_view& str);

        void json_write(const nlohmann::json& j);
        void json_append(const std::string& value);
        void json_append(const nlohmann::json& j);
        void json_down(const std::string& key);
        void json_up();

        static void print_buffer(std::ostream& ostream);

        void cancel_json_print();

    private:
        Console();
        ~Console();

        void json_print();
        void deactivate_progress_bar(std::size_t idx, const std::string_view& msg = "");

        std::unique_ptr<ConsoleData> p_data;

        friend class ProgressProxy;
    };

    class MessageLogger
    {
    public:
        MessageLogger(const char* file, int line, log_level level);
        ~MessageLogger();

        std::stringstream& stream();

        static void activate_buffer();
        static void deactivate_buffer();
        static void print_buffer(std::ostream& ostream);

    private:
        std::string m_file;
        int m_line;
        log_level m_level;
        std::stringstream m_stream;

        static void emit(const std::string& msg, const log_level& level);
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
#define LOG_TRACE LOG(mamba::log_level::trace)
#define LOG_DEBUG LOG(mamba::log_level::debug)
#define LOG_INFO LOG(mamba::log_level::info)
#define LOG_WARNING LOG(mamba::log_level::warn)
#define LOG_ERROR LOG(mamba::log_level::err)
#define LOG_CRITICAL LOG(mamba::log_level::critical)

#endif  // MAMBA_OUTPUT_HPP
