
#ifndef MAMBA_CORE_UTIL_STRING_HPP
#define MAMBA_CORE_UTIL_STRING_HPP

#include <vector>
#include <string>
#include <string_view>
#include <ostream>
#include <cstring>
#include <sstream>
#include <algorithm>
#include <iomanip>

namespace mamba
{
    inline const char* check_char(const char* ptr)
    {
        return ptr ? ptr : "";
    }

    constexpr const char* WHITESPACES(" \r\n\t\f\v");
    constexpr const wchar_t* WHITESPACES_WSTR(L" \r\n\t\f\v");

    bool starts_with(const std::string_view& str, const std::string_view& prefix);
    bool ends_with(const std::string_view& str, const std::string_view& suffix);
    bool contains(const std::string_view& str, const std::string_view& sub_str);

    // TODO: add concepts here, or at least some contraints
    template <typename T, typename AssociativeContainer>
    auto contains(const AssociativeContainer& values, const T& value_to_find)
        -> decltype(values.find(value_to_find)
                    != values.end())  // this should make invalid usage SFINAE
    {
        return values.find(value_to_find) != values.end();
    }

    bool any_starts_with(const std::vector<std::string_view>& str, const std::string_view& prefix);

    bool starts_with_any(const std::string_view& str, const std::vector<std::string_view>& prefix);

    template <class CharType>
    std::basic_string_view<CharType> strip(const std::basic_string_view<CharType>& input,
                                           const std::basic_string_view<CharType>& chars)
    {
        size_t start = input.find_first_not_of(chars);
        if (start == std::basic_string<CharType>::npos)
        {
            return std::basic_string_view<CharType>();
        }
        size_t stop = input.find_last_not_of(chars) + 1;
        size_t length = stop - start;
        return length == 0 ? std::basic_string_view<CharType>() : input.substr(start, length);
    }

    std::string_view strip(const std::string_view& input);
    std::wstring_view strip(const std::wstring_view& input);
    std::string_view lstrip(const std::string_view& input);
    std::string_view rstrip(const std::string_view& input);

    std::string_view strip(const std::string_view& input, const std::string_view& chars);
    std::string_view lstrip(const std::string_view& input, const std::string_view& chars);
    std::string_view rstrip(const std::string_view& input, const std::string_view& chars);

    // FIXME: doesnt support const char* mixed with other string types as input.
    template <class CharType>
    std::vector<std::basic_string<CharType>> split(const std::basic_string_view<CharType> input,
                                                   const std::basic_string_view<CharType> sep,
                                                   std::size_t max_split = SIZE_MAX)
    {
        std::vector<std::basic_string<CharType>> result;
        std::size_t i = 0, j = 0, len = input.size(), n = sep.size();

        while (i + n <= len)
        {
            if (input[i] == sep[0] && input.substr(i, n) == sep)
            {
                if (max_split-- <= 0)
                    break;
                result.emplace_back(input.substr(j, i - j));
                i = j = i + n;
            }
            else
            {
                i++;
            }
        }
        result.emplace_back(input.substr(j, len - j));
        return result;
    }

    // unfortunately, c++ doesn't support templating function s. t. strip(std::string, std::string)
    // works, so this is a workaround that fixes it
    // FIXME: this overload should not be necessary, but the generic version doesn't
    // work with const char* and mixed with other types for some reason.
    inline std::vector<std::string> split(const std::string_view& input,
                                          const std::string_view& sep,
                                          std::size_t max_split = SIZE_MAX)
    {
        return split<char>(input, sep, max_split);
    }

    std::vector<std::string> rsplit(const std::string_view& input,
                                    const std::string_view& sep,
                                    std::size_t max_split = SIZE_MAX);

    namespace details
    {
        struct PlusEqual
        {
            template <typename T, typename U>
            auto operator()(T& left, const U& right)
            {
                left += right;
            }
        };
    }

    template <typename InputIt, typename Inserter, typename Value>
    void join_insert(InputIt first, InputIt last, Inserter inserter, Value const& sep)
    {
        if (first < last)
        {
            inserter(*(first++));
            for (; first < last; ++first)
            {
                inserter(sep);
                inserter(*first);
            }
        }
    }

    template <class Range, class Value, class Joiner = details::PlusEqual>
    auto join(Value const& sep, const Range& container, Joiner joiner = details::PlusEqual{}) ->
        typename Range::value_type
    {
        auto out = typename Range::value_type();
        auto joiner_inserter = [&](auto&& val) { joiner(out, std::forward<decltype(val)>(val)); };
        join_insert(container.begin(), container.end(), joiner_inserter, sep);
        return out;
    }

    template <class Range, class Joiner = details::PlusEqual>
    auto join_trunc(Range const& container,
                    std::string_view sep = ", ",
                    std::string_view etc = "...",
                    std::size_t threshold = 5,
                    std::pair<std::size_t, std::size_t> show = { 2, 1 },
                    Joiner joiner = details::PlusEqual{}) -> typename Range::value_type
    {
        if (container.size() <= threshold)
        {
            return join(sep, container);
        }
        auto const [show_head, show_tail] = show;
        auto out = typename Range::value_type();

        auto inserter = [&](auto&& val) { joiner(out, std::forward<decltype(val)>(val)); };
        join_insert(container.begin(), container.begin() + show_head, inserter, sep);
        inserter(sep);
        inserter(etc);
        inserter(sep);
        join_insert(container.end() - show_tail, container.end(), inserter, sep);
        return out;
    }


    void replace_all(std::string& data, const std::string& search, const std::string& replace);

    void replace_all(std::wstring& data, const std::wstring& search, const std::wstring& replace);

    template <typename T>
    auto without_duplicates(std::vector<T> values)
    {
        const auto end_it = std::unique(values.begin(), values.end());
        values.erase(end_it, values.end());
        return values;
    }

    // Note: this function only works for non-unicode!
    std::string to_upper(const std::string_view& input);
    std::string to_lower(const std::string_view& input);

    namespace concat_impl
    {
        inline std::size_t size(const char* s)
        {
            return strlen(s);
        }
        inline std::size_t size(const char /*c*/)
        {
            return 1;
        }
        template <class T>
        inline std::size_t size(T& s)
        {
            return s.size();
        }
    }  // namespace concat_impl

    template <typename... Args>
    inline std::string concat(const Args&... args)
    {
        std::string result;
        result.reserve((concat_impl::size(args) + ...));
        ((result += args), ...);
        return result;
    }

    template <class B>
    inline std::string hex_string(const B& buffer, std::size_t size)
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
    inline std::string hex_string(const B& buffer)
    {
        return hex_string(buffer, buffer.size());
    }
}

#endif
