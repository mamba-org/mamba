// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_CORE_UTIL_STRING_HPP
#define MAMBA_CORE_UTIL_STRING_HPP

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace mamba::util
{
    /**
     * Return the string if the pointer is not null, otherwise a pointer to an empty string.
     */
    [[nodiscard]] auto raw_str_or_empty(const char* ptr) -> const char*;

    /**
     * Safe non utf-8 wrapping of <cctype> (see its doc).
     */
    [[nodiscard]] auto is_control(char c) -> bool;
    [[nodiscard]] auto is_control(wchar_t c) -> bool;
    [[nodiscard]] auto is_print(char c) -> bool;
    [[nodiscard]] auto is_print(wchar_t c) -> bool;
    [[nodiscard]] auto is_space(char c) -> bool;
    [[nodiscard]] auto is_space(wchar_t c) -> bool;
    [[nodiscard]] auto is_blank(char c) -> bool;
    [[nodiscard]] auto is_blank(wchar_t c) -> bool;
    [[nodiscard]] auto is_graphic(char c) -> bool;
    [[nodiscard]] auto is_graphic(wchar_t c) -> bool;
    [[nodiscard]] auto is_digit(char c) -> bool;
    [[nodiscard]] auto is_digit(wchar_t c) -> bool;
    [[nodiscard]] auto is_punct(char c) -> bool;
    [[nodiscard]] auto is_punct(wchar_t c) -> bool;
    [[nodiscard]] auto is_alpha(char c) -> bool;
    [[nodiscard]] auto is_alpha(wchar_t c) -> bool;
    [[nodiscard]] auto is_alphanum(char c) -> bool;
    [[nodiscard]] auto is_alphanum(wchar_t c) -> bool;
    [[nodiscard]] auto is_lower(char c) -> bool;
    [[nodiscard]] auto is_lower(wchar_t c) -> bool;
    [[nodiscard]] auto is_upper(char c) -> bool;
    [[nodiscard]] auto is_upper(wchar_t c) -> bool;
    [[nodiscard]] auto to_lower(char c) -> char;
    [[nodiscard]] auto to_lower(wchar_t c) -> wchar_t;

    [[nodiscard]] auto to_lower(std::string_view str) -> std::string;
    [[nodiscard]] auto to_lower(std::wstring_view str) -> std::wstring;
    // The use of a template here serves to exclude the overload for const Char*
    template <typename Char>
    [[nodiscard]] auto to_lower(std::basic_string<Char>&& str) -> std::basic_string<Char>;
    extern template std::string to_lower(std::string&& str);
    extern template std::wstring to_lower(std::wstring&& str);

    [[nodiscard]] auto to_upper(char c) -> char;
    [[nodiscard]] auto to_upper(wchar_t c) -> wchar_t;
    [[nodiscard]] auto to_upper(std::string_view str) -> std::string;
    [[nodiscard]] auto to_upper(std::wstring_view str) -> std::wstring;
    // The use of a template here serves to exclude the overload for const Char*
    template <typename Char>
    [[nodiscard]] auto to_upper(std::basic_string<Char>&& str) -> std::basic_string<Char>;
    extern template std::string to_upper(std::string&& str);
    extern template std::wstring to_upper(std::wstring&& str);

    [[nodiscard]] auto starts_with(std::string_view str, std::string_view prefix) -> bool;
    [[nodiscard]] auto starts_with(std::string_view str, std::string_view::value_type c) -> bool;

    [[nodiscard]] auto ends_with(std::string_view str, std::string_view suffix) -> bool;
    [[nodiscard]] auto ends_with(std::string_view str, std::string_view::value_type c) -> bool;

    [[nodiscard]] auto contains(std::string_view str, std::string_view sub_str) -> bool;
    [[nodiscard]] auto contains(std::string_view str, char c) -> bool;
    [[nodiscard]] auto contains(char c1, char c2) -> bool;

    /**
     * Check if any of the strings starts with the prefix.
     */
    template <typename StrRange>
    [[nodiscard]] auto any_starts_with(const StrRange& strs, std::string_view prefix) -> bool;
    template <typename StrRange>
    [[nodiscard]] auto any_starts_with(const StrRange& strs, std::wstring_view prefix) -> bool;

    /**
     * Check if the string starts with any of the prefix.
     */
    template <typename StrRange>
    [[nodiscard]] auto starts_with_any(std::string_view str, const StrRange& prefix) -> bool;
    template <typename StrRange>
    [[nodiscard]] auto starts_with_any(std::wstring_view str, const StrRange& prefix) -> bool;

    /**
     * Return a view to the input without the prefix if present.
     */
    [[nodiscard]] auto remove_prefix(std::string_view str, std::string_view prefix)
        -> std::string_view;
    [[nodiscard]] auto remove_prefix(std::string_view str, std::string_view::value_type c)
        -> std::string_view;

    /**
     * Return a view to prefix if present, and a view to the rest of the input.
     */
    [[nodiscard]] auto split_prefix(std::string_view str, std::string_view prefix)
        -> std::array<std::string_view, 2>;
    [[nodiscard]] auto split_prefix(std::string_view str, std::string_view::value_type c)
        -> std::array<std::string_view, 2>;

    /**
     * Return a view to the input without the suffix if present.
     */
    [[nodiscard]] auto remove_suffix(std::string_view str, std::string_view suffix)
        -> std::string_view;
    [[nodiscard]] auto remove_suffix(std::string_view str, std::string_view::value_type c)
        -> std::string_view;

    /**
     * Return a view to the head of the input, and a view to suffix if present.
     */
    [[nodiscard]] auto split_suffix(std::string_view str, std::string_view suffix)
        -> std::array<std::string_view, 2>;
    [[nodiscard]] auto split_suffix(std::string_view str, std::string_view::value_type c)
        -> std::array<std::string_view, 2>;

    [[nodiscard]] auto lstrip(std::string_view input, char c) -> std::string_view;
    [[nodiscard]] auto lstrip(std::wstring_view input, wchar_t c) -> std::wstring_view;
    [[nodiscard]] auto lstrip(std::string_view input, std::string_view chars) -> std::string_view;
    [[nodiscard]] auto lstrip(std::wstring_view input, std::wstring_view chars) -> std::wstring_view;
    [[nodiscard]] auto lstrip(std::string_view input) -> std::string_view;
    [[nodiscard]] auto lstrip(std::wstring_view input) -> std::wstring_view;

    [[nodiscard]] auto lstrip_parts(std::string_view input, char c)
        -> std::array<std::string_view, 2>;
    [[nodiscard]] auto lstrip_parts(std::wstring_view input, wchar_t c)
        -> std::array<std::wstring_view, 2>;
    [[nodiscard]] auto lstrip_parts(std::string_view input, std::string_view chars)
        -> std::array<std::string_view, 2>;
    [[nodiscard]] auto lstrip_parts(std::wstring_view input, std::wstring_view chars)
        -> std::array<std::wstring_view, 2>;

    template <typename UnaryFunc>
    [[nodiscard]] auto lstrip_if(std::string_view input, UnaryFunc should_strip) -> std::string_view;
    template <typename UnaryFunc>
    [[nodiscard]] auto lstrip_if(std::wstring_view input, UnaryFunc should_strip)
        -> std::wstring_view;

    template <typename UnaryFunc>
    [[nodiscard]] auto lstrip_if_parts(std::string_view input, UnaryFunc should_strip)
        -> std::array<std::string_view, 2>;
    template <typename UnaryFunc>
    [[nodiscard]] auto lstrip_if_parts(std::wstring_view input, UnaryFunc should_strip)
        -> std::array<std::wstring_view, 2>;

    [[nodiscard]] auto rstrip(std::string_view input, char c) -> std::string_view;
    [[nodiscard]] auto rstrip(std::wstring_view input, wchar_t c) -> std::wstring_view;
    [[nodiscard]] auto rstrip(std::string_view input, std::string_view chars) -> std::string_view;
    [[nodiscard]] auto rstrip(std::wstring_view input, std::wstring_view chars) -> std::wstring_view;
    [[nodiscard]] auto rstrip(std::string_view input) -> std::string_view;
    [[nodiscard]] auto rstrip(std::wstring_view input) -> std::wstring_view;

    [[nodiscard]] auto rstrip_parts(std::string_view input, char c)
        -> std::array<std::string_view, 2>;
    [[nodiscard]] auto rstrip_parts(std::wstring_view input, wchar_t c)
        -> std::array<std::wstring_view, 2>;
    [[nodiscard]] auto rstrip_parts(std::string_view input, std::string_view chars)
        -> std::array<std::string_view, 2>;
    [[nodiscard]] auto rstrip_parts(std::wstring_view input, std::wstring_view chars)
        -> std::array<std::wstring_view, 2>;

    template <typename UnaryFunc>
    [[nodiscard]] auto rstrip_if(std::string_view input, UnaryFunc should_strip) -> std::string_view;
    template <typename UnaryFunc>
    [[nodiscard]] auto rstrip_if(std::wstring_view input, UnaryFunc should_strip)
        -> std::wstring_view;

    template <typename UnaryFunc>
    [[nodiscard]] auto rstrip_if_parts(std::string_view input, UnaryFunc should_strip)
        -> std::array<std::string_view, 2>;
    template <typename UnaryFunc>
    [[nodiscard]] auto rstrip_if_parts(std::wstring_view input, UnaryFunc should_strip)
        -> std::array<std::wstring_view, 2>;

    [[nodiscard]] auto strip(std::string_view input, char c) -> std::string_view;
    [[nodiscard]] auto strip(std::wstring_view input, wchar_t c) -> std::wstring_view;
    [[nodiscard]] auto strip(std::string_view input, std::string_view chars) -> std::string_view;
    [[nodiscard]] auto strip(std::wstring_view input, std::wstring_view chars) -> std::wstring_view;
    [[nodiscard]] auto strip(std::string_view input) -> std::string_view;
    [[nodiscard]] auto strip(std::wstring_view input) -> std::wstring_view;

    /**
     * Dedicated implementation for inplace stripping of `std::string` to avoid copies
     */
    void inplace_strip(std::string& input);

    [[nodiscard]] auto strip_parts(std::string_view input, char c) -> std::array<std::string_view, 3>;
    [[nodiscard]] auto strip_parts(std::wstring_view input, wchar_t c)
        -> std::array<std::wstring_view, 3>;
    [[nodiscard]] auto strip_parts(std::string_view input, std::string_view chars)
        -> std::array<std::string_view, 3>;
    [[nodiscard]] auto strip_parts(std::wstring_view input, std::wstring_view chars)
        -> std::array<std::wstring_view, 3>;

    template <typename UnaryFunc>
    [[nodiscard]] auto strip_if(std::string_view input, UnaryFunc should_strip) -> std::string_view;
    template <typename UnaryFunc>
    [[nodiscard]] auto strip_if(std::wstring_view input, UnaryFunc should_strip)
        -> std::wstring_view;

    template <typename UnaryFunc>
    [[nodiscard]] auto strip_if_parts(std::string_view input, UnaryFunc should_strip)
        -> std::array<std::string_view, 3>;
    template <typename UnaryFunc>
    [[nodiscard]] auto strip_if_parts(std::wstring_view input, UnaryFunc should_strip)
        -> std::array<std::wstring_view, 3>;

    [[nodiscard]] auto split_once(std::string_view str, char sep)
        -> std::tuple<std::string_view, std::optional<std::string_view>>;
    [[nodiscard]] auto split_once(std::string_view str, std::string_view sep)
        -> std::tuple<std::string_view, std::optional<std::string_view>>;

    [[nodiscard]] auto rsplit_once(std::string_view str, char sep)
        -> std::tuple<std::optional<std::string_view>, std::string_view>;
    [[nodiscard]] auto rsplit_once(std::string_view str, std::string_view sep)
        -> std::tuple<std::optional<std::string_view>, std::string_view>;

    [[nodiscard]] auto split_once_on_any(std::string_view str, std::string_view many_seps)
        -> std::tuple<std::string_view, std::optional<std::string_view>>;
    template <std::size_t N>
    [[nodiscard]] auto split_once_on_any(std::string_view str, std::array<char, N> many_seps)
        -> std::tuple<std::string_view, std::optional<std::string_view>>;

    [[nodiscard]] auto rsplit_once_on_any(std::string_view str, std::string_view many_seps)
        -> std::tuple<std::optional<std::string_view>, std::string_view>;
    template <std::size_t N>
    [[nodiscard]] auto rsplit_once_on_any(std::string_view str, std::array<char, N> many_seps)
        -> std::tuple<std::optional<std::string_view>, std::string_view>;

    [[nodiscard]] auto
    split(std::string_view input, std::string_view sep, std::size_t max_split = SIZE_MAX)
        -> std::vector<std::string>;
    [[nodiscard]] auto split(std::string_view input, char sep, std::size_t max_split = SIZE_MAX)
        -> std::vector<std::string>;
    [[nodiscard]] auto
    split(std::wstring_view input, std::wstring_view sep, std::size_t max_split = SIZE_MAX)
        -> std::vector<std::wstring>;
    [[nodiscard]] auto split(std::wstring_view input, wchar_t sep, std::size_t max_split = SIZE_MAX)
        -> std::vector<std::wstring>;

    [[nodiscard]] auto
    rsplit(std::string_view input, std::string_view sep, std::size_t max_split = SIZE_MAX)
        -> std::vector<std::string>;
    [[nodiscard]] auto rsplit(std::string_view input, char sep, std::size_t max_split = SIZE_MAX)
        -> std::vector<std::string>;
    [[nodiscard]] auto
    rsplit(std::wstring_view input, std::wstring_view sep, std::size_t max_split = SIZE_MAX)
        -> std::vector<std::wstring>;
    [[nodiscard]] auto rsplit(std::wstring_view input, wchar_t sep, std::size_t max_split = SIZE_MAX)
        -> std::vector<std::wstring>;

    /**
     * Concatenate string while removing the suffix of the first that may be prefix of second.
     *
     * Comparison are done as if comparing elements in a split given by @p sep.
     * For instance "private/channel" and "channel/label/foo" with separator "/"
     * would return "private/channel/label/foo", but "private/chan" and "channel/label/foo"
     * would return the "private/chan/channel/label/foo".
     */
    [[nodiscard]] auto concat_dedup_splits(std::string_view str1, std::string_view str2, char sep)
        -> std::string;
    [[nodiscard]] auto
    concat_dedup_splits(std::string_view str1, std::string_view str2, std::string_view sep)
        -> std::string;

    void replace_all(std::string& data, std::string_view search, std::string_view replace);
    void replace_all(std::wstring& data, std::wstring_view search, std::wstring_view replace);

    namespace detail
    {
        struct PlusEqual
        {
            template <typename T, typename U>
            auto operator()(T& left, const U& right);
        };
    }

    /**
     * Execute the function @p func on each element of a join iteration.
     *
     * The join iteration of an iterator pair (@p first, @p last) with a separator @p sep is
     * defined by iterating through the ``n`` elements of the iterator pair, interleaving the
     * separator in between the elements (thus appearing ``n-1`` times).
     */
    template <typename InputIt, typename UnaryFunction, typename Value>
    auto join_for_each(InputIt first, InputIt last, UnaryFunction func, const Value& sep)
        -> UnaryFunction;

    /**
     * Concatenate the elements of the container @p container by interleaving a separator.
     *
     * Joining is done by successively joining (using the provided @p joiner) the aggregate with
     * element of the container and the separator, such that the separator only appears
     * in-between two elements of the range.
     *
     * @see join_for_each
     */
    template <class Range, class Value, class Joiner = detail::PlusEqual>
    auto join(const Value& sep, const Range& container, Joiner joiner = detail::PlusEqual{}) ->
        typename Range::value_type;

    /**
     * Execute the function @p func on each element of a tuncated join iteration.
     *
     * The join iteration of an iterator pair (@p first, @p last) with a separator @p sep
     * and a truncation symbol @p etc is define by the join iteration of either all the elements
     * in the iterator pair if they are less than @p threshold, a limited number of elements, with
     * middle elements represented by @p etc.
     * defined by iterating through the ``n`` elements of the iterator pair, interleaving the
     * separator in between the elements (thus appearing ``n-1`` times).
     *
     * @param first The iterator pointing to the beginning of the range of elements to join.
     * @param last The iterator pointing to past the end of the range of elements to join.
     * @param func The unary function to apply to all elements (separation and truncation included).
     * @param sep The separator used in between elements.
     * @param etc The value used to represent the truncation of the elements.
     * @param threshold Distance between the iterator pair beyond which truncation is preformed.
     * @param show Number of elements to keep at the begining/end when truncation is preformed.
     *
     * @see join_for_each
     */
    template <typename InputIt, typename UnaryFunction, typename Value>
    auto join_trunc_for_each(
        InputIt first,
        InputIt last,
        UnaryFunction func,
        const Value& sep,
        const Value& etc,
        std::size_t threshold = 5,
        std::pair<std::size_t, std::size_t> show = { 2, 1 }
    ) -> UnaryFunction;

    /**
     * Join elements of a range, with possible truncation.
     *
     * @param range Elements to join.
     * @param sep The separator used in between elements.
     * @param etc The value used to represent the truncation of the elements.
     * @param threshold Distance between the iterator pair beyond which truncation is preformed.
     * @param show Number of elements to keep at the begining/end when truncation is preformed.
     *
     * @see join_trunc_for_each
     * @see join
     */
    template <typename Range, typename Joiner = detail::PlusEqual>
    auto join_trunc(
        const Range& range,
        std::string_view sep = ", ",
        std::string_view etc = "...",
        std::size_t threshold = 5,
        std::pair<std::size_t, std::size_t> show = { 2, 1 },
        Joiner joiner = detail::PlusEqual{}
    ) -> typename Range::value_type;
    ;

    /************************
     *  Implementation misc *
     ************************/

    inline auto raw_str_or_empty(const char* ptr) -> const char*
    {
        return ptr ? ptr : "";
    }

    /********************************************
     *  Implementation of start_with functions  *
     ********************************************/

    template <typename StrRange, typename Char>
    auto any_starts_with(const StrRange& strs, std::basic_string_view<Char> prefix) -> bool
    {
        return std::any_of(
            strs.cbegin(),
            strs.cend(),
            [&prefix](const auto& s) { return starts_with(s, prefix); }
        );
    }

    template <typename StrRange>
    auto any_starts_with(const StrRange& strs, std::string_view prefix) -> bool
    {
        return any_starts_with<StrRange, decltype(prefix)::value_type>(strs, prefix);
    }

    template <typename StrRange>
    auto any_starts_with(const StrRange& strs, std::wstring_view prefix) -> bool
    {
        return any_starts_with<StrRange, decltype(prefix)::value_type>(strs, prefix);
    }

    extern template bool any_starts_with(const std::vector<std::string>&, std::string_view);
    extern template bool any_starts_with(const std::vector<std::string_view>&, std::string_view);

    template <typename StrRange, typename Char>
    auto starts_with_any(std::basic_string_view<Char> str, const StrRange& prefix) -> bool
    {
        return std::any_of(
            prefix.cbegin(),
            prefix.cend(),
            [&str](const auto& p) { return starts_with(str, p); }
        );
    }

    template <typename StrRange>
    auto starts_with_any(std::string_view str, const StrRange& prefix) -> bool
    {
        return starts_with_any<StrRange, char>(str, prefix);
    }

    template <typename StrRange>
    auto starts_with_any(std::wstring_view str, const StrRange& prefix) -> bool
    {
        return starts_with_any<StrRange, wchar_t>(str, prefix);
    }

    extern template bool starts_with_any(std::string_view, const std::vector<std::string>&);
    extern template bool starts_with_any(std::string_view, const std::vector<std::string_view>&);

    /***************************************
     *  Implementation of strip functions  *
     ***************************************/

    namespace detail
    {
        template <typename Char, typename UnaryFunc>
        auto lstrip_if_parts_impl(std::basic_string_view<Char> input, UnaryFunc should_strip)
            -> std::array<std::basic_string_view<Char>, 2>
        {
            const auto start_iter = std::find_if(
                input.cbegin(),
                input.cend(),
                [&should_strip](Char c) -> bool { return !should_strip(c); }
            );
            const auto start_idx = static_cast<std::size_t>(start_iter - input.cbegin());
            return { input.substr(0, start_idx), input.substr(start_idx) };
        }
    }

    template <typename UnaryFunc>
    auto lstrip_if(std::string_view input, UnaryFunc should_strip) -> std::string_view
    {
        return lstrip_if_parts(input, std::move(should_strip))[1];
    }

    template <typename UnaryFunc>
    auto lstrip_if(std::wstring_view input, UnaryFunc should_strip) -> std::wstring_view
    {
        return lstrip_if_parts(input, std::move(should_strip))[1];
    }

    template <typename UnaryFunc>
    auto lstrip_if_parts(std::string_view input, UnaryFunc should_strip)
        -> std::array<std::string_view, 2>
    {
        return detail::lstrip_if_parts_impl(input, std::move(should_strip));
    }

    template <typename UnaryFunc>
    auto lstrip_if_parts(std::wstring_view input, UnaryFunc should_strip)
        -> std::array<std::wstring_view, 2>
    {
        return detail::lstrip_if_parts_impl(input, std::move(should_strip));
    }

    namespace detail
    {
        template <typename Char, typename UnaryFunc>
        auto rstrip_if_parts_impl(std::basic_string_view<Char> input, UnaryFunc should_strip)
            -> std::array<std::basic_string_view<Char>, 2>
        {
            const auto rstart_iter = std::find_if(
                input.crbegin(),
                input.crend(),
                [&should_strip](Char c) -> bool { return !should_strip(c); }
            );
            const auto past_end_idx = static_cast<std::size_t>(input.crend() - rstart_iter);
            return { input.substr(0, past_end_idx), input.substr(past_end_idx) };
        }
    }

    template <typename UnaryFunc>
    auto rstrip_if(std::string_view input, UnaryFunc should_strip) -> std::string_view
    {
        return rstrip_if_parts(input, std::move(should_strip))[0];
    }

    template <typename UnaryFunc>
    auto rstrip_if(std::wstring_view input, UnaryFunc should_strip) -> std::wstring_view
    {
        return rstrip_if_parts(input, std::move(should_strip))[0];
    }

    template <typename UnaryFunc>
    auto rstrip_if_parts(std::string_view input, UnaryFunc should_strip)
        -> std::array<std::string_view, 2>
    {
        return detail::rstrip_if_parts_impl(input, std::move(should_strip));
    }

    template <typename UnaryFunc>
    auto rstrip_if_parts(std::wstring_view input, UnaryFunc should_strip)
        -> std::array<std::wstring_view, 2>
    {
        return detail::rstrip_if_parts_impl(input, std::move(should_strip));
    }

    namespace detail
    {
        template <typename Char, typename UnaryFunc>
        auto strip_if_parts_impl(std::basic_string_view<Char> input, UnaryFunc should_strip)
            -> std::array<std::basic_string_view<Char>, 3>
        {
            const auto [head, not_head] = lstrip_if_parts(input, should_strip);
            const auto [body, tail] = rstrip_if_parts(not_head, std::move(should_strip));
            return { head, body, tail };
        }
    }

    template <typename UnaryFunc>
    auto strip_if(std::string_view input, UnaryFunc should_strip) -> std::string_view
    {
        return strip_if_parts(input, std::move(should_strip))[1];
    }

    template <typename UnaryFunc>
    auto strip_if(std::wstring_view input, UnaryFunc should_strip) -> std::wstring_view
    {
        return strip_if_parts(input, std::move(should_strip))[1];
    }

    template <typename UnaryFunc>
    auto strip_if_parts(std::string_view input, UnaryFunc should_strip)
        -> std::array<std::string_view, 3>
    {
        return detail::strip_if_parts_impl(input, std::move(should_strip));
    }

    template <typename UnaryFunc>
    auto strip_if_parts(std::wstring_view input, UnaryFunc should_strip)
        -> std::array<std::wstring_view, 3>
    {
        return detail::strip_if_parts_impl(input, std::move(should_strip));
    }

    template <std::size_t N>
    auto split_once_on_any(std::string_view str, std::array<char, N> many_seps)
        -> std::tuple<std::string_view, std::optional<std::string_view>>
    {
        return split_once_on_any(str, std::string_view{ many_seps.data(), many_seps.size() });
    }

    template <std::size_t N>
    auto rsplit_once_on_any(std::string_view str, std::array<char, N> many_seps)
        -> std::tuple<std::optional<std::string_view>, std::string_view>
    {
        return rsplit_once_on_any(str, std::string_view{ many_seps.data(), many_seps.size() });
    }

    /**************************************
     *  Implementation of join functions  *
     **************************************/

    namespace detail
    {
        template <typename T, typename U>
        auto PlusEqual::operator()(T& left, const U& right)
        {
            left += right;
        }

        template <class T, class = void>
        struct has_reserve : std::false_type
        {
        };

        template <class T>
        struct has_reserve<T, std::void_t<decltype(std::declval<T>().reserve(std::size_t()))>>
            : std::true_type
        {
        };

        template <typename T>
        inline constexpr bool has_reserve_v = has_reserve<T>::value;

        auto length(const char* s) -> std::size_t;
        auto length(const wchar_t* s) -> std::size_t;
        auto length(const char c) -> std::size_t;
        auto length(const wchar_t c) -> std::size_t;

        template <class T>
        auto length(const T& s) -> std::size_t
        {
            return s.length();
        }
    }

    // TODO(C++20) Use ``std::ranges::join_view`` (or ``std::ranges::join``)
    template <typename InputIt, typename UnaryFunction, typename Value>
    auto join_for_each(InputIt first, InputIt last, UnaryFunction func, const Value& sep)
        -> UnaryFunction
    {
        if (first < last)
        {
            func(*(first++));
            for (; first < last; ++first)
            {
                func(sep);
                func(*first);
            }
        }
        return func;
    }

    template <class Range, class Value, class Joiner>
    auto join(const Value& sep, const Range& container, Joiner joiner) -> typename Range::value_type
    {
        using Result = typename Range::value_type;
        Result out{};
        if constexpr (detail::has_reserve_v<Result>)
        {
            std::size_t final_size = 0;
            auto inc_size = [&final_size](const auto& val) { final_size += detail::length(val); };
            join_for_each(container.begin(), container.end(), inc_size, sep);
            out.reserve(final_size);
        }
        auto out_joiner = [&](auto&& val) { joiner(out, std::forward<decltype(val)>(val)); };
        join_for_each(container.begin(), container.end(), out_joiner, sep);
        return out;
    }

    /********************************************
     *  Implementation of join_trunc functions  *
     ********************************************/

    // TODO(C++20) Take an input range and return a range
    template <typename InputIt, typename UnaryFunction, typename Value>
    auto join_trunc_for_each(
        InputIt first,
        InputIt last,
        UnaryFunction func,
        const Value& sep,
        const Value& etc,
        std::size_t threshold,
        std::pair<std::size_t, std::size_t> show
    ) -> UnaryFunction
    {
        if (std::cmp_less_equal(last - first, threshold))
        {
            return join_for_each(first, last, std::move(func), sep);
        }

        // Working around non-assignable function types, such as lambda with references.
        auto join_for_each_func = [&func](auto f, auto l, auto val)
        {
            if constexpr (std::is_assignable_v<UnaryFunction, UnaryFunction>)
            {
                func = join_for_each(f, l, std::move(func), val);
            }
            else
            {
                join_for_each(f, l, func, val);
            }
        };

        const auto [show_head, show_tail] = show;
        if (show_head > 0)
        {
            join_for_each_func(first, first + static_cast<std::ptrdiff_t>(show_head), sep);
            func(sep);
        }
        func(etc);
        if (show_tail)
        {
            func(sep);
            join_for_each_func(last - static_cast<std::ptrdiff_t>(show_tail), last, sep);
        }
        return func;
    }

    template <typename Range, typename Joiner>
    auto join_trunc(
        const Range& range,
        std::string_view sep,
        std::string_view etc,
        std::size_t threshold,
        std::pair<std::size_t, std::size_t> show,
        Joiner joiner
    ) -> typename Range::value_type
    {
        using Result = typename Range::value_type;
        Result out{};
        if constexpr (detail::has_reserve_v<Result>)
        {
            std::size_t final_size = 0;
            auto inc_size = [&final_size](const auto& val) { final_size += detail::length(val); };
            join_trunc_for_each(range.begin(), range.end(), inc_size, sep, etc, threshold, show);
            out.reserve(final_size);
        }

        auto out_joiner = [&](auto&& val) { joiner(out, std::forward<decltype(val)>(val)); };
        join_trunc_for_each(range.begin(), range.end(), out_joiner, sep, etc, threshold, show);
        return out;
    }

    template <typename... Args>
    auto concat(const Args&... args) -> std::string
    {
        std::string result;
        result.reserve((detail::length(args) + ...));
        ((result += args), ...);
        return result;
    }
}

#endif
