#pragma once

#include "thirdparty/minilog.hpp"
#include "thirdparty/indicators/progress_bar.hpp"

#include <iostream>
#include <string>
#include <mutex>

#include "context.hpp"

#ifdef _WIN32
#include <windows.h>
#endif

#define PREFIX_LENGTH 25

namespace cursor
{
    class CursorMovementTriple {
    public:
        CursorMovementTriple(const char* esc, int n, const char* mod)
            : m_esc(esc), m_mod(mod), m_n(n)
        {}

        const char* m_esc;
        const char* m_mod;
        int m_n;
    };

    std::ostream& operator<<(std::ostream& o, const CursorMovementTriple& m)
    {
        o << m.m_esc << m.m_n << m.m_mod;
        return o;
    }

    class CursorMod {
    public:
        CursorMod(const char* mod)
            : m_mod(mod)
        {}

        std::ostream& operator<<(std::ostream& o)
        {
            o << m_mod;
            return o;
        }

        const char* m_mod;
    };

    auto up(int n)
    {
        return CursorMovementTriple("up[", n, "A");
    }

    auto down(int n)
    {
        return CursorMovementTriple("\x1b[", n, "B");
    }

    auto forward(int n)
    {
        return CursorMovementTriple("\x1b[", n, "C");
    }

    auto back(int n)
    {
        return CursorMovementTriple("\x1b[", n, "D");
    }

    auto next_line(int n)
    {
        return CursorMovementTriple("\x1b[", n, "E");
    }

    auto prev_line(int n)
    { 
        return CursorMovementTriple("\x1b[", n, "F");
    }

    auto horizontal_abs(int n)
    {
        return CursorMovementTriple("\x1b[", n, "G");
    }

    auto erase_line(int n = 0)
    {
        return CursorMovementTriple("\x1b[", n, "K");
    }

    auto show(int n)
    {
        return CursorMod("\x1b[?25h");
    }

    auto hide(int n)
    {
        return CursorMod("\x1b[?25l");
    }
}

namespace mamba
{
    class Output
    {
    public:
        class SpecialStream : public std::stringstream
        {
        public:
            SpecialStream()
                : std::stringstream()
            {
            }

            ~SpecialStream()
            {
                Output::instance().print(str());
            }
        };

        static SpecialStream print()
        {
            return SpecialStream();
        }

        static void print(const std::string_view& str)
        {
            if (!(Context::instance().quiet || Context::instance().json))
            {
                // print above the progress bars
                if (Output::instance().m_progress_started)
                {
                    {
                        const std::lock_guard<std::mutex> lock(instance().m_mutex);
                        const auto& ps = instance().m_active_progress_bars.size();
                        std::cout << cursor::prev_line(ps) << cursor::erase_line()
                                  << str << std::endl;
                    }
                    Output::instance().print_progress(-1);
                }
                else
                {
                    const std::lock_guard<std::mutex> lock(instance().m_mutex);
                    std::cout << str << std::endl;
                }
            }
        }

        static bool prompt(const std::string_view& message, char fallback='_')
        {
            if (Context::instance().always_yes) {
                return true;
            }
            char in;
            while (!Context::instance().sig_interrupt) {
                std::cout << message << ": ";
                if (fallback == 'n') {
                    std::cout << "[y/N] ";
                }
                else if (fallback == 'y') {
                    std::cout << "[Y/n] ";
                }
                else {
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

        struct ProgressProxy
        {
            indicators::ProgressBar* p_bar;
            std::size_t m_idx;

            ProgressProxy() = default;

            ProgressProxy(indicators::ProgressBar* ptr, std::size_t idx)
                : p_bar(ptr), m_idx(idx)
            {
            }

            void set_progress(std::size_t p)
            {
                p_bar->set_progress(p);
                Output::instance().print_progress(m_idx);
            }
            template <class T>
            void set_option(T&& option)
            {
                p_bar->set_option(std::forward<T>(option));
                Output::instance().print_progress(m_idx);
            }

            void mark_as_completed(const std::string_view& final_message = "") const
            {
                // mark as completed should print bar or message at FIRST position!
                // then discard
                p_bar->mark_as_completed();
                Output::instance().deactivate_progress_bar(m_idx);

                if (Context::instance().quiet || Context::instance().json)
                {
                    return;
                }

                if (final_message.size())
                {
                    {
                        const std::lock_guard<std::mutex> lock(instance().m_mutex);
                        int ps = instance().m_active_progress_bars.size();
                        std::cout << cursor::prev_line(ps + 1) << cursor::erase_line()
                                  << final_message << std::endl;
                    }
                    Output::instance().print_progress(-1);
                }
                else if (Context::instance().no_progress_bars == false)
                {
                    {
                        const std::lock_guard<std::mutex> lock(instance().m_mutex);
                        int ps = instance().m_active_progress_bars.size();
                        std::cout << cursor::prev_line(ps + 1) << cursor::erase_line();
                        p_bar->print_progress(true);
                        std::cout << "\n";
                    }
                    Output::instance().print_progress(-1);
                }
                else if (Context::instance().no_progress_bars)
                {
                    Output::print("Finished downloading " + p_bar->get_value<indicators::details::ProgressBarOption::prefix_text>());
                }
            }
        };

        ProgressProxy add_progress_bar(const std::string& name)
        {
            std::string prefix = name;
            prefix.resize(PREFIX_LENGTH - 1, ' ');
            prefix += ' ';

            m_progress_bars.push_back(std::make_unique<indicators::ProgressBar>(
                indicators::option::BarWidth{15},
                indicators::option::ForegroundColor{indicators::Color::unspecified},
                indicators::option::ShowElapsedTime{true},
                indicators::option::ShowRemainingTime{false},
                indicators::option::MaxPostfixTextLen{36},
                indicators::option::PrefixText(prefix)
            ));

            m_progress_bars[m_progress_bars.size() - 1].get()->multi_progress_mode_ = true;
            return ProgressProxy(m_progress_bars[m_progress_bars.size() - 1].get(),
                                 m_progress_bars.size() - 1);
        }

        void init_multi_progress()
        {
            m_active_progress_bars.clear();
            m_progress_bars.clear();
            m_progress_started = false;
        }

        void deactivate_progress_bar(std::size_t idx)
        {
            const std::lock_guard<std::mutex> lock(instance().m_mutex);

            auto it = std::find(m_active_progress_bars.begin(), m_active_progress_bars.end(), m_progress_bars[idx].get());
            if (it != m_active_progress_bars.end())
            {
                m_active_progress_bars.erase(it);
            }
        }

        void print_progress(std::ptrdiff_t idx)
        {
            if (Context::instance().quiet || Context::instance().json || Context::instance().no_progress_bars) return;

            const std::lock_guard<std::mutex> lock(instance().m_mutex);
            std::size_t cursor_up = m_active_progress_bars.size();
            if (idx != -1)
            {
                auto it = std::find(m_active_progress_bars.begin(), m_active_progress_bars.end(), m_progress_bars[idx].get());
                if (it == m_active_progress_bars.end())
                {
                    m_active_progress_bars.push_back(m_progress_bars[idx].get());
                }
            }

            if (idx == -1 && m_progress_started)
            {
                for (auto& bar : m_active_progress_bars)
                {
                    bar->print_progress(true);
                    std::cout << "\n";
                }
                return;
            }

            if (m_progress_started && cursor_up > 0)
            {
                std::cout << cursor::prev_line(cursor_up);
            }

            for (auto& bar : m_active_progress_bars)
            {
                bar->print_progress(true);
                std::cout << "\n";
            }
            if (m_progress_started == false && idx != -1)
            {
                m_progress_started = true;
            }
        }

        static Output& instance()
        {
            static Output out;
            return out;
        }

    private:
        Output() {
        #ifdef _WIN32
            // initialize ANSI codes on Win terminals
            auto hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
            SetConsoleMode(hStdout, ENABLE_PROCESSED_OUTPUT | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
        #endif
        }

        std::mutex m_mutex;
        std::vector<std::unique_ptr<indicators::ProgressBar>> m_progress_bars;
        std::vector<indicators::ProgressBar*> m_active_progress_bars;
        bool m_progress_started = false;
    };
}
