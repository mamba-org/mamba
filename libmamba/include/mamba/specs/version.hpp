// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_SPECS_VERSION_HPP
#define MAMBA_SPECS_VERSION_HPP

#include <string>
#include <string_view>
#include <utility>

namespace mamba::specs
{

    /**
     * A succession of a number and a  lowercase litteral.
     *
     * Comparison is done lexicographically, with the number first and the litteral second.
     * Certain litterals have special meaning:
     * "*" < "dev" < "_"< any other litteral < "" < "post"
     */
    class VersionPartAtom
    {
    public:

        VersionPartAtom() noexcept = default;
        VersionPartAtom(std::size_t numeral) noexcept;
        VersionPartAtom(std::size_t numeral, std::string_view litteral);
        // The use of a template is only meant to prevent ambiguous conversions
        template <typename Char>
        VersionPartAtom(std::size_t numeral, std::basic_string<Char>&& litteral);

        auto numeral() const noexcept -> std::size_t;
        auto litteral() const& noexcept -> const std::string&;
        auto litteral() && noexcept -> std::string;

        auto operator==(const VersionPartAtom& other) const -> bool;
        auto operator!=(const VersionPartAtom& other) const -> bool;
        auto operator<(const VersionPartAtom& other) const -> bool;
        auto operator<=(const VersionPartAtom& other) const -> bool;
        auto operator>(const VersionPartAtom& other) const -> bool;
        auto operator>=(const VersionPartAtom& other) const -> bool;

    private:

        // Stored in decreasing size order for performance
        std::string m_litteral = "";
        std::size_t m_numeral = 0;
    };

    extern template VersionPartAtom::VersionPartAtom(std::size_t, std::string&&);
}
#endif
