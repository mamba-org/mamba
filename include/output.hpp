#include <iostream>
#include <string>

#include "context.hpp"

#ifdef _WIN32
#include <windows.h>
#endif

namespace mamba
{
    class NullBuffer : public std::streambuf
    {
    public:
      int overflow(int c) { return c; }
    };

    class NullStream : public std::ostream
    {
    public:
        NullStream()
            : std::ostream(&m_sb)
        {
        }

    private:
        NullBuffer m_sb;
    };

    class Output
    {
    public:
        static std::ostream& print()
        {
            if (Context::instance().quiet || Context::instance().json)
            {
                return Output::instance().null_stream;
            }
            else
            {
                return std::cout;
            }
        }

        static void print(const std::string_view& str)
        {
            if (!(Context::instance().quiet || Context::instance().json))
            {
                std::cout << str << std::endl;
            }
        }

        struct ProgressProxy
        {
            indicators::ProgressBar* p_bar;

            void set_progress(std::size_t p)
            {
                p_bar->set_progress(p);
                Output::instance().print_progress();
            }
            template <class T>
            void set_option(T&& option)
            {
                p_bar->set_option(std::forward<T>(option));
                Output::instance().print_progress();
            }
            void mark_as_completed()
            {
                p_bar->mark_as_completed();
                Output::instance().print_progress();
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
            return ProgressProxy{m_progress_bars[m_progress_bars.size() - 1].get()};
        }

        void init_multi_progress()
        {
            m_progress_bars.clear();
            m_progress_started = false;
        }

        void print_progress()
        {
            if (Context::instance().quiet || Context::instance().json) return;
            if (m_progress_started)
            {
                std::cout << "\x1b[" << m_progress_bars.size() << "A";
            }
            for (auto& bar : m_progress_bars)
            {
                bar->print_progress(true);
                std::cout << "\n";
            }
            m_progress_started = true;
        }

        static Output& instance()
        {
            static Output out;
            return out;
        }

        NullStream null_stream;

    private:
        Output() {
        #ifdef _WIN32
            // initialize ANSI codes on Win terminals
            auto hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
            SetConsoleMode(hStdout, ENABLE_PROCESSED_OUTPUT | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
        #endif
        }

        std::vector<std::unique_ptr<indicators::ProgressBar>> m_progress_bars;
        bool m_progress_started = false;
    };
}
