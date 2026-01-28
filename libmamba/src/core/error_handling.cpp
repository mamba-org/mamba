#include <algorithm>

#include "mamba/core/error_handling.hpp"
#include "mamba/core/logging.hpp"

namespace mamba
{
    namespace
    {
        void maybe_dump_backtrace(mamba_error_code ec)
        {
            if (ec == mamba_error_code::internal_failure)
            {
                logging::log_backtrace();
            }
        }

    }

    mamba_error::mamba_error(const std::string& msg, mamba_error_code ec)
        : base_type(msg)
        , m_error_code(ec)
    {
        maybe_dump_backtrace(m_error_code);
    }

    mamba_error::mamba_error(const char* msg, mamba_error_code ec)
        : base_type(msg)
        , m_error_code(ec)
    {
        maybe_dump_backtrace(m_error_code);
    }

    mamba_error::mamba_error(const std::string& msg, mamba_error_code ec, std::any&& data)
        : base_type(msg)
        , m_error_code(ec)
        , m_data(std::move(data))
    {
        maybe_dump_backtrace(m_error_code);
    }

    mamba_error::mamba_error(const char* msg, mamba_error_code ec, std::any&& data)
        : base_type(msg)
        , m_error_code(ec)
        , m_data(std::move(data))
    {
        maybe_dump_backtrace(m_error_code);
    }

    mamba_error_code mamba_error::error_code() const noexcept
    {
        return m_error_code;
    }

    const std::any& mamba_error::data() const noexcept
    {
        return m_data;
    }

    namespace
    {
        constexpr auto* message_aggregated_error_top = "Multiple errors occurred:\n";
        constexpr auto* message_aggregated_bug_report = "If you run into this error repeatedly, your package cache may be corrupted.\n"
                                                        "Please try running `mamba clean -a` to remove this cache before retrying the operation.\n"
                                                        "\n"
                                                        "If you still are having issues, please report the error on `mamba-org/mamba`'s issue tracker:\n"
                                                        "https://github.com/mamba-org/mamba/issues/new?assignees=&labels=&projects=&template=bug.yml";

    }

    mamba_aggregated_error::mamba_aggregated_error(error_list_t&& error_list, bool with_bug_report_info)
        : base_type(message_aggregated_error_top, mamba_error_code::aggregated)
        , m_error_list(std::move(error_list))
        , m_aggregated_message()
        , m_with_bug_report_message(with_bug_report_info)
    {
    }

    const char* mamba_aggregated_error::what() const noexcept
    {
        if (m_aggregated_message.empty())
        {
            // If we have only one error, don't say it's multiple errors.
            if (m_error_list.size() > 1)
            {
                m_aggregated_message = message_aggregated_error_top;
            }

            for (const mamba_error& er : m_error_list)
            {
                m_aggregated_message += er.what();
                m_aggregated_message += "\n";
            }

            if (m_with_bug_report_message)
            {
                m_aggregated_message += message_aggregated_bug_report;
            }
        }
        return m_aggregated_message.c_str();
    }

    bool mamba_aggregated_error::has_only_error(const mamba_error_code code) const
    {
        return std::ranges::all_of(
            m_error_list,
            [&](const auto& err) { return err.error_code() == code; }
        );
    }

    tl::unexpected<mamba_error> make_unexpected(const char* msg, mamba_error_code ec)
    {
        return tl::make_unexpected(mamba_error(msg, ec));
    }

    tl::unexpected<mamba_error> make_unexpected(const std::string& msg, mamba_error_code ec)
    {
        return tl::make_unexpected(mamba_error(msg, ec));
    }

    tl::unexpected<mamba_aggregated_error> make_unexpected(std::vector<mamba_error>&& error_list)
    {
        return tl::make_unexpected(mamba_aggregated_error(std::move(error_list)));
    }
}
