// Copyright (c) 2025, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <catch2/catch_all.hpp>

#include <mutex>
#include <shared_mutex>
#include <future>
#include <thread>
#include <atomic>
#include <concepts>
#include <memory>
#include <ranges>

#include "mamba/util/synchronized_value.hpp"

#include "mambatests_utils.hpp"

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

    static_assert(std::move_constructible<scoped_locked_ptr<std::unique_ptr<int>, std::mutex          , true>>);
    static_assert(std::move_constructible<scoped_locked_ptr<std::unique_ptr<int>, std::recursive_mutex, true>>);
    static_assert(std::move_constructible<scoped_locked_ptr<std::unique_ptr<int>, std::shared_mutex   , true>>);
    static_assert(std::move_constructible<scoped_locked_ptr<std::unique_ptr<int>, std::mutex          , false>>);
    static_assert(std::move_constructible<scoped_locked_ptr<std::unique_ptr<int>, std::recursive_mutex, false>>);
    static_assert(std::move_constructible<scoped_locked_ptr<std::unique_ptr<int>, std::shared_mutex   , false>>);
}

namespace {

    struct ValueType
    {
        int x = 0;

        auto increment() -> void { x++; }
        auto next_value() const -> ValueType { return {x + 1}; }

        auto operator<=>(const ValueType&) const noexcept = default;
    };

    using supported_mutex_types = std::tuple< std::mutex, std::shared_mutex, std::recursive_mutex >;

    TEMPLATE_LIST_TEST_CASE("synchronized_value basics", "[template][thread-safe]", supported_mutex_types)
    {
        using synchronized_value = mamba::util::synchronized_value<ValueType, TestType>;

        SECTION("default constructible")
        {
            synchronized_value a;
        }

        static constexpr auto initial_value = ValueType{42};
        synchronized_value sv{ initial_value };

        SECTION("value access and assignation")
        {
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

        SECTION("value access using synchronize")
        {
            sv = initial_value;
            {
                auto sync_sv = std::as_const(sv).synchronize();
                REQUIRE(*sync_sv == initial_value);
                REQUIRE(sync_sv->x == initial_value.x);
            }
            REQUIRE(sv.unsafe_get() == initial_value);
            REQUIRE(sv.value() == initial_value);
            REQUIRE(sv == initial_value);
            REQUIRE(sv->x == initial_value.x);

            static constexpr auto expected_value = ValueType{12345};
            {
                auto sync_sv = sv.synchronize();
                sync_sv->x = expected_value.x;
            }
            REQUIRE(sv.unsafe_get() == expected_value);
            REQUIRE(sv.value() == expected_value);
            REQUIRE(sv == expected_value);
            REQUIRE(sv->x == expected_value.x);

            {
                auto sync_sv = sv.synchronize();
                *sync_sv = initial_value;
            }
            REQUIRE(sv.unsafe_get() == initial_value);
            REQUIRE(sv.value() == initial_value);
            REQUIRE(sv == initial_value);
            REQUIRE(sv->x == initial_value.x);

        }

        SECTION("value access using apply")
        {
            sv = initial_value;
            {
                auto result = std::as_const(sv).apply([](const ValueType& value){
                    return value.x;
                });
                REQUIRE(result == initial_value.x);
            }
            REQUIRE(sv.unsafe_get() == initial_value);
            REQUIRE(sv.value() == initial_value);
            REQUIRE(sv == initial_value);
            REQUIRE(sv->x == initial_value.x);

            static constexpr auto expected_value = ValueType{98765};
            sv.apply([](ValueType& value){
                value = expected_value;
            });
            REQUIRE(sv.unsafe_get() == expected_value);
            REQUIRE(sv.value() == expected_value);
            REQUIRE(sv == expected_value);
            REQUIRE(sv->x == expected_value.x);

            sv.apply([](ValueType& value, auto new_value){
                value = new_value;
            }, initial_value);
            REQUIRE(sv.unsafe_get() == initial_value);
            REQUIRE(sv.value() == initial_value);
            REQUIRE(sv == initial_value);
            REQUIRE(sv->x == initial_value.x);

        }
    }

    TEMPLATE_LIST_TEST_CASE("synchronized_value apply example", "[template][thread-safe]", supported_mutex_types)
    {
        using synchronized_value = mamba::util::synchronized_value< std::vector<int>, TestType >;

        const std::vector initial_values{ 9, 8, 7, 6, 5, 4, 3, 2, 1, 0 };
        const std::vector sorted_values{ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };

        synchronized_value values{ initial_values };
        values.apply(std::ranges::sort);
        REQUIRE(values == sorted_values);
        values.apply(std::ranges::sort, std::ranges::greater{});
        REQUIRE(values == initial_values);
    }

    template< mamba::util::Mutex M >
    auto test_concurrent_increment(std::invocable<mamba::util::synchronized_value<ValueType, M>&> auto increment_task)
    {
        static constexpr auto arbitrary_number_of_executing_threads = 512;

        mamba::util::synchronized_value<ValueType, M> current_value;
        static constexpr int expected_result = arbitrary_number_of_executing_threads;

        std::atomic<bool> run_tasks = false; // used to launch tasks about the same time, simpler than condition_variable
        std::vector<std::future<void>> tasks; // REVIEW: should I use TaskSynchronizer here instead?

        // Launch the reading and writing tasks (maybe threads, depends on how async is implemented)
        for(int i = 0; i < expected_result * 2; ++i)
        {
            if(i % 2)
            {
                // add writing task
                tasks.push_back(std::async(std::launch::async,
                    [&, increment_task]{
                        // dont actually run until we get the green light
                        mambatests::wait_condition([&]{ return run_tasks == true; });
                        increment_task(current_value);
                    }
                ));
            }
            else
            {
                // add reading task
                tasks.push_back(std::async(std::launch::async,
                    [&]{
                        // dont actually run until we get the green light
                        mambatests::wait_condition([&]{ return run_tasks == true; });
                        const auto& readonly_value = std::as_const(current_value);
                        static constexpr auto arbitrary_read_count = 100;
                        long long sum = 0;
                        for(int c = 0; c < arbitrary_read_count; ++c )
                        {
                            sum += readonly_value->x; // TODO: also try to mix reading and writing using different kinds of access
                            std::this_thread::yield(); // for timing randomness and limit over-exhaustion
                        }
                        REQUIRE(sum != 0); // It is possible but extremely unlikely that all reading tasks will read before any writing tasks.
                    }
                ));
            }

        }


        run_tasks = true; // green light
        for(auto& task : tasks) task.wait(); // wait all to be finished

        REQUIRE(current_value->x == expected_result);
    }

    TEMPLATE_LIST_TEST_CASE("synchronized_value thread-safe direct_access", "[template][thread-safe]", supported_mutex_types)
    {
        using synchronized_value = mamba::util::synchronized_value<ValueType, TestType>;
        test_concurrent_increment<TestType>([](synchronized_value& sv){
            sv->x += 1;
        });
    }

    TEMPLATE_LIST_TEST_CASE("synchronized_value thread-safe synchronize", "[template][thread-safe]", supported_mutex_types)
    {
        using synchronized_value = mamba::util::synchronized_value<ValueType, TestType>;
        test_concurrent_increment<TestType>([](synchronized_value& sv){
            auto synched_sv = sv.synchronize();
            synched_sv->x += 1;
        });
    }


    TEMPLATE_LIST_TEST_CASE("synchronized_value thread-safe apply", "[template][thread-safe]", supported_mutex_types)
    {
        using synchronized_value = mamba::util::synchronized_value<ValueType, TestType>;
        test_concurrent_increment<TestType>([](synchronized_value& sv){
            sv.apply([](ValueType& value){
                value.x += 1;
            });
        });
    }
}