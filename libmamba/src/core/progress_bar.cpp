#include "mamba/core/progress_bar.hpp"
#include "progress_bar_impl.hpp"

namespace mamba
{
    /*****************
     * ProgressProxy *
     *****************/

    ProgressProxy::ProgressProxy(ProgressBar* ptr)
        : p_bar(ptr)
    {
    }

    bool ProgressProxy::defined() const
    {
        return p_bar != nullptr;
    }

    ProgressProxy::operator bool() const
    {
        return p_bar != nullptr;
    }

    ProgressProxy& ProgressProxy::set_bar(ProgressBar* ptr)
    {
        p_bar = ptr;
        return *this;
    }

    ProgressProxy& ProgressProxy::set_progress(std::size_t current, std::size_t total)
    {
        p_bar->set_progress(current, total);
        return *this;
    }

    ProgressProxy& ProgressProxy::update_progress(std::size_t current, std::size_t total)
    {
        p_bar->update_progress(current, total);
        return *this;
    }

    ProgressProxy& ProgressProxy::set_progress(double progress)
    {
        p_bar->set_progress(progress);
        return *this;
    }

    ProgressProxy& ProgressProxy::set_current(std::size_t current)
    {
        p_bar->set_progress(current);
        return *this;
    }

    ProgressProxy& ProgressProxy::set_in_progress(std::size_t in_progress)
    {
        p_bar->set_in_progress(in_progress);
        return *this;
    }

    ProgressProxy& ProgressProxy::update_current(std::size_t current)
    {
        p_bar->update_current(current);
        return *this;
    }

    ProgressProxy& ProgressProxy::set_total(std::size_t total)
    {
        p_bar->set_total(total);
        return *this;
    }

    ProgressProxy& ProgressProxy::set_full()
    {
        p_bar->set_full();
        return *this;
    }

    ProgressProxy& ProgressProxy::set_speed(std::size_t speed)
    {
        p_bar->set_speed(speed);
        return *this;
    }

    ProgressProxy& ProgressProxy::activate_spinner()
    {
        p_bar->activate_spinner();
        return *this;
    }

    ProgressProxy& ProgressProxy::deactivate_spinner()
    {
        p_bar->deactivate_spinner();
        return *this;
    }

    std::size_t ProgressProxy::current() const
    {
        return p_bar->current();
    }

    std::size_t ProgressProxy::in_progress() const
    {
        return p_bar->in_progress();
    }

    std::size_t ProgressProxy::total() const
    {
        return p_bar->total();
    }

    std::size_t ProgressProxy::speed() const
    {
        return p_bar->speed();
    }

    std::size_t ProgressProxy::avg_speed(const std::chrono::milliseconds& duration)
    {
        return p_bar->avg_speed(duration);
    }

    double ProgressProxy::progress() const
    {
        return p_bar->progress();
    }

    bool ProgressProxy::completed() const
    {
        return p_bar->completed();
    }

    ProgressProxy& ProgressProxy::start()
    {
        p_bar->start();
        return *this;
    }

    ProgressProxy& ProgressProxy::stop()
    {
        p_bar->stop();
        return *this;
    }

    ProgressProxy& ProgressProxy::pause()
    {
        p_bar->pause();
        return *this;
    }

    ProgressProxy& ProgressProxy::resume()
    {
        p_bar->resume();
        return *this;
    }

    bool ProgressProxy::started() const
    {
        return p_bar->started();
    }

    ProgressProxy& ProgressProxy::mark_as_completed(const std::chrono::milliseconds& delay)
    {
        p_bar->mark_as_completed(delay);
        return *this;
    }

    ProgressProxy& ProgressProxy::set_prefix(const std::string& str)
    {
        p_bar->set_prefix(str);
        return *this;
    }

    ProgressProxy& ProgressProxy::set_postfix(const std::string& text)
    {
        p_bar->set_postfix(text);
        return *this;
    }

    ProgressProxy& ProgressProxy::set_repr_hook(std::function<void(ProgressBarRepr&)> f)
    {
        p_bar->set_repr_hook(f);
        return *this;
    }

    ProgressProxy& ProgressProxy::set_progress_hook(std::function<void(ProgressProxy&)> f)
    {
        p_bar->set_progress_hook(f);
        return *this;
    }

    std::string ProgressProxy::elapsed_time_to_str() const
    {
        return p_bar->elapsed_time_to_str();
    }

    std::string ProgressProxy::prefix() const
    {
        return p_bar->prefix();
    }

    int ProgressProxy::width() const
    {
        return p_bar->width();
    }

    ProgressProxy& ProgressProxy::print(std::ostream& stream, std::size_t width, bool with_endl)
    {
        // std::cout << "Printing progress bar ... " << std::endl;
        p_bar->print(stream, width, with_endl);
        return *this;
    }

    ProgressBarRepr& ProgressProxy::update_repr(bool compute_progress)
    {
        return p_bar->update_repr(compute_progress);
    }

    const ProgressBarRepr& ProgressProxy::repr() const
    {
        return p_bar->repr();
    }

    ProgressBarRepr& ProgressProxy::repr()
    {
        return p_bar->repr();
    }
}
