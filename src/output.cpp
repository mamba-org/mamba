#ifdef _WIN32
#include <windows.h>
#else
#include <sys/ioctl.h>
#endif

#include <algorithm>
#include <cstdlib>

#include "thirdparty/termcolor.hpp"

#include "output.hpp"
#include "util.hpp"

namespace mamba
{
    std::ostream& write_duration(std::ostream &os, std::chrono::nanoseconds ns)
    {
        using namespace std::chrono;

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

    /***********************
     * ProgressScaleWriter *
     ***********************/

    ProgressScaleWriter::ProgressScaleWriter(int bar_width,
                                             const std::string& fill,
                                             const std::string& lead,
                                             const std::string& remainder)
        : m_bar_width(bar_width)
        , m_fill(fill)
        , m_lead(lead)
        , m_remainder(remainder)
    {
    }

    std::ostream& ProgressScaleWriter::write(std::ostream& os, std::size_t progress) const
    {
        auto pos = static_cast<size_t>(progress * m_bar_width / 100.0);
        for (size_t i = 0; i < m_bar_width; ++i)
        {
            if (i < pos)
            {
                os << m_fill;
            }
            else if (i == pos)
            {
                os << m_lead;
            }
            else
            {
                os << m_remainder;
            }
        }
        return os;
    }

    /***************
     * ProgressBar *
     ***************/

    ProgressBar::ProgressBar(const std::string& prefix)
        : m_prefix(prefix), m_start_time_saved(false)
    {
    }

    void ProgressBar::set_start()
    {
        m_start_time = std::chrono::high_resolution_clock::now();
        m_start_time_saved = true;
    }

    void ProgressBar::set_progress(char p)
    {
        if (!m_start_time_saved)
        {
            set_start();
        }

        if (p == -1)
        {
            m_activate_bob = true;
            m_progress += 5;
        }
        else
        {
            m_activate_bob = false;
            m_progress = p;
        }
    }

    void ProgressBar::set_postfix(const std::string& postfix_text)
    {
        m_postfix = postfix_text;
    }

    const std::string& ProgressBar::prefix() const
    {
        return m_prefix;
    }

    void ProgressBar::elapsed_time_to_stream(std::stringstream& s)
    {
        if (m_start_time_saved)
        {
            auto now = std::chrono::high_resolution_clock::now();
            m_elapsed_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(now - m_start_time);
            s << "(";
            write_duration(s, m_elapsed_ns);
            s << ") ";
        }
        else
        {
            s << "(--:--) ";
        }
    }

    void ProgressBar::print()
    {
        std::cout << cursor::erase_line(2) << "\r";
        std::cout << m_prefix << "[";

        std::stringstream pf;
        elapsed_time_to_stream(pf);
        pf << m_postfix;
        auto fpf = pf.str();
        int width = get_console_width();
        width = (width == -1) ? 20 : (std::min)(int(width - (m_prefix.size() + 4) - fpf.size()), 20);

        if (!m_activate_bob)
        {
            ProgressScaleWriter w{width, "=", ">", " "};
            w.write(std::cout, m_progress);
        }
        else
        {
            auto pos = static_cast<std::size_t>(m_progress * width / 100.0);
            for (size_t i = 0; i < width; ++i) {
              if (i == pos - 1)
              {
                std::cout << '<';
              }
              else if (i == pos)
              {
                std::cout << '=';
              }
              else if (i == pos + 1)
              {
                std::cout << '>';
              }
              else
              {
                std::cout << ' ';
              }
            }
        }
        std::cout << "] " << fpf;
    }

    void ProgressBar::mark_as_completed()
    {
        // todo
    }

    /*****************
     * ProgressProxy *
     *****************/

    ProgressProxy::ProgressProxy(ProgressBar* ptr, std::size_t idx)
        : p_bar(ptr)
        , m_idx(idx)
    {
    }

    void ProgressProxy::set_progress(char p)
    {
        p_bar->set_progress(p);
        Console::instance().print_progress(m_idx);
    }

    void ProgressProxy::elapsed_time_to_stream(std::stringstream& s)
    {
        p_bar->elapsed_time_to_stream(s);
    }

    void ProgressProxy::mark_as_completed(const std::string_view& final_message)
    {
        // mark as completed should print bar or message at FIRST position!
        // then discard
        p_bar->mark_as_completed();
        Console::instance().deactivate_progress_bar(m_idx, final_message);
    }

    std::string cut_repo_name(const std::string_view& reponame)
    {
        if (starts_with(reponame, "https://conda.anaconda.org/"))
        {
            return reponame.substr(27, std::string::npos).data();
        }
        if (starts_with(reponame, "https://repo.anaconda.com/"))
        {
            return reponame.substr(26, std::string::npos).data();
        }
        return reponame.data();
    }

    /***********
     * Table   *
     ***********/

    namespace printers
    {
        constexpr const char* green = "\033[32m";
        constexpr const char* red   = "\033[31m";
        constexpr const char* reset = "\033[00m";

        Table::Table(const std::vector<FormattedString>& header)
            : m_header(header)
        {
        }

        void Table::set_alignment(const std::vector<int>& a)
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

        void Table::add_rows(const std::string& header, const std::vector<std::vector<FormattedString>>& rs)
        {
            m_table.push_back({ header });

            for (auto& r : rs)
                m_table.push_back(r);
        }

        void Table::print()
        {
            if (m_table.size() == 0) return;
            std::size_t n_col = m_header.size();

            if (m_align.size() == 0) m_align = std::vector<int>(n_col, alignment::left);

            std::vector<std::size_t> cell_sizes(n_col);
            for (auto i = 0; i < n_col; ++i)
                cell_sizes[i] = m_header[i].size();

            for (auto i = 0; i < m_table.size(); ++i)
            {
                if (m_table[i].size() == 1) continue;
                for (auto j = 0; j < m_table[i].size(); ++j)
                    cell_sizes[j] = std::max(cell_sizes[j], m_table[i][j].size());
            }

            if (m_padding.size())
            {
                for (std::size_t i = 0; i < n_col; ++i) cell_sizes[i];
            }
            else
            {
                m_padding = std::vector<int>(n_col, 1);
            }

            std::size_t total_length = std::accumulate(cell_sizes.begin(), cell_sizes.end(), 0);
            total_length = std::accumulate(m_padding.begin(), m_padding.end(), total_length);

            auto print_row = [this, &cell_sizes](const std::vector<FormattedString>& row)
            {
                for (auto j = 0; j < row.size(); ++j)
                {
                    if (row[j].flag != 0)
                    {
                        if (row[j].flag & RED) std::cout << termcolor::red;
                        if (row[j].flag & GREEN) std::cout << termcolor::green;
                        if (row[j].flag & YELLOW) std::cout << termcolor::yellow;

                    }
                    if (this->m_align[j] & alignment::left)
                    {
                        std::cout << std::left;
                        for (std::size_t x = 0; x < this->m_padding[j]; ++x)
                            std::cout << ' ';
                        std::cout << std::setw(cell_sizes[j]) << row[j].s;
                    }
                    else
                    {
                        std::cout << std::right << std::setw(cell_sizes[j] + m_padding[j]) << row[j].s;
                    }
                    if (row[j].flag != 0)
                    {
                        std::cout << termcolor::reset;
                    }
                }
            };

            print_row(m_header);

            #ifdef _WIN32
            #define MAMBA_TABLE_DELIM "-"
            #else
            #define MAMBA_TABLE_DELIM "─"
            #endif

            std::cout << "\n";
            for (int i = 0; i < total_length + m_padding[0]; ++i) std::cout << MAMBA_TABLE_DELIM;
            std::cout << "\n";

            for (auto i = 0; i < m_table.size(); ++i)
            {
                if (m_table[i].size() == 1)
                {
                    // print header
                    if (i != 0) std::cout << "\n";

                    for (std::size_t x = 0; x < m_padding[0]; ++x) std::cout  << ' ';
                    std::cout << m_table[i][0].s;

                    std::cout << "\n";
                    for (int i = 0; i < total_length + m_padding[0]; ++i) std::cout << MAMBA_TABLE_DELIM;
                    std::cout << "\n";
                }
                else
                {
                    print_row(m_table[i]);
                }
                std::cout << "\n";
            }
        }
    }

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
            // print above the progress bars
            if (Console::instance().m_progress_started)
            {
                {
                    const std::lock_guard<std::mutex> lock(instance().m_mutex);
                    const auto& ps = instance().m_active_progress_bars.size();
                    std::cout << cursor::prev_line(ps) << cursor::erase_line()
                              << str << std::endl;
                }
                Console::instance().print_progress();
            }
            else
            {
                const std::lock_guard<std::mutex> lock(instance().m_mutex);
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
        char in;
        while (!Context::instance().sig_interrupt)
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
            in = std::cin.get();
            if (in == '\n')
            {
                // enter pressed
                in = fallback;
            }
            if (in == 'y' || in == 'Y')
            {
                return true && !Context::instance().sig_interrupt;
            }
            if (in == 'n' || in == 'N')
            {
                return false;
            }
        }
        return false;
    }

    ProgressProxy Console::add_progress_bar(const std::string& name)
    {
        std::string prefix = name;
        prefix.resize(PREFIX_LENGTH - 1, ' ');
        prefix += ' ';

        m_progress_bars.push_back(std::make_unique<ProgressBar>(prefix));

        return ProgressProxy(m_progress_bars[m_progress_bars.size() - 1].get(),
                             m_progress_bars.size() - 1);
    }

    void Console::init_multi_progress()
    {
        m_active_progress_bars.clear();
        m_progress_bars.clear();
        m_progress_started = false;
    }

    void Console::deactivate_progress_bar(std::size_t idx, const std::string_view& msg)
    {
        std::lock_guard<std::mutex> lock(instance().m_mutex);

        if (Context::instance().no_progress_bars && !(Context::instance().quiet || Context::instance().json))
        {
            std::cout << m_progress_bars[idx]->prefix() << " " << msg << "\n";
        }

        auto it = std::find(m_active_progress_bars.begin(), m_active_progress_bars.end(), m_progress_bars[idx].get());
        if (it == m_active_progress_bars.end() || Context::instance().quiet || Context::instance().json)
        {
            // if no_progress_bars is true, should return here as no progress bars are active
            return;
        }

        m_active_progress_bars.erase(it);
        int ps = m_active_progress_bars.size();
        std::cout << cursor::prev_line(ps + 1) << cursor::erase_line();
        if (msg.empty())
        {
            m_progress_bars[idx]->print();
            std::cout << "\n";
        }
        else
        {
            std::cout << msg << std::endl;
        }
        print_progress_unlocked();
    }

    void Console::print_progress(std::size_t idx)
    {
        if (skip_progress_bars())
        {
            return;
        }

        std::lock_guard<std::mutex> lock(instance().m_mutex);

        std::size_t cursor_up = m_active_progress_bars.size();
        if (m_progress_started && cursor_up > 0)
        {
            std::cout << cursor::prev_line(cursor_up);
        }

        auto it = std::find(m_active_progress_bars.begin(), m_active_progress_bars.end(), m_progress_bars[idx].get());
        if (it == m_active_progress_bars.end())
        {
            m_active_progress_bars.push_back(m_progress_bars[idx].get());
        }

        print_progress_unlocked();
        m_progress_started = true;
    }

    void Console::print_progress()
    {
        if (skip_progress_bars())
        {
            return;
        }

        std::lock_guard<std::mutex> lock(instance().m_mutex);
        if (m_progress_started)
        {
            print_progress_unlocked();
        }
    }

    void Console::print_progress_unlocked()
    {
        for (auto& bar : m_active_progress_bars)
        {
            bar->print();
            std::cout << "\n";
        }
    }

    bool Console::skip_progress_bars() const
    {
        return Context::instance().quiet || Context::instance().json || Context::instance().no_progress_bars;
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
        return pos != std::string::npos ?
                file.substr(pos + 1, std::string::npos) :
                file;
    }

    MessageLogger::MessageLogger(const char* file, int line, LogSeverity severity)
        : m_file(strip_file_prefix(file))
        , m_line(line)
        , m_severity(severity)
        , m_stream()
    {
        m_stream << m_file << ":" << m_line << " ";
    }

    MessageLogger::~MessageLogger()
    {
        if (m_severity < global_log_severity())
        {
            return;
        }

        switch(m_severity)
        {
        case LogSeverity::FATAL:
            Console::stream() << "\033[1;35m" << "FATAL   " << m_stream.str() << "\033[0m";
            break;
        case LogSeverity::ERROR:
            Console::stream() << "\033[1;31m" << "ERROR   " << m_stream.str() << "\033[0m";
            break;
        case LogSeverity::WARNING:
            Console::stream() << "\033[1;33m" << "WARNING " << m_stream.str() << "\033[0m";
            break;
        case LogSeverity::INFO:
            Console::stream()                 << "INFO    " << m_stream.str();
            break;
        case LogSeverity::DEBUG:
            Console::stream()                 << "DEBUG   " << m_stream.str();
            break;
        default:
            Console::stream()                 << "UNKOWN  " << m_stream.str();
            break;
        }

        if (m_severity == LogSeverity::FATAL)
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
        static LogSeverity sev = LogSeverity::WARNING;
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

    // write all the key/value pairs of a JSON object into the current entry, which is then a JSON object
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
}
