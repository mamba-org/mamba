// Copyright (c) 2026, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#pragma once

#include <chrono>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>

#if defined(MAMBA_USE_HOWARD_HINNANT_DATE)
#include <date/date.h>
#include <date/tz.h>
#endif

namespace mamba::detail
{
    /**
     * Parse ``value`` with ``std::chrono::parse`` or ``date::from_stream`` using ``fmt``.
     *
     * When ``MAMBA_USE_HOWARD_HINNANT_DATE`` is set (libc++ lacks P0355;
     * https://github.com/llvm/llvm-project/issues/166051), ``date::from_stream`` is used.
     *
     * Returns ``std::nullopt`` when parsing fails or trailing characters remain.
     */
    template <typename T>
    [[nodiscard]] auto parse_chrono(std::string_view value, const char* fmt) -> std::optional<T>
    {
        std::istringstream stream{ std::string(value) };
        T out{};
#if defined(MAMBA_USE_HOWARD_HINNANT_DATE)
        date::from_stream(stream, fmt, out);
#else
        stream >> std::chrono::parse(fmt, out);
#endif
        if (stream.fail() || stream.peek() != std::istringstream::traits_type::eof())
        {
            return std::nullopt;
        }
        return out;
    }
}  // namespace mamba::detail
