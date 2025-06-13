// Copyright (c) 2025, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <catch2/catch_all.hpp>

#include <mutex>
#include <shared_mutex>

#include "mamba/util/synchronized_value.hpp"

namespace mamba::util
{
    static_assert(BasicLockable<std::mutex>);
    static_assert(BasicLockable<std::recursive_mutex>);
    static_assert(BasicLockable<std::shared_mutex>);

    static_assert(Lockable<std::mutex>);
    static_assert(Lockable<std::recursive_mutex>);
    static_assert(Lockable<std::shared_mutex>);


    static_assert(Mutex<std::mutex>);
    static_assert(Mutex<std::recursive_mutex>);
    static_assert(Mutex<std::shared_mutex>);


    static_assert(SharedMutex<std::shared_mutex>);
}

// TODO: parametrize with a various set of basic types?
// TODO: parametrize with the different supported mutexes

namespace {

    struct ValueType
    {
        int x = 0;

        auto increment() -> void { x++; }
        auto next_value() const -> ValueType { return {x + 1}; }

        auto operator<=>(const ValueType&) const noexcept = default;
    };

    TEST_CASE("synchronized_value")
    {

        SECTION("default constructible")
        {
            mamba::util::synchronized_value<ValueType> a;
        }

        static constexpr auto initial_value = ValueType{42};

        SECTION("basic value access and assignation")
        {
            mamba::util::synchronized_value<ValueType> sv{ initial_value };
            REQUIRE(sv.unsafe_get() == initial_value);
            REQUIRE(sv.value() == initial_value);
            REQUIRE(sv == initial_value);
            REQUIRE(sv->x == initial_value.x);

            const auto& const_sv = std::as_const(sv);
            REQUIRE(const_sv.unsafe_get() == initial_value);
            REQUIRE(const_sv.value() == initial_value);
            REQUIRE(const_sv == initial_value);
            REQUIRE(const_sv->x == initial_value.x);

            sv->increment();
            const auto expected_new_value = initial_value.next_value();
            REQUIRE(sv.unsafe_get() == expected_new_value);
            REQUIRE(sv.value() == expected_new_value);
            REQUIRE(sv == expected_new_value);
            REQUIRE(sv->x == expected_new_value.x);
            REQUIRE(const_sv.unsafe_get() == expected_new_value);
            REQUIRE(const_sv.value() == expected_new_value);
            REQUIRE(const_sv == expected_new_value);
            REQUIRE(const_sv->x == expected_new_value.x);

            sv = initial_value;
            REQUIRE(sv.unsafe_get() == initial_value);
            REQUIRE(sv.value() == initial_value);
            REQUIRE(sv == initial_value);
            REQUIRE(sv->x == initial_value.x);
            REQUIRE(const_sv.unsafe_get() == initial_value);
            REQUIRE(const_sv.value() == initial_value);
            REQUIRE(const_sv == initial_value);
            REQUIRE(const_sv->x == initial_value.x);
        }
    }
}