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

    TEST_CASE("synchronized_value-basics")
    {

        SECTION("default constructible")
        {
            mamba::util::synchronized_value<ValueType> a;
        }

        static constexpr auto initial_value = ValueType{42};
        mamba::util::synchronized_value<ValueType> sv{ initial_value };

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

    auto test_concurrent_increment(auto increment_task)
    {
        static constexpr auto arbitrary_number_of_executing_threads = 1024;

        mamba::util::synchronized_value<ValueType> current_value;
        static constexpr int expected_result = arbitrary_number_of_executing_threads;

        std::atomic<bool> run_tasks = false; // used to launch tasks about the same time, simpler than condition_variable
        std::vector<std::future<void>> tasks; // REVIEW: should I use TaskSynchronizer here instead?

        // Launch the tasks (maybe threads, depends on how async is implemented)
        for(int i = 0; i < expected_result; ++i)
        {
            tasks.push_back(std::async(std::launch::async
                ,[&, increment_task]{
                    // dont actually run until we get the green light
                    mambatests::wait_condition([&]{ return run_tasks == true; });
                    increment_task(current_value);
                }
            ));
        }

        run_tasks = true; // green light
        for(auto& task : tasks) task.wait(); // wait all to be finished

        REQUIRE(current_value->x == expected_result);
    }

    TEST_CASE("synchronized_value-thread_safe-direct_access")
    {
        test_concurrent_increment([](mamba::util::synchronized_value<ValueType>& sv){
            sv->x += 1;
        });
    }

    TEST_CASE("synchronized_value-thread_safe-synchronize")
    {
        test_concurrent_increment([](mamba::util::synchronized_value<ValueType>& sv){
            auto synched_sv = sv.synchronize();
            synched_sv->x += 1;
        });
    }


    TEST_CASE("synchronized_value-thread_safe-apply")
    {
        test_concurrent_increment([](mamba::util::synchronized_value<ValueType>& sv){
            sv.apply([](ValueType& value){
                value.x += 1;
            });
        });
    }
}