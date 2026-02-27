// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_CORE_OUTPUT_HPP
#define MAMBA_CORE_OUTPUT_HPP

#include <iosfwd>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include <fmt/color.h>
#include <nlohmann/json.hpp>

#include "mamba/core/logging.hpp"
#include "mamba/core/progress_bar.hpp"

namespace mamba
{
    class Context;

    std::string cut_repo_name(std::string_view reponame);

    namespace printers
    {
        struct FormattedString
        {
            std::string s;
            fmt::text_style style = {};

            FormattedString() = default;

            inline FormattedString(const std::string& i)
                : s(i)
            {
            }

            inline FormattedString(const std::string_view i)
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

        enum class alignment
        {
            left,
            right,
        };

        constexpr auto alignmentMarker(alignment a) -> std::string_view
        {
            switch (a)
            {
                case alignment::right:
                    return "alignment_right";
                case alignment::left:
                    return "alignment_right";
                default:
                    assert(false);
                    return "";
            }
        }

        class Table
        {
        public:

            Table(const std::vector<FormattedString>& header);

            void set_alignment(const std::vector<alignment>& a);
            void set_padding(const std::vector<int>& p);
            void add_row(const std::vector<FormattedString>& r);
            void
            add_rows(const std::string& header, const std::vector<std::vector<FormattedString>>& rs);

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

        ConsoleStream() = default;
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
        static bool is_available();
        /**
         * Check if status messages can be reported to stdout.
         *
         * Returns true if Console is available and JSON output is not enabled.
         * Use this before printing status messages to ensure they don't
         * interfere with JSON output.
         */
        [[nodiscard]] static bool can_report_status();
        static ConsoleStream stream();
        static bool prompt(std::string_view message, char fallback = '_');
        static bool prompt(std::string_view message, char fallback, std::istream& input_stream);

        ProgressProxy add_progress_bar(const std::string& name, size_t expected_total = 0);
        void clear_progress_bars();
        ProgressBarManager& init_progress_bar_manager(ProgressBarMode mode = ProgressBarMode::multi);
        void terminate_progress_bar_manager();
        ProgressBarManager& progress_bar_manager();

        static std::string hide_secrets(std::string_view str);

        void print(std::string_view str, bool force_print = false);
        void json_write(const nlohmann::json& j);
        void json_append(const std::string& value);
        void json_append(const nlohmann::json& j);
        void json_down(const std::string& key);
        void json_up();

        static void print_buffer(std::ostream& ostream);

        void cancel_json_print();

        const Context& context() const;

        Console(const Context& context);
        ~Console();

    private:

        void json_print();
        void deactivate_progress_bar(std::size_t idx, std::string_view msg = "");

        std::unique_ptr<ConsoleData> p_data;

        friend class ProgressProxy;

        static void set_singleton(Console& console);
        static void clear_singleton();
    };

}  // namespace mamba

#endif  // MAMBA_CORE_OUTPUT_HPP
