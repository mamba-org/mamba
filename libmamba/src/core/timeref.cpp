#include <mamba/core/timeref.hpp>
#include <mamba/core/util.hpp>

namespace mamba::validation
{

    TimeRef::TimeRef(const std::time_t& time)
        : m_time_ref(time)
    {
    }

    TimeRef::TimeRef()
        : TimeRef(mamba::utc_time_now())
    {
    }

    void TimeRef::set(const std::time_t& time)
    {
        m_time_ref = time;
    }

    void TimeRef::set_now()
    {
        m_time_ref = mamba::utc_time_now();
    }

    std::string TimeRef::timestamp() const
    {
        return mamba::timestamp(m_time_ref);
    }

}
