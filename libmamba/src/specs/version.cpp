// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <algorithm>
#include <iterator>
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

    /*******************************
     *  Implementation of Version  *
     *******************************/

    Version::Version(std::size_t epoch, CommonVersion&& version, CommonVersion&& local) noexcept
        : m_version{ std::move(version) }
        , m_local{ std::move(local) }
        , m_epoch{ epoch }
    {
    }

    auto Version::epoch() const noexcept -> std::size_t
    {
        return m_epoch;
    }

    auto Version::version() const noexcept -> const CommonVersion&
    {
        return m_version;
    }

    auto Version::local() const noexcept -> const CommonVersion&
    {
        return m_local;
    }

    namespace
    {
        /**
         * Compare two range where some trailing elements can be considered as empty.
         *
         * If ``0`` is considered "empty" then all the ranges ``[1, 2] and ``[1, 2, 0]``,
         * ``[1, 2, 0, 0]`` are considered equal, however ``[1, 2]`` and ``[1, 0, 2]`` are not.
         */
        template <typename Iter1, typename Iter2, typename BinaryPred, typename UnaryPred>
        auto equal_trailing(
            const Iter1 first1,
            Iter1 last1,
            Iter2 first2,
            Iter2 last2,
            BinaryPred equal,
            UnaryPred empty
        ) -> bool
        {
            const auto rfirst1 = std::find_if_not(
                std::reverse_iterator{ last1 },
                std::reverse_iterator{ first1 },
                empty
            );
            const auto rfirst2 = std::find_if_not(
                std::reverse_iterator{ last2 },
                std::reverse_iterator{ first2 },
                empty
            );
            // TODO(C++20) use std::lexicographical_compare_three_way to share implementation
            return std::equal(first1, rfirst1.base(), first2, rfirst2.base(), equal);
        }

        // Since VersionPart is weakly typed (an alias for std::vector), it not good
        // to expose this function as an operator==
        auto equal(const VersionPart& a, const VersionPart b) -> bool
        {
            return equal_trailing(
                a.cbegin(),
                a.cend(),
                b.cbegin(),
                b.cend(),
                std::equal_to<>{},
                [empty_a = VersionPartAtom{}](const auto& a) -> bool { return a == empty_a; }
            );
        }

        auto equal(const CommonVersion& a, const CommonVersion b) -> bool
        {
            return equal_trailing(
                a.cbegin(),
                a.cend(),
                b.cbegin(),
                b.cend(),
                [](const VersionPart& a, const VersionPart& b) -> bool { return equal(a, b); },
                [empty_a = VersionPartAtom{}](const auto& p) -> bool
                { return p.empty() || ((p.size() == 1) && (p.front() == empty_a)); }
            );
        }

        /**
         * Compare two range where some trailing elements can be considered as empty.
         *
         * If ``0`` is considered "empty" then all the ranges ``[1, 1] is less than ``[1, 2, 0]``
         * but more than ``[1, 1, -1]`` because ``-1 < 0``.
         */
        template <typename Iter1, typename Iter2, typename Compare, typename Equal, typename T>
        auto lexicographical_compare_trailing(
            Iter1 first1,
            Iter1 last1,
            Iter2 first2,
            Iter2 last2,
            Compare comp,
            Equal equal,
            T empty
        ) -> bool
        {
            auto [it1, it2] = std::mismatch(first1, last1, first2, last2, std::move(equal));
            // There is a mismatching element we use it for comparison
            if ((it1 != last1) && (it2 != last2))
            {
                return comp(*it1, *it2);
            }
            // They have the same leading elements but 1 has more elements
            // We do a lexicographic compare with an infite sequence of empties
            if ((it1 != last1))
            {
                for (; it1 != last1; ++it1)
                {
                    if (comp(*it1, empty))
                    {
                        return true;
                    }
                    if (comp(empty, *it1))
                    {
                        return false;
                    }
                }
                // Equal
                return false;
            }
            // it2 != last2
            // They have the same leading elements but 2 has more elements
            // We do a lexicographic compare with an infite sequence of empties
            for (; it2 != last2; ++it2)
            {
                if (comp(*it2, empty))
                {
                    return false;
                }
                if (comp(empty, *it2))
                {
                    return true;
                }
            }
            return false;
        }

        // Since VersionPart is weakly typed (an alias for std::vector), it not good
        // to expose this function as an operator==
        auto less(const VersionPart& a, const VersionPart b) -> bool
        {
            return lexicographical_compare_trailing(
                a.cbegin(),
                a.cend(),
                b.cbegin(),
                b.cend(),
                std::less<>{},
                std::equal_to<>{},
                VersionPartAtom{}
            );
        }

        auto less(const CommonVersion& a, const CommonVersion b) -> bool
        {
            return lexicographical_compare_trailing(
                a.cbegin(),
                a.cend(),
                b.cbegin(),
                b.cend(),
                [](const VersionPart& a, const VersionPart& b) -> bool { return less(a, b); },
                [](const VersionPart& a, const VersionPart& b) -> bool { return equal(a, b); },
                VersionPart{}
            );
        }
    }

    // TODO(C++20) use operator<=> to simplify code and improve operator<=
    auto Version::operator==(const Version& other) const -> bool
    {
        return (epoch() == other.epoch()) && equal(version(), other.version())
               && equal(local(), other.local());
    }

    auto Version::operator!=(const Version& other) const -> bool
    {
        return !(*this == other);
    }

    auto Version::operator<(const Version& other) const -> bool
    {
        if (epoch() < other.epoch())
        {
            return true;
        }
        if (other.epoch() < epoch())
        {
            return false;
        }

        if (less(version(), other.version()))
        {
            return true;
        }
        if (less(other.version(), version()))
        {
            return false;
        }

        if (less(local(), other.local()))
        {
            return true;
        }
        if (less(other.local(), local()))
        {
            return false;
        }

        return false;
    }

    auto Version::operator<=(const Version& other) const -> bool
    {
        return (*this < other) || (*this == other);
    }

    auto Version::operator>(const Version& other) const -> bool
    {
        return !(*this <= other);
    }

    auto Version::operator>=(const Version& other) const -> bool
    {
        return !(*this < other);
    }

}
