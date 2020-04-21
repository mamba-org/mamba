#ifdef _WIN32
#include <windows.h>
#endif

#include "output.hpp"

namespace mamba
{
    /***********************
     * ConsoleStringStream *
     ***********************/

    ConsoleStringStream::~ConsoleStringStream()
    {
        Console::instance().print(str());
    }

    /*****************
     * ProgressProxy *
     *****************/
        
    ProgressProxy::ProgressProxy(indicators::ProgressBar* ptr, std::size_t idx)
        : p_bar(ptr)
        , m_idx(idx)
    {
    }

    void ProgressProxy::set_progress(std::size_t p)
    {
        p_bar->set_progress(p);
        Console::instance().print_progress(m_idx);
    }

    void ProgressProxy::mark_as_completed(const std::string_view& final_message)
    {
        // mark as completed should print bar or message at FIRST position!
        // then discard
        p_bar->mark_as_completed();
        Console::instance().deactivate_progress_bar(m_idx, final_message);
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

    ConsoleStringStream Console::print()
    {
        return ConsoleStringStream();
    }

    void Console::print(const std::string_view& str)
    {
        if (!(Context::instance().quiet || Context::instance().json))
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

        m_progress_bars.push_back(
            std::make_unique<indicators::ProgressBar>(
                indicators::option::BarWidth{15},
                indicators::option::ForegroundColor{indicators::Color::unspecified},
                indicators::option::ShowElapsedTime{true},
                indicators::option::ShowRemainingTime{false},
                indicators::option::MaxPostfixTextLen{36},
                indicators::option::PrefixText(prefix)
            )
        );

        m_progress_bars[m_progress_bars.size() - 1]->multi_progress_mode_ = true;
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
        auto it = std::find(m_active_progress_bars.begin(), m_active_progress_bars.end(), m_progress_bars[idx].get());
        if (it == m_active_progress_bars.end() || Context::instance().quiet || Context::instance().json)
        {
            return;
        }

        m_active_progress_bars.erase(it);
        if (Context::instance().no_progress_bars)
        {
            Console::print("Finished downloading "
                    + m_progress_bars[idx]->get_value<indicators::details::ProgressBarOption::prefix_text>());
        }
        else
        {
            int ps = m_active_progress_bars.size();
            std::cout << cursor::prev_line(ps + 1) << cursor::erase_line();
            if (msg.empty())
            {
                m_progress_bars[idx]->print_progress(true);
                std::cout << "\n";
            }
            else
            {
                std::cout << msg << std::endl;
            }
            print_progress_unlocked();
        }
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
            bar->print_progress(true);
            std::cout << "\n";
        }
    }

    bool Console::skip_progress_bars() const
    {
        return Context::instance().quiet || Context::instance().json || Context::instance().no_progress_bars;
    }
}

