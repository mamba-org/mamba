// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/ioctl.h>
#endif

#include <algorithm>
#include <cstdlib>

#include "mamba/core/output.hpp"
#include "mamba/core/thread_utils.hpp"
#include "mamba/core/url.hpp"
#include "mamba/core/util.hpp"

#include "thirdparty/termcolor.hpp"

namespace mamba
{
    std::ostream& write_duration(std::ostream& os, std::chrono::nanoseconds ns)
    {
        using std::chrono::duration;
        using std::chrono::duration_cast;
        using std::chrono::hours;
        using std::chrono::minutes;
        using std::chrono::seconds;

        using days = duration<int, std::ratio<86400>>;
        char fill = os.fill();
        os.fill('0');
        auto d = duration_cast<days>(ns);
        ns -= d;
        auto h = duration_cast<hours>(ns);
        ns -= h;
        auto m = duration_cast<minutes>(ns);
        ns -= m;
        auto s = duration_cast<seconds>(ns);
        if (d.count() > 0)
        {
            os << std::setw(2) << d.count() << "d:";
        }
        if (h.count() > 0)
        {
            os << std::setw(2) << h.count() << "h:";
        }
        os << std::setw(2) << m.count() << "m:" << std::setw(2) << s.count() << 's';
        os.fill(fill);
        return os;
    }

    int get_console_width()
    {
#ifndef _WIN32
        struct winsize w;
        ioctl(0, TIOCGWINSZ, &w);
        return w.ws_col;
#else

        CONSOLE_SCREEN_BUFFER_INFO coninfo;
        auto res = GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &coninfo);
        return coninfo.dwSize.X;
#endif

        return -1;
    }

    std::string cut_repo_name(const std::string& full_url)
    {
        std::string remaining_url, scheme, auth, token;
        // TODO maybe add some caching...
        split_scheme_auth_token(full_url, remaining_url, scheme, auth, token);

        if (starts_with(remaining_url, "conda.anaconda.org/"))
        {
            return remaining_url.substr(19, std::string::npos).data();
        }
        if (starts_with(remaining_url, "repo.anaconda.com/"))
        {
            return remaining_url.substr(18, std::string::npos).data();
        }
        return remaining_url;
    }

    /***********
     * Table   *
     ***********/

    namespace printers
    {
        Table::Table(const std::vector<FormattedString>& header)
            : m_header(header)
        {
        }

        void Table::set_alignment(const std::vector<alignment>& a)
        {
            m_align = a;
        }

        void Table::set_padding(const std::vector<int>& p)
        {
            m_padding = p;
        }

        void Table::add_row(const std::vector<FormattedString>& r)
        {
            m_table.push_back(r);
        }

        void Table::add_rows(const std::string& header,
                             const std::vector<std::vector<FormattedString>>& rs)
        {
            m_table.push_back({ header });

            for (auto& r : rs)
                m_table.push_back(r);
        }

        std::ostream& Table::print(std::ostream& out)
        {
            if (m_table.size() == 0)
                return out;
            std::size_t n_col = m_header.size();

            if (m_align.size() == 0)
                m_align = std::vector<alignment>(n_col, alignment::left);

            std::vector<std::size_t> cell_sizes(n_col);
            for (size_t i = 0; i < n_col; ++i)
                cell_sizes[i] = m_header[i].size();

            for (size_t i = 0; i < m_table.size(); ++i)
            {
                if (m_table[i].size() == 1)
                    continue;
                for (size_t j = 0; j < m_table[i].size(); ++j)
                    cell_sizes[j] = std::max(cell_sizes[j], m_table[i][j].size());
            }

            if (m_padding.size())
            {
                for (std::size_t i = 0; i < n_col; ++i)
                    cell_sizes[i];
            }
            else
            {
                m_padding = std::vector<int>(n_col, 1);
            }

            std::size_t total_length = std::accumulate(cell_sizes.begin(), cell_sizes.end(), 0);
            total_length = std::accumulate(m_padding.begin(), m_padding.end(), total_length);

            auto print_row = [this, &cell_sizes, &out](const std::vector<FormattedString>& row) {
                for (size_t j = 0; j < row.size(); ++j)
                {
                    if (row[j].flag != format::none)
                    {
                        if (static_cast<std::size_t>(row[j].flag)
                            & static_cast<std::size_t>(format::red))
                            out << termcolor::red;
                        if (static_cast<std::size_t>(row[j].flag)
                            & static_cast<std::size_t>(format::green))
                            out << termcolor::green;
                        if (static_cast<std::size_t>(row[j].flag)
                            & static_cast<std::size_t>(format::yellow))
                            out << termcolor::yellow;
                    }
                    if (this->m_align[j] == alignment::left)
                    {
                        out << std::left;
                        for (int x = 0; x < this->m_padding[j]; ++x)
                            out << ' ';
                        out << std::setw(cell_sizes[j]) << row[j].s;
                    }
                    else
                    {
                        out << std::right << std::setw(cell_sizes[j] + m_padding[j]) << row[j].s;
                    }
                    if (row[j].flag != format::none)
                    {
                        out << termcolor::reset;
                    }
                }
            };

            print_row(m_header);

#ifdef _WIN32
#define MAMBA_TABLE_DELIM "-"
#else
#define MAMBA_TABLE_DELIM "─"
#endif

            out << "\n";
            for (size_t i = 0; i < total_length + m_padding[0]; ++i)
            {
                out << MAMBA_TABLE_DELIM;
            }
            out << "\n";

            for (size_t i = 0; i < m_table.size(); ++i)
            {
                if (m_table[i].size() == 1)
                {
                    // print header
                    if (i != 0)
                        out << "\n";

                    for (int x = 0; x < m_padding[0]; ++x)
                    {
                        out << ' ';
                    }
                    out << m_table[i][0].s;

                    out << "\n";
                    for (size_t i = 0; i < total_length + m_padding[0]; ++i)
                    {
                        out << MAMBA_TABLE_DELIM;
                    }
                    out << "\n";
                }
                else
                {
                    print_row(m_table[i]);
                }
                out << '\n';
            }
            out << std::flush;
            return out;
        }
    }  // namespace printers

    /*****************
     * ConsoleStream *
     *****************/

    ConsoleStream::~ConsoleStream()
    {
        Console::instance().print(str());
    }

    /***********
     * Console *
     ***********/

    Console::Console()
        : m_mutex()
        , p_progress_manager(make_progress_bar_manager(ProgressBarMode::multi))
    {
#ifdef _WIN32
        // initialize ANSI codes on Win terminals
        auto hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
        SetConsoleMode(hStdout, ENABLE_PROCESSED_OUTPUT | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
#endif
    }

    Console& Console::instance()
    {
        static Console c;
        return c;
    }

    ConsoleStream Console::stream()
    {
        return ConsoleStream();
    }

    void Console::print(const std::string_view& str, bool force_print)
    {
        if (!(Context::instance().quiet || Context::instance().json) || force_print)
        {
            const std::lock_guard<std::mutex> lock(instance().m_mutex);
            if (instance().p_progress_manager)
            {
                instance().p_progress_manager->print(str, instance().skip_progress_bars());
            }
            else
            {
                std::cout << str << std::endl;
            }
        }
    }

    bool Console::prompt(const std::string_view& message, char fallback)
    {
        if (Context::instance().always_yes)
        {
            return true;
        }
        while (!is_sig_interrupted())
        {
            std::cout << message << ": ";
            if (fallback == 'n')
            {
                std::cout << "[y/N] ";
            }
            else if (fallback == 'y')
            {
                std::cout << "[Y/n] ";
            }
            else
            {
                std::cout << "[y/n] ";
            }
            std::string response;
            std::getline(std::cin, response);
#ifdef _WIN32
            response = strip(response);
#endif
            if (response.size() == 0)
            {
                // enter pressed
                response = std::string(1, fallback);
            }
            if (response.compare("yes") == 0 || 
		response.compare("Yes") == 0||
		response.compare("y") == 0 ||
		response.compare("Y") == 0)
            {
                return true && !is_sig_interrupted();
            }
            if (response.compare("no") == 0 ||
		response.compare("No") == 0 || 
		response.compare("n") == 0 || 
		response.compare("N") == 0)
            {
                Console::print("Aborted.");
                return false;
            }
        }
        return false;
    }

    ProgressProxy Console::add_progress_bar(const std::string& name, size_t expected_total)
    {
        return p_progress_manager->add_progress_bar(name, expected_total);
    }

    void Console::init_multi_progress(ProgressBarMode mode)
    {
        p_progress_manager = make_progress_bar_manager(mode);
    }

    void Console::deactivate_progress_bar(std::size_t idx, const std::string_view& msg)
    {
        std::lock_guard<std::mutex> lock(instance().m_mutex);
        p_progress_manager->deactivate_progress_bar(idx, msg);
    }

    void Console::print_progress(std::size_t idx)
    {
        if (skip_progress_bars())
        {
            return;
        }

        std::lock_guard<std::mutex> lock(instance().m_mutex);
        p_progress_manager->print_progress(idx);
    }

    bool Console::skip_progress_bars() const
    {
        return Context::instance().quiet || Context::instance().json
               || Context::instance().no_progress_bars;
    }

    /*****************
     * MessageLogger *
     *****************/

    std::string strip_file_prefix(const std::string& file)
    {
#ifdef _WIN32
        char sep = '\\';
#else
        char sep = '/';
#endif
        size_t pos = file.rfind(sep);
        return pos != std::string::npos ? file.substr(pos + 1, std::string::npos) : file;
    }

    MessageLogger::MessageLogger(const char* file, int line, LogSeverity severity)
        : m_file(strip_file_prefix(file))
        , m_line(line)
        , m_severity(severity)
        , m_stream()
    {
#ifdef MAMBA_DEVELOPMENT
        m_stream << m_file << ":" << m_line << " ";
#endif
    }

    MessageLogger::~MessageLogger()
    {
        if (m_severity < global_log_severity())
        {
            return;
        }

        switch (m_severity)
        {
            case LogSeverity::kFatal:
                std::cerr << termcolor::on_red << "FATAL   " << termcolor::reset
                          << prepend(m_stream.str(), "", std::string(8, ' ').c_str()) << std::endl;
                break;
            case LogSeverity::kError:
                std::cerr << termcolor::red << "ERROR   " << termcolor::reset
                          << prepend(m_stream.str(), "", std::string(8, ' ').c_str()) << std::endl;
                break;
            case LogSeverity::kWarning:
                std::cerr << termcolor::yellow << "WARNING " << termcolor::reset
                          << prepend(m_stream.str(), "", std::string(8, ' ').c_str()) << std::endl;
                break;
            case LogSeverity::kInfo:
                std::cerr << "INFO    " << prepend(m_stream.str(), "", std::string(8, ' ').c_str())
                          << std::endl;
                break;
            case LogSeverity::kDebug:
                std::cerr << "DEBUG   " << prepend(m_stream.str(), "", std::string(8, ' ').c_str())
                          << std::endl;
                break;
            default:
                std::cerr << "UNKOWN  " << prepend(m_stream.str(), "", std::string(8, ' ').c_str())
                          << std::endl;
                break;
        }

        if (m_severity == LogSeverity::kFatal)
        {
            std::abort();
        }
    }

    std::stringstream& MessageLogger::stream()
    {
        return m_stream;
    }

    LogSeverity& MessageLogger::global_log_severity()
    {
        static LogSeverity sev = LogSeverity::kWarning;
        return sev;
    }

    /***************
     * JsonLogger *
     ***************/

    JsonLogger::JsonLogger()
    {
    }

    JsonLogger& JsonLogger::instance()
    {
        static JsonLogger j;
        return j;
    }

    // write all the key/value pairs of a JSON object into the current entry, which
    // is then a JSON object
    void JsonLogger::json_write(const nlohmann::json& j)
    {
        if (Context::instance().json)
        {
            nlohmann::json tmp = j.flatten();
            for (auto it = tmp.begin(); it != tmp.end(); ++it)
                json_log[json_hier + it.key()] = it.value();
        }
    }

    // append a value to the current entry, which is then a list
    void JsonLogger::json_append(const std::string& value)
    {
        if (Context::instance().json)
        {
            json_log[json_hier + '/' + std::to_string(json_index)] = value;
            json_index += 1;
        }
    }

    // append a JSON object to the current entry, which is then a list
    void JsonLogger::json_append(const nlohmann::json& j)
    {
        if (Context::instance().json)
        {
            nlohmann::json tmp = j.flatten();
            for (auto it = tmp.begin(); it != tmp.end(); ++it)
                json_log[json_hier + '/' + std::to_string(json_index) + it.key()] = it.value();
            json_index += 1;
        }
    }

    // go down in the hierarchy in the "key" entry, create it if it doesn't exist
    void JsonLogger::json_down(const std::string& key)
    {
        if (Context::instance().json)
        {
            json_hier += '/' + key;
            json_index = 0;
        }
    }

    // go up in the hierarchy
    void JsonLogger::json_up()
    {
        if (Context::instance().json)
            json_hier.erase(json_hier.rfind('/'));
    }
}  // namespace mamba
