// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <tuple>

#include "mamba/core/error_handling.hpp"
#include "mamba/core/util_cast.hpp"
#include "mamba/core/util_string.hpp"
#include "mamba/specs/version.hpp"

namespace mamba::specs
{
    /***************************************
     *  Implementation of VersionPartAtom  *
     ***************************************/

    VersionPartAtom::VersionPartAtom(std::size_t numeral) noexcept
        : m_numeral{ numeral }
    {
    }

    VersionPartAtom::VersionPartAtom(std::size_t numeral, std::string_view litteral)
        : m_litteral{ to_lower(litteral) }
        , m_numeral{ numeral }
    {
    }

    template <typename Char>
    VersionPartAtom::VersionPartAtom(std::size_t numeral, std::basic_string<Char>&& litteral)
        : m_litteral{ to_lower(std::move(litteral)) }
        , m_numeral{ numeral }
    {
    }

    template VersionPartAtom::VersionPartAtom(std::size_t, std::string&&);

    auto VersionPartAtom::numeral() const noexcept -> std::size_t
    {
        return m_numeral;
    }

    auto VersionPartAtom::litteral() const& noexcept -> const std::string&
    {
        return m_litteral;
    }

    auto VersionPartAtom::litteral() && noexcept -> std::string
    {
        return std::move(m_litteral);
    }

    // TODO(C++20) use operator<=>
    auto VersionPartAtom::operator==(const VersionPartAtom& other) const -> bool
    {
        auto attrs = [](const VersionPartAtom& a) -> std::tuple<std::size_t, const std::string&> {
            return { a.numeral(), a.litteral() };
        };
        return attrs(*this) == attrs(other);
    }

    auto VersionPartAtom::operator!=(const VersionPartAtom& other) const -> bool
    {
        return !(*this == other);
    }

    auto VersionPartAtom::operator<(const VersionPartAtom& other) const -> bool
    {
        // Numeral has priority in comparison
        if (numeral() < other.numeral())
        {
            return true;
        }
        if (numeral() > other.numeral())
        {
            return false;
        }

        // Certain litterals have sepcial meaning we map then to a priority
        // 0 meaning regular string
        auto lit_priority = [](const auto& l) -> int
        {
            if (l == "*")
            {
                return -3;
            }
            if (l == "dev")
            {
                return -2;
            }
            if (l == "_")
            {
                return -1;
            }
            if (l == "")
            {
                return 1;
            }
            if (l == "post")
            {
                return 2;
            }
            return 0;
        };
        const auto this_lit_val = lit_priority(litteral());
        const auto other_lit_val = lit_priority(other.litteral());
        // If two regular string, we need to use string comparison
        if ((this_lit_val == 0) && (other_lit_val == 0))
        {
            return litteral() < other.litteral();
        }
        return this_lit_val < other_lit_val;
    }

    auto VersionPartAtom::operator<=(const VersionPartAtom& other) const -> bool
    {
        return (*this < other) || (*this == other);
    }

    auto VersionPartAtom::operator>(const VersionPartAtom& other) const -> bool
    {
        return !(*this <= other);
    }

    auto VersionPartAtom::operator>=(const VersionPartAtom& other) const -> bool
    {
        return !(*this < other);
    }

}
