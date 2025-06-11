#include "solv-cpp/dependency.hpp"

namespace solv
{
    ObjDependencyViewConst::ObjDependencyViewConst(const ::Reldep& reldep) noexcept
        : m_reldep(&reldep)
    {
    }

    ObjDependencyViewConst::~ObjDependencyViewConst() noexcept
    {
        m_reldep = nullptr;
    }

    auto ObjDependencyViewConst::raw() const -> const ::Reldep*
    {
        return m_reldep;
    }

    auto ObjDependencyViewConst::name() const -> StringId
    {
        return m_reldep->name;
    }

    auto ObjDependencyViewConst::version_range() const -> StringId
    {
        return m_reldep->evr;
    }

    auto ObjDependencyViewConst::flags() const -> RelationFlag
    {
        return m_reldep->flags;
    }
}
