// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_UTIL_RANDOM_HPP
#define MAMBA_UTIL_RANDOM_HPP

#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <random>
#include <string>
#include <string_view>

namespace mamba::util
{
    using default_random_generator = std::mt19937;

    template <typename Generator = default_random_generator>
    [[nodiscard]] auto random_generator() -> Generator;

    template <typename Generator = default_random_generator>
    auto local_random_generator() -> Generator&;

    template <typename T = int, typename Generator = default_random_generator>
    auto random_int(T min, T max, Generator& generator = local_random_generator()) -> T;

    template <typename Generator = default_random_generator>
    auto generate_random_alphanumeric_string(  //
        std::size_t len,
        Generator& generator = local_random_generator()
    ) -> std::string;

    /********************
     *  Implementation  *
     ********************/

    template <typename Generator>
    auto random_generator() -> Generator
    {
        using std::begin;
        using std::end;
        constexpr auto seed_bits = sizeof(typename Generator::result_type) * Generator::state_size;
        constexpr auto seed_len = seed_bits / std::numeric_limits<std::seed_seq::result_type>::digits;
        auto seed = std::array<std::seed_seq::result_type, seed_len>{};
        auto dev = std::random_device{};
        std::generate_n(begin(seed), seed_len, std::ref(dev));
        auto seed_seq = std::seed_seq(begin(seed), end(seed));
        return Generator{ seed_seq };
    }

    extern template auto random_generator<default_random_generator>() -> default_random_generator;

    template <typename Generator>
    auto local_random_generator() -> Generator&
    {
        thread_local auto rng = random_generator<Generator>();
        return rng;
    }

    extern template auto local_random_generator<default_random_generator>()
        -> default_random_generator&;

    template <typename T, typename Generator>
    auto random_int(T min, T max, Generator& generator) -> T
    {
        return std::uniform_int_distribution<T>{ min, max }(generator);
    }

    template <typename Generator>
    auto generate_random_alphanumeric_string(std::size_t len, Generator& generator) -> std::string
    {
        static constexpr auto chars = std::string_view{
            "0123456789"
            "abcdefghijklmnopqrstuvwxyz",
        };
        auto dist = std::uniform_int_distribution{ {}, chars.size() - 1 };
        auto result = std::string(len, '\0');
        std::generate_n(begin(result), len, [&]() { return chars[dist(generator)]; });
        return result;
    }

    extern template auto generate_random_alphanumeric_string<default_random_generator>(
        std::size_t len,
        default_random_generator& generator
    ) -> std::string;
}
#endif
