#include "mamba/core/context.hpp"

#include "progress_bar_impl.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <iomanip>
#include <utility>
#include <limits>
#include <random>
#include <sstream>

#include "mamba/core/execution.hpp"

namespace cursor
{
    class CursorMovementTriple
    {
    public:
        CursorMovementTriple(const char* esc, int n, const char* mod)
            : m_esc(esc)
            , m_mod(mod)
            , m_n(n)
        {
        }

        const char* m_esc;
        const char* m_mod;
        int m_n;
    };

    inline std::ostream& operator<<(std::ostream& o, const CursorMovementTriple& m)
    {
        o << m.m_esc << m.m_n << m.m_mod;
        return o;
    }

    class CursorMod
    {
    public:
        CursorMod(const char* mod)
            : m_mod(mod)
        {
        }

        std::ostream& operator<<(std::ostream& o)
        {
            o << m_mod;
            return o;
        }

        const char* m_mod;
    };

    inline std::ostream& operator<<(std::ostream& o, const CursorMod& m)
    {
        o << m.m_mod;
        return o;
    }

    inline auto up(int n)
    {
        return CursorMovementTriple("\x1b[", n, "A");
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

    inline auto home()
    {
        return CursorMod("\x1b[H");
    }

    inline auto erase_display(int n = 0)
    {
        return CursorMovementTriple("\x1b[", n, "J");
    }

    inline auto erase_line(int n = 0)
    {
        return CursorMovementTriple("\x1b[", n, "K");
    }

    inline auto scroll_up(int n = 1)
    {
        return CursorMovementTriple("\x1b[", n, "S");
    }

    inline auto scroll_down(int n = 1)
    {
        return CursorMovementTriple("\x1b[", n, "T");
    }

    inline auto show()
    {
        return CursorMod("\x1b[?25h");
    }

    inline auto hide()
    {
        return CursorMod("\x1b[?25l");
    }

    inline auto pos()
    {
        return CursorMod("\x1b[R");
    }

    inline auto delete_line(int n = 1)
    {
        return CursorMovementTriple("\x1b[", n, "M");
    }

    inline auto alternate_screen()
    {
        return CursorMod("\x1b[?1049h");
    }

    inline auto main_screen()
    {
        return CursorMod("\x1b[?1049l");
    }
}  // namespace cursor

namespace mamba
{
    void to_human_readable_filesize(std::ostream& o, double bytes, std::size_t precision)
    {
        const char* sizes[] = { " B", "kB", "MB", "GB", "TB", "PB" };
        int order = 0;
        while (bytes >= 1000 && order < (6 - 1))
        {
            order++;
            bytes = bytes / 1000;
        }
        o << std::fixed << std::setprecision(precision) << bytes << sizes[order];
    }

    std::string to_human_readable_filesize(double bytes, std::size_t precision)
    {
        std::stringstream o;
        to_human_readable_filesize(o, bytes, precision);
        return o.str();
    }


    namespace
    {
        void print_formatted_field_repr(std::ostream& ostream,
                                        FieldRepr& r,
                                        std::size_t& current_width,
                                        std::size_t max_width,
                                        const std::string& sep = " ",
                                        bool allow_overflow = false)
        {
            if (r && (!max_width || (current_width + r.width() <= max_width)))
            {
                ostream << sep << r.formatted_value(allow_overflow);
                current_width += r.width();
            }
        }

        void print_formatted_bar_repr(std::ostream& ostream,
                                      ProgressBarRepr& r,
                                      std::size_t width,
                                      bool with_endl = true)
        {
            std::stringstream sstream;
            std::size_t cumulated_width = 0;

            print_formatted_field_repr(sstream, r.prefix, cumulated_width, width, "");
            print_formatted_field_repr(sstream, r.progress, cumulated_width, width, " ", true);

            if (r.style.has_foreground())
            {
                ostream << fmt::format(r.style, "{}", sstream.str());
                sstream.str("");
            }

            print_formatted_field_repr(sstream, r.current, cumulated_width, width);
            print_formatted_field_repr(sstream, r.separator, cumulated_width, width);
            print_formatted_field_repr(sstream, r.total, cumulated_width, width);
            print_formatted_field_repr(sstream, r.speed, cumulated_width, width);
            print_formatted_field_repr(sstream, r.postfix, cumulated_width, width);
            print_formatted_field_repr(sstream, r.elapsed, cumulated_width, width);

            if (with_endl)
                sstream << "\n";

            if (r.style.has_foreground())
                ostream << fmt::format(r.style, "{}", sstream.str());
            else
                ostream << fmt::format("{}", sstream.str());
        }
    }

    /**********
     * Chrono *
     **********/

    bool Chrono::started() const
    {
        return m_state == ChronoState::started;
    }

    bool Chrono::paused() const
    {
        return m_state == ChronoState::paused;
    }

    bool Chrono::stopped() const
    {
        return m_state == ChronoState::stopped;
    }

    bool Chrono::terminated() const
    {
        return m_state == ChronoState::terminated;
    }

    bool Chrono::unset() const
    {
        return m_state == ChronoState::unset;
    }

    ChronoState Chrono::status() const
    {
        return m_state;
    }

    void Chrono::start()
    {
        start(now());
    }

    void Chrono::start(const time_point_t& time_point)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_start = time_point;
        m_state = ChronoState::started;
    }

    void Chrono::pause()
    {
        compute_elapsed();
        std::lock_guard<std::mutex> lock(m_mutex);
        m_state = ChronoState::paused;
    }

    void Chrono::resume()
    {
        if (m_state != ChronoState::started)
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_state = ChronoState::started;
            m_start = now() - m_elapsed_ns;
        }
    }

    void Chrono::stop()
    {
        compute_elapsed();
        std::lock_guard<std::mutex> lock(m_mutex);
        m_state = ChronoState::stopped;
    }

    void Chrono::terminate()
    {
        compute_elapsed();
        std::lock_guard<std::mutex> lock(m_mutex);
        m_state = ChronoState::terminated;
    }

    auto Chrono::last_active_time() -> time_point_t
    {
        return m_start + m_elapsed_ns;
    }

    auto Chrono::elapsed() -> duration_t
    {
        compute_elapsed();
        return m_elapsed_ns;
    }

    void Chrono::set_elapsed_time(const duration_t& time)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_elapsed_ns = time;
        m_start = now() - time;
    }

    std::string Chrono::elapsed_time_to_str()
    {
        std::stringstream stream;
        if (m_state != ChronoState::unset)
            write_duration(stream, elapsed());
        else
            stream << "--";

        return stream.str();
    }

    auto Chrono::start_time() const -> time_point_t
    {
        return m_start;
    }

    void Chrono::set_start_time(const time_point_t& time_point)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_elapsed_ns = m_start - time_point;
        m_start = time_point;
    }

    void Chrono::compute_elapsed()
    {
        if (m_state == ChronoState::started)
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_elapsed_ns = now() - m_start;
        }
    }

    std::unique_lock<std::mutex> Chrono::chrono_lock()
    {
        return std::unique_lock<std::mutex>(m_mutex);
    }

    auto Chrono::now() -> time_point_t
    {
        return std::chrono::time_point_cast<duration_t>(std::chrono::high_resolution_clock::now());
    }

    /*************
     * FieldRepr *
     *************/

    bool FieldRepr::defined() const
    {
        return width() > 0;
    }

    FieldRepr::operator bool() const
    {
        return defined();
    }

    bool FieldRepr::active() const
    {
        return m_active;
    }

    FieldRepr& FieldRepr::activate()
    {
        m_active = true;
        return *this;
    }

    FieldRepr& FieldRepr::deactivate()
    {
        m_active = false;
        return *this;
    }

    bool FieldRepr::overflow() const
    {
        return m_value.size() > m_width;
    }

    std::string FieldRepr::formatted_value(bool allow_overflow) const
    {
        auto w = width();
        std::string val;
        if (!allow_overflow && overflow())
            val = resize(m_value, w);
        else
            val = m_value;

        if (active() && w)
            if (m_format.empty())
                return fmt::format("{:<{}}", val, w);
            else
                return fmt::format(m_format, val, w);
        else
            return "";
    }

    std::string FieldRepr::value() const
    {
        return m_value;
    }

    std::size_t FieldRepr::width(bool allow_overflow) const
    {
        if (m_active)
        {
            if (m_width || !allow_overflow)
                return m_width;
            return m_value.size();
        }
        else
            return 0;
    }

    std::size_t FieldRepr::stored_width() const
    {
        return m_width;
    }

    FieldRepr& FieldRepr::set_format(const std::string& str)
    {
        m_format = str;
        return *this;
    }

    FieldRepr& FieldRepr::set_format(const std::string& str, std::size_t size)
    {
        m_format = str;
        m_width = size;
        return *this;
    }

    FieldRepr& FieldRepr::set_value(const std::string& str)
    {
        m_value = str;
        return *this;
    }

    FieldRepr& FieldRepr::set_width(std::size_t size)
    {
        m_width = size;
        return *this;
    }

    FieldRepr& FieldRepr::reset_width()
    {
        m_width = 0;
        return *this;
    }

    FieldRepr& FieldRepr::resize(std::size_t size)
    {
        m_value = resize(m_value, size);
        return *this;
    }

    std::string FieldRepr::resize(const std::string& str, std::size_t size)
    {
        if (str.size() > size)
            return str.substr(0, size - 2) + "..";
        else
            return str;
    }

    /*******************
     * ProgressBarRepr *
     *******************/

    ProgressBarRepr::ProgressBarRepr(ProgressBar* pbar)
        : p_progress_bar(pbar)
    {
    }

    ProgressBar& ProgressBarRepr::progress_bar()
    {
        return *p_progress_bar;
    }

    ProgressBarRepr& ProgressBarRepr::set_width(std::size_t width)
    {
        m_width = width;
        return *this;
    }

    std::size_t ProgressBarRepr::width() const
    {
        return m_width;
    }

    void ProgressBarRepr::print(std::ostream& ostream, std::size_t width, bool with_endl)
    {
        print_formatted_bar_repr(ostream, *this, width, with_endl);
    }

    void ProgressBarRepr::set_same_widths(const ProgressBarRepr& r)
    {
        prefix.set_width(r.prefix.width());
        progress.set_width(r.progress.width());
        current.set_width(r.current.width());
        separator.set_width(r.separator.width());
        total.set_width(r.total.width());
        speed.set_width(r.speed.width());
        postfix.set_width(r.postfix.width());
        elapsed.set_width(r.elapsed.width());

        if (!r.current.active())
            current.deactivate();
        if (!r.separator.active())
            separator.deactivate();
        if (!r.total.active())
            total.deactivate();
        if (!r.speed.active())
            speed.deactivate();
        if (!r.postfix.active())
            postfix.deactivate();
        if (!r.elapsed.active())
            elapsed.deactivate();
    }

    void ProgressBarRepr::compute_progress()
    {
        compute_progress_width();
        compute_progress_value();
    }

    namespace
    {
        class ProgressScaleWriter
        {
        public:
            ProgressScaleWriter(std::size_t bar_width)
                : m_bar_width(bar_width)
            {
            }

            template <class T>
            static void format_progress(T& sstream,
                                        fmt::text_style color,
                                        std::size_t width,
                                        bool end)
            {
                if (width == 0)
                    return;
                if (!Context::instance().ascii_only)
                {
                    if (end)
                        sstream << fmt::format(color, "{:━>{}}", "", width);
                    else
                        sstream << fmt::format(color, "{:━>{}}╸", "", width - 1);
                }
                else
                {
                    sstream << fmt::format(color, "{:->{}}", "", width);
                }
            }

            std::string repr(std::size_t progress, std::size_t in_progress = 0) const
            {
                auto current_pos = static_cast<std::size_t>(progress * m_bar_width / 100.0);
                auto in_progress_pos
                    = static_cast<std::size_t>((progress + in_progress) * m_bar_width / 100.0);

                current_pos = std::clamp(current_pos, std::size_t(0), m_bar_width);
                in_progress_pos = std::clamp(in_progress_pos, std::size_t(0), m_bar_width);

                std::ostringstream oss;

                ProgressScaleWriter::format_progress(
                    oss, fmt::text_style(), current_pos, current_pos == m_bar_width);
                if (in_progress_pos && in_progress_pos > current_pos)
                {
                    ProgressScaleWriter::format_progress(oss,
                                                         fmt::fg(fmt::terminal_color::yellow),
                                                         in_progress_pos - current_pos,
                                                         in_progress_pos == m_bar_width);
                }
                ProgressScaleWriter::format_progress(
                    oss,
                    fmt::fg(fmt::terminal_color::bright_black),
                    m_bar_width - (in_progress_pos ? in_progress_pos : current_pos),
                    true);

                return oss.str();
            }

        private:
            std::size_t m_bar_width;
        };
    }  // namespace

    void ProgressBarRepr::compute_progress_width()
    {
        std::size_t max_width;

        if (m_width)
            max_width = m_width;
        else
        {
            int console_width = get_console_width();
            if (console_width != -1)
                max_width = console_width;
            else
                max_width = 100;
        }

        progress.set_width(40);
        std::size_t total_width = prefix.width() + progress.width() + current.width()
                                  + separator.width() + total.width() + speed.width()
                                  + postfix.width() + elapsed.width() + 1;

        // Add extra whitespaces between fields (prefix, progress,
        // and elasped fields are assumed always displayed)
        if (current)
            total_width += 1;
        if (separator)
            total_width += 1;
        if (total)
            total_width += 1;
        if (speed)
            total_width += 1;
        if (postfix)
            total_width += 1;
        if (elapsed)
            total_width += 1;

        // Reduce some fields to fit console width
        // 1: reduce bar width
        if (max_width < total_width && progress)
        {
            total_width = total_width - progress.width() + 15;
            progress.set_width(15);
        }
        // 2: remove the total value and the separator
        if (max_width < total_width && total)
        {
            total_width = total_width - total.width() - separator.width() - 2;
            total.deactivate();
            separator.deactivate();
        }
        // 3: remove the speed
        if (max_width < total_width && speed)
        {
            total_width = total_width - speed.width() - 1;
            speed.deactivate();
        }
        // 4: remove the postfix
        if (max_width < total_width && postfix)
        {
            total_width = total_width - postfix.width() - 1;
            postfix.deactivate();
        }
        std::size_t prefix_min_width = prefix.width();
        // 5: truncate the prefix if too long
        if (max_width < total_width && prefix.width() > 20 && prefix)
        {
            // keep a minimal size to make it readable
            total_width = total_width - prefix.width() + 20;
            prefix.set_width(20);
        }
        // 6: display progress without a bar
        if (max_width < total_width && progress)
        {
            // keep capability to display progress up to "100%"
            total_width = total_width - progress.width() + 4;
            progress.set_width(4);
        }
        // 7: remove the current value
        if (max_width < total_width && current)
        {
            total_width = total_width - current.width() - 1;
            current.deactivate();
        }
        // 8: remove the elapsed time
        if (max_width < total_width && elapsed)
        {
            total_width = total_width - elapsed.width() - 1;
            elapsed.deactivate();
        }

        // Redistribute available space
        // 1: start with the prefix if it was shrinked
        if (total_width < max_width && prefix && prefix.width() < prefix_min_width)
        {
            if ((max_width - total_width) < (prefix_min_width - prefix.width()))
            {
                prefix.set_width(prefix.width() + (max_width - total_width));
                total_width = max_width;
            }
            else
            {
                total_width += prefix_min_width - prefix.width();
                prefix.set_width(prefix_min_width);
            }
        }
        // 2: give the remaining free space to the progress bar
        if (total_width < max_width)
        {
            progress.set_width(progress.width() + (max_width - total_width));
            total_width = max_width;
        }
    }

    std::vector<FieldRepr*> ProgressBarRepr::fields()
    {
        return { &prefix, &progress, &current, &separator, &total, &speed, &postfix, &elapsed };
    }

    ProgressBarRepr& ProgressBarRepr::reset_fields()
    {
        for (auto& f : fields())
            f->set_format("{:>{}}").activate().set_width(0);
        prefix.set_format("{:<{}}");

        return *this;
    }

    void ProgressBarRepr::compute_progress_value()
    {
        std::stringstream sstream;
        auto width = progress.width(false);

        if (!p_progress_bar->is_spinner())
        {
            if (width < 12)
            {
                sstream << std::ceil(p_progress_bar->progress()) << "%";
            }
            else
            {
                ProgressScaleWriter w{ width };
                double in_progress
                    = static_cast<double>(p_progress_bar->current() + p_progress_bar->in_progress())
                      / static_cast<double>(p_progress_bar->total()) * 100.;
                sstream << w.repr(p_progress_bar->progress(), in_progress);
            }
        }
        else
        {
            if (width < 12)
            {
                std::vector<std::string> spinner;
                if (!Context::instance().ascii_only)
                    spinner = { "⣾", "⣽", "⣻", "⢿", "⣿", "⡿", "⣟", "⣯", "⣷", "⣿" };
                else
                    spinner = { "|", "/", "-", "|", "\\", "|", "/", "-", "|", "\\" };

                constexpr int spinner_rounds = 2;
                auto pos = static_cast<std::size_t>(
                               std::round(progress * ((spinner_rounds * spinner.size()) / 100.0)))
                           % spinner.size();
                sstream << fmt::format("{:^4}", spinner[pos]);
            }
            else
            {
                int pos = static_cast<int>(
                    std::round(p_progress_bar->progress() * (width - 1) / 100.0));

                std::size_t current_pos = 0, in_progress_pos = 0;

                if (p_progress_bar->total())
                {
                    current_pos = static_cast<int>(
                        std::floor(static_cast<double>(p_progress_bar->current())
                                   / static_cast<double>(p_progress_bar->total()) * width));
                    in_progress_pos = static_cast<int>(
                        std::ceil(static_cast<double>(p_progress_bar->current()
                                                      + p_progress_bar->in_progress())
                                  / static_cast<double>(p_progress_bar->total()) * width));

                    current_pos = std::clamp(current_pos, std::size_t(0), width);
                    in_progress_pos = std::clamp(in_progress_pos, std::size_t(0), width);
                }

                auto spinner_width = 8;

                if (current_pos)
                {
                    ProgressScaleWriter::format_progress(
                        sstream, fmt::text_style(), current_pos, current_pos == width);
                    if (in_progress_pos && in_progress_pos > current_pos)
                        ProgressScaleWriter::format_progress(sstream,
                                                             fmt::fg(fmt::terminal_color::yellow),
                                                             in_progress_pos - current_pos,
                                                             in_progress_pos == width);
                    ProgressScaleWriter::format_progress(
                        sstream,
                        fmt::fg(fmt::terminal_color::bright_black),
                        width - (in_progress_pos ? in_progress_pos : current_pos),
                        true);
                }
                else
                {
                    std::size_t spinner_start, spinner_length, rest;

                    spinner_start = pos > spinner_width ? pos - spinner_width : 0;
                    spinner_length = ((pos + spinner_width) < width ? pos + spinner_width : width)
                                     - spinner_start;

                    ProgressScaleWriter::format_progress(
                        sstream, fmt::fg(fmt::terminal_color::bright_black), spinner_start, false);
                    ProgressScaleWriter::format_progress(sstream,
                                                         fmt::fg(fmt::terminal_color::yellow),
                                                         spinner_length,
                                                         spinner_start + spinner_length == width);
                    if (spinner_length + spinner_start < width)
                    {
                        rest = width - spinner_start - spinner_length;
                        ProgressScaleWriter::format_progress(
                            sstream, fmt::fg(fmt::terminal_color::bright_black), rest, true);
                    }
                }
            }
        }

        progress.set_value(sstream.str());
    }

    /**********************
     * ProgressBarManager *
     **********************/

    ProgressBarManager::ProgressBarManager(std::size_t width)
        : m_width(width)
    {
    }

    ProgressBarManager::~ProgressBarManager()
    {
        if (m_watch_print_started)
            terminate();
    }

    std::unique_ptr<ProgressBarManager> make_progress_bar_manager(ProgressBarMode mode)
    {
        if (mode == ProgressBarMode::multi)
        {
            return std::make_unique<MultiBarManager>();
        }
        return std::make_unique<AggregatedBarManager>();
    }

    void ProgressBarManager::clear_progress_bars()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_labels.clear();
        m_progress_bars.clear();
    }

    ProgressBar* ProgressBarManager::raw_bar(const ProgressProxy& progress_bar)
    {
        return progress_bar.p_bar;
    }

    void ProgressBarManager::erase_lines(std::ostream& ostream, std::size_t count)
    {
        for (std::size_t i = 0; i < count; ++i)
            ostream << cursor::erase_line(2) << cursor::up(1);

        call_print_hooks(ostream);
    }

    void ProgressBarManager::call_print_hooks(std::ostream& ostream)
    {
        ostream << cursor::erase_line(2) << cursor::horizontal_abs(0);
        for (auto& f : m_print_hooks)
            f(ostream);
    }

    void ProgressBarManager::compute_bars_progress(std::vector<ProgressBar*>& bars)
    {
        if (!bars.empty())
        {
            std::size_t prefix_w = 0, current_w = 0, separator_w = 0, total_w = 0, speed_w = 0,
                        postfix_w = 0, elapsed_w = 0;

            for (auto& b : bars)
            {
                auto& r = b->repr();
                r.reset_fields().set_width(m_width);
                b->update_repr(false);

                prefix_w = std::max(r.prefix.value().size(), prefix_w);
                current_w = std::max(r.current.value().size(), current_w);
                separator_w = std::max(r.separator.value().size(), separator_w);
                total_w = std::max(r.total.value().size(), total_w);
                speed_w = std::max(r.speed.value().size(), speed_w);
                postfix_w = std::max(r.postfix.value().size(), postfix_w);
                elapsed_w = std::max(r.elapsed.value().size(), elapsed_w);
            }

            auto& r0 = bars[0]->repr();
            r0.prefix.set_width(prefix_w);
            r0.current.set_width(current_w);
            r0.separator.set_width(separator_w);
            r0.total.set_width(total_w);
            r0.speed.set_width(speed_w);
            r0.postfix.set_width(postfix_w);
            r0.elapsed.set_width(elapsed_w);
            r0.compute_progress();

            for (auto& b : bars)
            {
                b->repr().set_same_widths(r0);
                b->repr().compute_progress_value();
            }
        }
    }

    void ProgressBarManager::run()
    {
        auto time = start_time();
        bool watch = m_period > duration_t::zero();
        std::size_t previously_printed = 0;
        std::cout << cursor::hide();

        do
        {
            std::stringstream ostream;
            auto duration = time - start_time();

            erase_lines(ostream, previously_printed);
            if (m_marked_to_terminate)
            {
                std::cout << ostream.str() << cursor::show() << std::flush;
                m_marked_to_terminate = false;
                break;
            }

            ostream << "[+] " << std::fixed << std::setprecision(1) << duration_str(duration)
                    << "\n";
            previously_printed
                = std::max(print(ostream, 0, get_console_height() - 1, false), std::size_t(1));
            std::cout << ostream.str() << std::flush;

            auto now = std::chrono::high_resolution_clock::now();
            while (now > time)
                time += m_period;

            if (watch)
                std::this_thread::sleep_until(time);
        } while (started() && watch);

        m_watch_print_started = false;
    }

    void ProgressBarManager::watch_print(const duration_t& period)
    {
        m_period = period;
        start();
        m_marked_to_terminate = false;
        m_watch_print_started = true;
        MainExecutor::instance().schedule([&] { run(); });
    }

    void ProgressBarManager::start()
    {
        for (auto& f : m_pre_start_hooks)
            f();

        Chrono::start();
    }

    void ProgressBarManager::terminate()
    {
        if (!terminated())
        {
            if (m_watch_print_started)
            {
                m_marked_to_terminate = true;
                while (m_marked_to_terminate)
                    std::this_thread::sleep_for(m_period / 2);
            }

            Chrono::terminate();

            for (auto& f : m_post_stop_hooks)
                f();
        }
    }

    void ProgressBarManager::add_label(const std::string& label, const ProgressProxy& progress_bar)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto& p : m_progress_bars)
            if (p.get() == raw_bar(progress_bar))
            {
                if (m_labels.count(label) == 0)
                    m_labels.insert({ label, { raw_bar(progress_bar) } });
                else
                    m_labels[label].push_back(raw_bar(progress_bar));

                break;
            }
    }

    void ProgressBarManager::register_print_hook(std::function<void(std::ostream&)> f)
    {
        m_print_hooks.push_back(f);
    }

    void ProgressBarManager::register_pre_start_hook(std::function<void()> f)
    {
        m_pre_start_hooks.push_back(f);
    }

    void ProgressBarManager::register_post_stop_hook(std::function<void()> f)
    {
        m_post_stop_hooks.push_back(f);
    }

    void ProgressBarManager::activate_sorting()
    {
        m_sort_bars = true;
    }

    void ProgressBarManager::deactivate_sorting()
    {
        m_sort_bars = false;
    }

    void ProgressBarManager::sort_bars(bool max_height_exceeded)
    {
        if (!max_height_exceeded)
            std::sort(m_progress_bars.begin(),
                      m_progress_bars.end(),
                      [](auto& a, auto& b) { return a->prefix() > b->prefix(); });
        else
            std::sort(
                m_progress_bars.begin(),
                m_progress_bars.end(),
                [](auto& a, auto& b)
                {
                    if (!a->started() && b->started())
                        return false;
                    if (!b->started() && a->started())
                        return true;
                    if (a->status() == ChronoState::unset && b->status() != ChronoState::unset)
                        return true;
                    if (b->status() == ChronoState::unset && a->status() != ChronoState::unset)
                        return false;
                    return a->last_active_time() > b->last_active_time();
                });
    }

    /***************
     * ProgressBar *
     ***************/

    ProgressBar::ProgressBar(const std::string& prefix, std::size_t total, int width)
        : m_progress(0)
        , m_total(total)
        , m_width(width)
        , m_repr(this)
        , m_is_spinner(false)
    {
        m_repr.prefix.set_value(prefix);
    }

    ProgressBar::~ProgressBar()
    {
        terminate();
        std::lock_guard<std::mutex> lock(m_mutex);
    }

    ProgressBar& ProgressBar::set_progress(std::size_t current, std::size_t total)
    {
        m_current = current;
        m_total = total;

        if (!m_is_spinner && total && total != std::numeric_limits<std::size_t>::max())
        {
            if (current < total)
                m_progress = static_cast<double>(current) / static_cast<double>(total) * 100.;
            else
                set_full();
        }
        else
        {
            m_progress = (static_cast<int>(m_progress) + 5) % 100;
        }
        return *this;
    }

    ProgressBar& ProgressBar::update_progress(std::size_t current, std::size_t total)
    {
        if (!started())
            start();

        set_progress(current, total);
        return *this;
    }

    ProgressBar& ProgressBar::set_progress(double progress)
    {
        m_progress = progress;
        m_current = m_total * progress / 100.;
        set_current(m_current);
        return *this;
    }

    ProgressBar& ProgressBar::set_current(std::size_t current)
    {
        set_progress(current, m_total);
        return *this;
    }

    ProgressBar& ProgressBar::set_in_progress(std::size_t in_progress)
    {
        m_in_progress = in_progress;
        return *this;
    }

    ProgressBar& ProgressBar::update_current(std::size_t current)
    {
        update_progress(current, m_total);
        return *this;
    }

    ProgressBar& ProgressBar::set_total(std::size_t total)
    {
        set_progress(m_current, total);
        return *this;
    }

    ProgressBar& ProgressBar::set_full()
    {
        if (m_total && m_total < std::numeric_limits<std::size_t>::max())
            m_current = m_total;
        else
            m_total = m_current;
        m_is_spinner = false;
        m_progress = 100.;
        return *this;
    }

    ProgressBar& ProgressBar::set_speed(std::size_t speed)
    {
        m_speed = speed;
        return *this;
    }

    ProgressBar& ProgressBar::activate_spinner()
    {
        if (!m_is_spinner)
        {
            unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
            std::default_random_engine generator(seed);
            std::uniform_int_distribution<int> distribution(0, 100);
            m_progress = distribution(generator);
        }
        m_is_spinner = true;
        return *this;
    }

    ProgressBar& ProgressBar::deactivate_spinner()
    {
        if (m_current < m_total && m_total)
            m_progress = static_cast<double>(m_current) / static_cast<double>(m_total) * 100.;
        else
            set_full();
        m_is_spinner = false;
        return *this;
    }

    std::size_t ProgressBar::current() const
    {
        return m_current;
    }

    std::size_t ProgressBar::in_progress() const
    {
        return m_in_progress;
    }

    std::size_t ProgressBar::total() const
    {
        return m_total;
    }

    std::size_t ProgressBar::speed() const
    {
        return m_speed;
    }

    std::size_t ProgressBar::avg_speed(const std::chrono::milliseconds& ref_duration)
    {
        if (started())
        {
            auto now = Chrono::now();
            auto elapsed_since_last_avg
                = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_avg_speed_time);
            auto total_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed());

            if (ref_duration <= elapsed_since_last_avg && elapsed_since_last_avg.count())
            {
                if (total_elapsed < ref_duration && total_elapsed.count())
                    m_avg_speed = m_current / total_elapsed.count() * 1000;
                else
                    m_avg_speed
                        = (m_current - m_current_avg) / elapsed_since_last_avg.count() * 1000;
                m_avg_speed_time = now;
                m_current_avg = m_current;
            }
        }
        else
            m_avg_speed = 0;

        return m_avg_speed;
    }

    double ProgressBar::progress() const
    {
        return m_progress;
    }

    bool ProgressBar::completed() const
    {
        return m_completed;
    }

    bool ProgressBar::is_spinner() const
    {
        return m_is_spinner;
    }

    const std::set<std::string>& ProgressBar::active_tasks() const
    {
        return m_active_tasks;
    }

    std::set<std::string>& ProgressBar::active_tasks()
    {
        return m_active_tasks;
    }

    const std::set<std::string>& ProgressBar::all_tasks() const
    {
        return m_all_tasks;
    }

    std::set<std::string>& ProgressBar::all_tasks()
    {
        return m_all_tasks;
    }

    std::string ProgressBar::last_active_task()
    {
        auto now = Chrono::now();
        if (((now - m_task_time) < std::chrono::milliseconds(330)) && !m_last_active_task.empty()
            && m_active_tasks.count(m_last_active_task))
            return m_last_active_task;

        m_task_time = now;
        if (m_active_tasks.empty())
            m_last_active_task = "";
        else if (m_active_tasks.size() == 1)
            m_last_active_task = *m_active_tasks.begin();
        else
        {
            auto it = m_active_tasks.find(m_last_active_task);
            if (std::distance(it, m_active_tasks.end()) <= 1)
            {
                m_last_active_task = *m_active_tasks.begin();
            }
            else
            {
                it++;
                m_last_active_task = *it;
            }
        }
        return m_last_active_task;
    }

    ProgressBar& ProgressBar::add_active_task(const std::string& name)
    {
        m_active_tasks.insert(name);
        m_all_tasks.insert(name);
        return *this;
    }

    ProgressBar& ProgressBar::add_task(const std::string& name)
    {
        m_all_tasks.insert(name);
        return *this;
    }

    ProgressBar& ProgressBar::mark_as_completed(const std::chrono::milliseconds& delay)
    {
        pause();
        set_full();

        const time_point_t stop_time_point
            = now() + delay;  // FIXME: can be captured by the lambda?

        if (delay.count())
        {
            MainExecutor::instance().schedule(
                [&](const time_point_t& stop_time_point)
                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    while (now() < stop_time_point && status() < ChronoState::stopped)
                        std::this_thread::sleep_for(std::chrono::milliseconds(100));

                    this->m_completed = true;
                    stop();
                },
                stop_time_point);
        }
        else
        {
            stop();
            m_completed = true;
        }
        return *this;
    }

    ProgressBar& ProgressBar::set_prefix(const std::string& str)
    {
        m_repr.prefix.set_value(str);
        return *this;
    }

    ProgressBar& ProgressBar::set_postfix(const std::string& str)
    {
        m_repr.postfix.set_value(str);
        return *this;
    }

    ProgressBar& ProgressBar::set_repr_hook(std::function<void(ProgressBarRepr&)> f)
    {
        p_repr_hook = f;
        return *this;
    }

    ProgressBar& ProgressBar::set_progress_hook(std::function<void(ProgressProxy&)> f)
    {
        p_progress_hook = f;
        return *this;
    }

    ProgressBar& ProgressBar::call_progress_hook()
    {
        if (p_progress_hook != nullptr)
        {
            auto proxy = ProgressProxy(this);
            p_progress_hook(proxy);
        }
        return *this;
    }

    ProgressBar& ProgressBar::call_repr_hook()
    {
        if (p_repr_hook != nullptr)
            p_repr_hook(m_repr);
        return *this;
    }

    std::string ProgressBar::prefix() const
    {
        return m_repr.prefix.value();
    }

    int ProgressBar::width() const
    {
        return m_width;
    }

    ProgressBarRepr& ProgressBar::update_repr(bool compute_progress)
    {
        call_progress_hook();
        m_repr.elapsed.set_value(fmt::format("{:>5}", elapsed_time_to_str()));
        call_repr_hook();

        if (compute_progress)
            m_repr.compute_progress();

        return m_repr;
    }

    const ProgressBarRepr& ProgressBar::repr() const
    {
        return m_repr;
    }

    ProgressBarRepr& ProgressBar::repr()
    {
        return m_repr;
    }

    /**********************
     * DefaultProgressBar *
     **********************/

    DefaultProgressBar::DefaultProgressBar(const std::string& prefix, std::size_t total, int width)
        : ProgressBar(prefix, total, width)
    {
    }

    void DefaultProgressBar::print(std::ostream& ostream, std::size_t width, bool with_endl)
    {
        if (!width && m_width)
            width = m_width;

        print_formatted_bar_repr(ostream, m_repr, width, with_endl);
    }

    /*********************
     * HiddenProgressBar *
     *********************/

    HiddenProgressBar::HiddenProgressBar(const std::string& prefix,
                                         AggregatedBarManager* /*manager*/,
                                         std::size_t total,
                                         int width)
        : ProgressBar(prefix, total, width)
    {
    }

    void HiddenProgressBar::print(std::ostream& /*stream*/,
                                  std::size_t /*width*/,
                                  bool /*with_endl*/)
    {
    }

    /*******************
     * MultiBarManager *
     *******************/

    MultiBarManager::MultiBarManager()
    {
    }

    MultiBarManager::MultiBarManager(std::size_t width)
        : ProgressBarManager(width)
    {
    }

    ProgressProxy MultiBarManager::add_progress_bar(const std::string& name,
                                                    std::size_t expected_total)
    {
        std::string prefix = name;
        std::lock_guard<std::mutex> lock(m_mutex);

        m_progress_bars.push_back(std::make_unique<DefaultProgressBar>(prefix, expected_total));
        return ProgressProxy(m_progress_bars[m_progress_bars.size() - 1].get());
    }

    std::size_t MultiBarManager::print(std::ostream& ostream,
                                       std::size_t width,
                                       std::size_t max_lines,
                                       bool with_endl)
    {
        std::size_t active_count = 0, not_displayed = 0;
        std::size_t max_sub_bars = std::numeric_limits<std::size_t>::max();
        std::lock_guard<std::mutex> lock(m_mutex);

        if (!width && m_width)
            width = m_width;

        if (max_lines < std::numeric_limits<std::size_t>::max())
            max_sub_bars = max_lines;

        std::vector<ProgressBar*> displayed_bars = {};
        {
            std::vector<std::unique_lock<std::mutex>> pbar_locks = {};

            std::size_t max_bars_to_print = 0;
            for (auto& pbar : m_progress_bars)
            {
                if (!pbar->stopped() && !pbar->completed())
                    ++max_bars_to_print;

                pbar_locks.push_back(pbar->chrono_lock());
            }

            if (m_sort_bars)
                sort_bars(max_bars_to_print <= max_sub_bars);

            for (auto& b : m_progress_bars)
            {
                if (b->started() || b->paused())
                {
                    if (active_count < max_sub_bars)
                    {
                        if (!b->started())
                            b->repr().style = fmt::fg(fmt::terminal_color::bright_black);
                        else
                            b->repr().style = fmt::text_style();

                        displayed_bars.push_back(b.get());
                        ++active_count;
                    }
                    else
                        ++not_displayed;
                }
            }
        }

        if (!displayed_bars.empty())
        {
            compute_bars_progress(displayed_bars);

            if (max_sub_bars && active_count >= max_sub_bars)
            {
                ostream << fmt::format(" > {} more active", not_displayed) << "\n";
                ++active_count;
            }

            for (std::size_t i = 0; i < displayed_bars.size(); ++i)
                if ((i == displayed_bars.size() - 1) && !with_endl)
                    print_formatted_bar_repr(ostream, displayed_bars[i]->repr(), width, with_endl);
                else
                    print_formatted_bar_repr(ostream, displayed_bars[i]->repr(), width, true);
        }

        return active_count;
    }

    /************************
     * AggregatedBarManager *
     ************************/

    AggregatedBarManager::AggregatedBarManager()
    {
    }

    AggregatedBarManager::AggregatedBarManager(std::size_t width)
        : ProgressBarManager(width)
    {
    }

    ProgressProxy AggregatedBarManager::add_progress_bar(const std::string& prefix,
                                                         std::size_t expected_total)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_progress_bars.push_back(
            std::make_unique<DefaultProgressBar>(prefix, expected_total, 100));

        return ProgressProxy(m_progress_bars[m_progress_bars.size() - 1].get());
    }

    void AggregatedBarManager::clear_progress_bars()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_labels.clear();
        m_progress_bars.clear();
        m_aggregated_bars.clear();
    }

    ProgressBar* AggregatedBarManager::aggregated_bar(const std::string& label)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_aggregated_bars.count(label))
            return m_aggregated_bars[label].get();
        else
            return nullptr;
    }

    void AggregatedBarManager::add_label(const std::string& label,
                                         const ProgressProxy& progress_bar)
    {
        ProgressBarManager::add_label(label, progress_bar);

        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_aggregated_bars.count(label) == 0)
            m_aggregated_bars.insert({ label,
                                       std::make_unique<DefaultProgressBar>(
                                           label, std::numeric_limits<std::size_t>::max(), 100) });
    }

    void AggregatedBarManager::activate_sub_bars()
    {
        m_print_sub_bars = true;
    }

    void AggregatedBarManager::deactivate_sub_bars()
    {
        m_print_sub_bars = false;
    }

    void AggregatedBarManager::update_aggregates_progress()
    {
        for (auto& [label, bars] : m_labels)
        {
            std::size_t current = 0, total = 0, in_progress = 0, speed = 0, active_count = 0,
                        total_count = 0;
            bool any_spinner = false;
            bool any_started = false;
            std::vector<ProgressBar::time_point_t> start_times = {};
            ProgressBar* aggregate_bar_ptr = m_aggregated_bars[label].get();

            aggregate_bar_ptr->active_tasks().clear();
            aggregate_bar_ptr->all_tasks().clear();

            for (auto& bar : bars)
            {
                current += bar->current();
                total += bar->total();
                ++total_count;

                if (!bar->unset())
                    start_times.push_back(bar->start_time());
                if (bar->started())
                {
                    speed += bar->speed();
                    in_progress += (bar->total() - bar->current());
                    ++active_count;
                    aggregate_bar_ptr->add_active_task(bar->prefix());
                    any_started = true;
                }
                else
                    aggregate_bar_ptr->add_task(bar->prefix());

                if (bar->is_spinner())
                    any_spinner = true;
            }

            if (aggregate_bar_ptr->unset() && !start_times.empty())
                aggregate_bar_ptr->start(*std::min_element(start_times.begin(), start_times.end()));

            if (any_spinner)
                aggregate_bar_ptr->activate_spinner();
            else
                aggregate_bar_ptr->deactivate_spinner();

            if (any_started)
            {
                if (aggregate_bar_ptr->paused())
                    aggregate_bar_ptr->resume();
            }
            else
            {
                aggregate_bar_ptr->pause();
                aggregate_bar_ptr->deactivate_spinner();
            }

            if (any_started || (current != aggregate_bar_ptr->current())
                || (total != aggregate_bar_ptr->total()))
            {
                aggregate_bar_ptr->set_progress(current, total);
                aggregate_bar_ptr->set_in_progress(in_progress);
                aggregate_bar_ptr->set_speed(speed);
            }
        }
    }

    std::size_t AggregatedBarManager::print(std::ostream& ostream,
                                            std::size_t width,
                                            std::size_t max_lines,
                                            bool with_endl)
    {
        std::size_t active_count = 0, not_displayed = 0;
        std::size_t max_sub_bars = std::numeric_limits<std::size_t>::max();
        std::lock_guard<std::mutex> lock(m_mutex);

        if (!width && m_width)
            width = m_width;

        if (max_lines < std::numeric_limits<std::size_t>::max())
        {
            if (max_lines < m_labels.size())
                return 0;
            else if (max_lines == m_labels.size())
            {
                max_sub_bars = 0;
                with_endl = false;
            }
            else
            {
                max_sub_bars = max_lines - m_labels.size();
                if (with_endl)
                    --max_sub_bars;
            }
        }

        std::vector<ProgressBar*> displayed_bars = {};
        {
            std::size_t max_bars_to_print = 0;
            std::vector<std::unique_lock<std::mutex>> pbar_locks = {};

            for (auto& pbar : m_progress_bars)
            {
                if (!pbar->stopped() && !pbar->completed())
                    ++max_bars_to_print;

                pbar_locks.push_back(pbar->chrono_lock());
            }

            if (m_sort_bars)
                sort_bars(max_bars_to_print <= max_sub_bars);

            if (m_print_sub_bars)
            {
                for (auto& b : m_progress_bars)
                {
                    if (b->started() || b->paused())
                    {
                        if (active_count < max_sub_bars)
                        {
                            if (!b->started())
                                b->repr().style = fmt::fg(fmt::terminal_color::bright_black);
                            else
                                b->repr().style = fmt::text_style();

                            displayed_bars.push_back(b.get());
                            ++active_count;
                        }
                        else
                            ++not_displayed;
                    }
                }
            }

            update_aggregates_progress();
            for (auto& [label, b] : m_labels)
            {
                displayed_bars.push_back(m_aggregated_bars[label].get());
                ++active_count;
            }
        }

        if (!displayed_bars.empty())
        {
            compute_bars_progress(displayed_bars);

            if (max_sub_bars && active_count > max_sub_bars)
            {
                ostream << fmt::format(" > {} more active", not_displayed) << "\n";
                ++active_count;
            }

            for (std::size_t i = 0; i < displayed_bars.size(); ++i)
                if ((i == displayed_bars.size() - 1) && !with_endl)
                    print_formatted_bar_repr(ostream, displayed_bars[i]->repr(), width, with_endl);
                else
                    print_formatted_bar_repr(ostream, displayed_bars[i]->repr(), width, true);
        }

        return active_count;
    }

    void AggregatedBarManager::update_download_bar(std::size_t /*current_diff*/)
    {
    }

    void AggregatedBarManager::update_extract_bar()
    {
    }

    bool AggregatedBarManager::is_complete() const
    {
        return false;
    }

    std::string duration_str(std::chrono::nanoseconds ns)
    {
        return duration_stream(ns).str();
    }

    std::stringstream duration_stream(std::chrono::nanoseconds ns)
    {
        using std::chrono::duration;
        using std::chrono::duration_cast;
        using std::chrono::hours;
        using std::chrono::milliseconds;
        using std::chrono::minutes;
        using std::chrono::seconds;

        using days = duration<int, std::ratio<86400>>;

        std::stringstream sstream;
        auto d = duration_cast<days>(ns);
        ns -= d;
        auto h = duration_cast<hours>(ns);
        ns -= h;
        auto m = duration_cast<minutes>(ns);
        ns -= m;
        auto s = duration_cast<seconds>(ns);
        ns -= s;
        auto ms = duration_cast<milliseconds>(ns);
        int ms_rounded;
        if (ms.count() >= 950)
        {
            ms_rounded = 0;
            s += seconds(1);
        }
        else
            ms_rounded = std::round(static_cast<double>(ms.count()) / 100.);

        if (d.count() > 0)
            sstream << d.count() << "d:";
        if (h.count() > 0)
            sstream << h.count() << "h:";
        if (m.count() > 0)
            sstream << m.count() << "m:";

        sstream << s.count() << "." << ms_rounded << "s";
        return sstream;
    }

    std::ostream& write_duration(std::ostream& os, std::chrono::nanoseconds ns)
    {
        os << duration_stream(ns).str();
        return os;
    }
}  // namespace pbar
