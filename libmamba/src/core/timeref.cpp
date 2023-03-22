#include <mamba/core/timeref.hpp>
#include <mamba/core/util.hpp>

namespace validate
{

    TimeRef::TimeRef(const std::time_t& time)
        : m_time_ref(time)
    {
    }

    TimeRef::TimeRef()
        : TimeRef(mamba::utc_time_now())
    {
    }

    TimeRef::~TimeRef() = default;

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
