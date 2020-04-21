#ifndef MAMBA_OUTPUT_HPP
#define MAMBA_OUTPUT_HPP

#include "thirdparty/minilog.hpp"
#include "thirdparty/indicators/progress_bar.hpp"

#include <iostream>
#include <string>
#include <mutex>

#include "context.hpp"

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

    inline std::ostream& operator<<(std::ostream& o, const CursorMovementTriple& m)
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

    inline auto up(int n)
    {
        return CursorMovementTriple("up[", n, "A");
    }

    inline auto down(int n)
    {
        return CursorMovementTriple("\x1b[", n, "B");
    }

    inline auto forward(int n)
    {
        return CursorMovementTriple("\x1b[", n, "C");
    }

    inline auto back(int n)
    {
        return CursorMovementTriple("\x1b[", n, "D");
    }

    inline auto next_line(int n)
    {
        return CursorMovementTriple("\x1b[", n, "E");
    }

    inline auto prev_line(int n)
    { 
        return CursorMovementTriple("\x1b[", n, "F");
    }

    inline auto horizontal_abs(int n)
    {
        return CursorMovementTriple("\x1b[", n, "G");
    }

    inline auto erase_line(int n = 0)
    {
        return CursorMovementTriple("\x1b[", n, "K");
    }

    inline auto show(int n)
    {
        return CursorMod("\x1b[?25h");
    }

    inline auto hide(int n)
    {
        return CursorMod("\x1b[?25l");
    }
}

namespace mamba
{
    // Todo: replace public inheritance with
    // private one + using directives
    class ConsoleStringStream : public std::stringstream
    {
    public:

        ConsoleStringStream() = default;
        ~ConsoleStringStream();
    };

    class ProgressProxy
    {
    public:

        ProgressProxy() = default;
        ~ProgressProxy() = default;

        ProgressProxy(const ProgressProxy&) = default;
        ProgressProxy& operator=(const ProgressProxy&) = default;
        ProgressProxy(ProgressProxy&&) = default;
        ProgressProxy& operator=(ProgressProxy&&) = default;

        void set_progress(std::size_t p);
        
        template <class T>
        void set_option(T&& option);
        void mark_as_completed(const std::string_view& final_message = "");

    private:

        ProgressProxy(indicators::ProgressBar* ptr, std::size_t idx);

        indicators::ProgressBar* p_bar;
        std::size_t m_idx;

        friend class Console;
    };

    class Console
    {
    public:

        Console(const Console&) = delete;
        Console& operator=(const Console&) = delete;
        
        Console(Console&&) = delete;
        Console& operator=(Console&&) = delete;

        static Console& instance();

        static ConsoleStringStream print();
        static void print(const std::string_view& str);
        static bool prompt(const std::string_view& message, char fallback='_');

        ProgressProxy add_progress_bar(const std::string& name);
        void init_multi_progress();

    private:

        using progress_bar_ptr = std::unique_ptr<indicators::ProgressBar>;

        Console();
        ~Console() = default;

        void deactivate_progress_bar(std::size_t idx, const std::string_view& msg = "");
        void print_progress(std::size_t idx);
        void print_progress();
        void print_progress_unlocked();
        bool skip_progress_bars() const;
        
        std::mutex m_mutex;
        std::vector<progress_bar_ptr> m_progress_bars;
        std::vector<indicators::ProgressBar*> m_active_progress_bars;
        bool m_progress_started = false;

        friend class ProgressProxy;
    };

    template <class T>
    void ProgressProxy::set_option(T&& option)
    {
        p_bar->set_option(std::forward<T>(option));
        Console::instance().print_progress(m_idx);
    }
}

#endif

