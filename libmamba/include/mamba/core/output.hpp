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

    /** Assignation of a JSON value at a specific address in a JSON object. */
    struct JSONAssign
    {
        nlohmann::json::json_pointer location;
        nlohmann::json value;
    };

    /** Changes to apply on a JSON object. */
    struct JSONEdit
    {
        std::vector<JSONAssign> to_assign;  ///< A sequence of assignations to apply.

        std::optional<bool> set_success = std::nullopt;  ///< If set, an assignation in the edited
                                                         ///< object to mark it as a success will be
                                                         ///< done once the other changes are
                                                         ///< applied.
    };

    class Console
    {
    public:

        Console(const Console&) = delete;
        Console& operator=(const Console&) = delete;

        Console(Console&&) = delete;
        Console& operator=(Console&&) = delete;

        static Console& instance();
        static bool is_available();
        /** Checks if status messages can be reported to stdout.
         *
         * Returns true when Console is available, `libmamba` is running from an
         * end-user executable (`mamba`/`micromamba`), and JSON output is disabled.
         * Use this before printing status messages to avoid leaking CLI-only
         * status lines to third-party `libmamba` integrations.
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
        void print_in_place(std::string_view str, bool finalize = false, bool force_print = false);

        /** Provides access to the json output object for arbitrary editing by passing it to a
            callable.
        */
        void edit_json_output(std::function<void(nlohmann::json&)> edit_func);

        /** Setup the json output object to value to express the current operations as
            a success or a failure.
        */
        void set_json_output_success(bool is_success);

        /** Assigns the provided json value to the associated location in the json output object.
            If the location already has a value, it will be overwritten.
            If `set_success` is specified, also calls `set_json_output_success` with the specified
            value.
        */
        void set_json_output(JSONEdit edit);

        /** If json output was requested, calling this before destroying a `Console` instance will
            not lead to a json output.
        */
        void cancel_json_print();

        static void print_buffer(std::ostream& ostream);

        const Context& context() const;

        Console(const Context& context);

        /** Prints in standard output the output json object's value if json output
            was requested in the context's options. @see Context::OutputParams
        */
        ~Console();

        /** Utility to set `"success": false` on destruction of this object
            when it's destructor is invoked while an exception is in flight.

            This helps avoiding situations where an exception that should be
            considered a failure of the overall operation is thrown but
            the json is still set with `"success": true`, while it should be `false`,
            misleading the testing outputs.
        */
        struct JSonFailureOnException
        {
            ~JSonFailureOnException()
            {
                if (std::uncaught_exceptions() > 0)
                {
                    Console::instance().set_json_output_success(false);
                }
            }
        };

    private:

        void print_json_output();

        std::unique_ptr<ConsoleData> p_data;

        friend class ProgressProxy;

        static void set_singleton(Console& console);
        static void clear_singleton();
    };

}  // namespace mamba

#endif  // MAMBA_CORE_OUTPUT_HPP
