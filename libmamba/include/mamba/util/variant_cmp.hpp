// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_UTIL_VARIANT_CMP_HPP
#define MAMBA_UTIL_VARIANT_CMP_HPP

#include <type_traits>
#include <utility>
#include <variant>

namespace mamba::util
{
    template <typename IndexCmp, typename AlternativeCmp>
    [[nodiscard]] auto make_variant_cmp(IndexCmp&& index_cmp, AlternativeCmp&& alternative_cmp);

    /********************
     *  Implementation  *
     ********************/

    template <typename IndexCmp, typename AlternativeCmp>
    auto make_variant_cmp(IndexCmp&& index_cmp, AlternativeCmp&& alternative_cmp)
    {
        return [int_cmp = std::forward<IndexCmp>(index_cmp),
                alt_cmp = std::forward<AlternativeCmp>(alternative_cmp)  //
        ](const auto& lhs, const auto& rhs) -> bool
        {
            // When alternatives are different, compare the index.
            if (lhs.index() != rhs.index())
            {
                return int_cmp(lhs.index(), rhs.index());
            }
            // When alternatives are the same, compare the underlying type.
            return std::visit(
                [&](const auto& l) -> bool
                {
                    using Alt = std::decay_t<decltype(l)>;
                    const auto& r = std::get<Alt>(rhs);
                    return alt_cmp(l, r);
                },
                lhs
            );
        };
    }
}
#endif
