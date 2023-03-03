// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_CORE_UTIL_STRING_HPP
#define MAMBA_CORE_UTIL_STRING_HPP

#include <algorithm>
#include <cstring>
#include <iomanip>
#include <ostream>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include "mamba/core/util_compare.hpp"

namespace mamba
{
    /**
     * Return the string if the pointer is not null, otherwise a pointer to an empty string.
     */
    const char* raw_str_or_empty(const char* ptr);

    char to_lower(char c);
    wchar_t to_lower(wchar_t c);
    std::string to_lower(std::string_view str);
    std::wstring to_lower(std::wstring_view str);
    template <typename Char>
    std::basic_string<Char> to_lower(std::basic_string<Char>&& str);
    template <typename Char, typename Iter>
    void to_lower_tranform(std::basic_string_view<Char> str, Iter out);

    char to_upper(char c);
    wchar_t to_upper(wchar_t c);
    std::string to_upper(std::string_view str);
    std::wstring to_upper(std::wstring_view str);
    template <typename Char>
    std::basic_string<Char> to_upper(std::basic_string<Char>&& str);
    template <typename Char, typename Iter>
    void to_upper_tranform(std::basic_string_view<Char> str, Iter out);

    bool starts_with(std::string_view str, std::string_view prefix);

    bool ends_with(std::string_view str, std::string_view suffix);

    bool contains(std::string_view str, std::string_view sub_str);

    /**
     * Check if any of the strings starts with the prefix.
     */
    template <typename StrRange>
    bool any_starts_with(const StrRange& strs, std::string_view prefix);
    template <typename StrRange>
    bool any_starts_with(const StrRange& strs, std::wstring_view prefix);

    /**
     * Check if the string starts with any of the prefix.
     */
    template <typename StrRange>
    bool starts_with_any(std::string_view str, const StrRange& prefix);
    template <typename StrRange>
    bool starts_with_any(std::wstring_view str, const StrRange& prefix);

    std::string_view lstrip(std::string_view input, char c);
    std::wstring_view lstrip(std::wstring_view input, wchar_t c);
    std::string_view lstrip(std::string_view input, std::string_view chars);
    std::wstring_view lstrip(std::wstring_view input, std::wstring_view chars);
    std::string_view lstrip(std::string_view input);
    std::wstring_view lstrip(std::wstring_view input);

    std::string_view rstrip(std::string_view input, char c);
    std::wstring_view rstrip(std::wstring_view input, wchar_t c);
    std::string_view rstrip(std::string_view input, std::string_view chars);
    std::wstring_view rstrip(std::wstring_view input, std::wstring_view chars);
    std::string_view rstrip(std::string_view input);
    std::wstring_view rstrip(std::wstring_view input);

    std::string_view strip(std::string_view input, char c);
    std::wstring_view strip(std::wstring_view input, wchar_t c);
    std::string_view strip(std::string_view input, std::string_view chars);
    std::wstring_view strip(std::wstring_view input, std::wstring_view chars);
    std::string_view strip(std::string_view input);
    std::wstring_view strip(std::wstring_view input);

    std::vector<std::string>
    split(std::string_view input, std::string_view sep, std::size_t max_split = SIZE_MAX);
    std::vector<std::wstring>
    split(std::wstring_view input, std::wstring_view sep, std::size_t max_split = SIZE_MAX);

    std::vector<std::string>
    rsplit(std::string_view input, std::string_view sep, std::size_t max_split = SIZE_MAX);
    std::vector<std::wstring>
    rsplit(std::wstring_view input, std::wstring_view sep, std::size_t max_split = SIZE_MAX);

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
    UnaryFunction join_for_each(InputIt first, InputIt last, UnaryFunction func, const Value& sep);

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
     * and a trunction symbol @p etc is define by the join iteration of either all the elements
     * in the iterator pair if they are less than @p threshold, a limited number of elements, with
     * middle elements represented by @p etc.
     * defined by iterating through the ``n`` elements of the iterator pair, interleaving the
     * separator in between the elements (thus appearing ``n-1`` times).
     *
     * @param first The iterator pointing to the begining of the range of elements to join.
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
    UnaryFunction join_trunc_for_each(
        InputIt first,
        InputIt last,
        UnaryFunction func,
        const Value& sep,
        const Value& etc,
        std::size_t threshold = 5,
        std::pair<std::size_t, std::size_t> show = { 2, 1 }
    );

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

    inline const char* raw_str_or_empty(const char* ptr)
    {
        return ptr ? ptr : "";
    }

    /***************************************************
     *  Implementation of to_lower to_upper functions  *
     ***************************************************/

    template <typename Char, typename Iter>
    void to_lower_tranform(std::basic_string_view<Char> str, Iter out)
    {
        std::transform(str.cbegin(), str.cend(), out, [](auto c) { return to_lower(c); });
    }


    template <typename Char>
    std::basic_string<Char> to_lower(std::basic_string<Char>&& str)
    {
        to_lower_tranform<Char>(str, str.begin());
        return str;
    }

    template <typename Char, typename Iter>
    void to_upper_tranform(std::basic_string_view<Char> str, Iter out)
    {
        std::transform(str.cbegin(), str.cend(), out, [](auto c) { return to_upper(c); });
    }

    template <typename Char>
    std::basic_string<Char> to_upper(std::basic_string<Char>&& str)
    {
        to_upper_tranform<Char>(str, str.begin());
        return str;
    }

    /********************************************
     *  Implementation of start_with functions  *
     ********************************************/

    template <typename StrRange, typename Char>
    bool any_starts_with(const StrRange& strs, std::basic_string_view<Char> prefix)
    {
        return std::any_of(
            strs.cbegin(),
            strs.cend(),
            [&prefix](const auto& s) { return starts_with(s, prefix); }
        );
    }

    template <typename StrRange>
    bool any_starts_with(const StrRange& strs, std::string_view prefix)
    {
        return any_starts_with<StrRange, decltype(prefix)::value_type>(strs, prefix);
    }

    template <typename StrRange>
    bool any_starts_with(const StrRange& strs, std::wstring_view prefix)
    {
        return any_starts_with<StrRange, decltype(prefix)::value_type>(strs, prefix);
    }

    extern template bool any_starts_with(const std::vector<std::string>&, std::string_view);
    extern template bool any_starts_with(const std::vector<std::string_view>&, std::string_view);

    template <typename StrRange, typename Char>
    bool starts_with_any(std::basic_string_view<Char> str, const StrRange& prefix)
    {
        return std::any_of(
            prefix.cbegin(),
            prefix.cend(),
            [&str](const auto& p) { return starts_with(str, p); }
        );
    }

    template <typename StrRange>
    bool starts_with_any(std::string_view str, const StrRange& prefix)
    {
        return starts_with_any<StrRange, char>(str, prefix);
    }

    template <typename StrRange>
    bool starts_with_any(std::wstring_view str, const StrRange& prefix)
    {
        return starts_with_any<StrRange, wchar_t>(str, prefix);
    }

    extern template bool starts_with_any(std::string_view, const std::vector<std::string>&);
    extern template bool starts_with_any(std::string_view, const std::vector<std::string_view>&);

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

        std::size_t size(const char* s);
        std::size_t size(const wchar_t* s);
        std::size_t size(const char c);
        template <class T>
        std::size_t size(const T& s)
        {
            using std::size;
            return size(s);
        }
    }

    // TODO(C++20) Take an input range and return a range
    template <typename InputIt, typename UnaryFunction, typename Value>
    UnaryFunction join_for_each(InputIt first, InputIt last, UnaryFunction func, const Value& sep)
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
            auto inc_size = [&final_size](const auto& val) { final_size += detail::size(val); };
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
    UnaryFunction join_trunc_for_each(
        InputIt first,
        InputIt last,
        UnaryFunction func,
        const Value& sep,
        const Value& etc,
        std::size_t threshold,
        std::pair<std::size_t, std::size_t> show
    )
    {
        if (util::cmp_less_equal(last - first, threshold))
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
            join_for_each_func(first, first + show_head, sep);
            func(sep);
        }
        func(etc);
        if (show_tail)
        {
            func(sep);
            join_for_each_func(last - show_tail, last, sep);
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
            auto inc_size = [&final_size](const auto& val) { final_size += detail::size(val); };
            join_trunc_for_each(range.begin(), range.end(), inc_size, sep, etc, threshold, show);
            out.reserve(final_size);
        }

        auto out_joiner = [&](auto&& val) { joiner(out, std::forward<decltype(val)>(val)); };
        join_trunc_for_each(range.begin(), range.end(), out_joiner, sep, etc, threshold, show);
        return out;
    }

    template <typename... Args>
    std::string concat(const Args&... args)
    {
        std::string result;
        result.reserve((detail::size(args) + ...));
        ((result += args), ...);
        return result;
    }

    template <class B>
    std::string hex_string(const B& buffer, std::size_t size)
    {
        std::ostringstream oss;
        oss << std::hex;
        for (std::size_t i = 0; i < size; ++i)
        {
            oss << std::setw(2) << std::setfill('0') << static_cast<int>(buffer[i]);
        }
        return oss.str();
    }

    template <class B>
    std::string hex_string(const B& buffer)
    {
        return hex_string(buffer, buffer.size());
    }
}

#endif
