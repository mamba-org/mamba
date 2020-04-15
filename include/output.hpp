#include <iostream>
#include <string>

#include "context.hpp"

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
            if (!Context::instance().quiet)
            {
                return std::cout;
            }
            else
            {
                return Output::instance().null_stream;
            }
        }

        static void print(const std::string_view& str)
        {
            if (!Context::instance().quiet)
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
            auto count_up = m_progress_bars.size();
            if (m_progress_started)
            {
                std::cout << "\x1b[" << count_up << "A";
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
        Output() {}

        std::vector<std::unique_ptr<indicators::ProgressBar>> m_progress_bars;
        bool m_progress_started = false;
    };
}