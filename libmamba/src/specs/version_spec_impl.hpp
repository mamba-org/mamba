#ifndef MAMBA_SPECS_VERSION_SPEC_IMPL_HPP
#define MAMBA_SPECS_VERSION_SPEC_IMPL_HPP


#include "mamba/specs/version.hpp"
#include "mamba/specs/version_spec.hpp"

namespace mamba::specs
{
    /**
     * Formatting of ``VersionSpec`` version glob  relies on special handling in ``Version``
     * formatter.
     * Said behaviour should not be surprising, this file however serves to make it clear why and
     * how this functionality was added to ``Version``.
     */

    inline static const auto VERSION_GLOB_SEGMENT = VersionPart(
        { { 0, VersionSpec::glob_pattern_str } }
    );

    inline static constexpr std::string_view GLOB_PATTERN_STR = VersionSpec::glob_pattern_str;
}

#endif
