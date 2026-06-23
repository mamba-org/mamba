// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef LIBMAMBATESTS_HPP
#define LIBMAMBATESTS_HPP

#include <array>
#include <functional>
#include <string_view>
#include <unordered_set>
#include <vector>

#include "mamba/core/context.hpp"
#include "mamba/core/output.hpp"
#include "mamba/fs/filesystem.hpp"
#include "mamba/util/environment.hpp"
#include "mamba/util/string.hpp"

namespace mambatests
{

#ifndef MAMBA_TEST_DATA_DIR
#error "MAMBA_TEST_DATA_DIR must be defined pointing to test data"
#endif
    inline static const mamba::fs::u8path test_data_dir = MAMBA_TEST_DATA_DIR;

#ifndef MAMBA_REPO_DIR
#error "MAMBA_REPO_DIR must be defined pointing to test data"
#endif
    inline static const mamba::fs::u8path repo_dir = MAMBA_REPO_DIR;

#ifndef MAMBA_TEST_LOCK_EXE
#error "MAMBA_TEST_LOCK_EXE must be defined pointing to testing_libmamba_lock"
#endif
    inline static const mamba::fs::u8path testing_libmamba_lock_exe = MAMBA_TEST_LOCK_EXE;

    struct Singletons
    {
        // mamba::MainExecutor main_executor; // FIXME: reactivate once the tests are not indirectly
        // using this anymore
        mamba::Context context{ { /* .enable_logging = */ true,
                                  /* .enable_signal_handling = */ true } };
        mamba::Console console{ context };
    };

    inline Singletons& singletons()  // FIXME: instead of this create the objects in main so that
                                     // their lifetime doesnt go beyond main() scope.
    {
        static Singletons singletons;
        return singletons;
    }

    // Provides the context object to use in all tests needing it.
    // Note that this context is setup to handle logging and signal handling.
    inline mamba::Context& context()
    {
        return singletons().context;
    }

    class EnvironmentCleaner
    {
    public:

        EnvironmentCleaner();

        template <typename... Func>
        EnvironmentCleaner(Func&&... cleaner);

        ~EnvironmentCleaner();

    private:

        mamba::util::environment_map m_env;
    };

    class CleanMambaEnv
    {
    public:

        inline static constexpr auto prefixes = std::array<std::string_view, 4>{
            "CONDA",
            "_CONDA",
            "MAMBA",
            "_MAMBA",
        };

        void operator()(const mamba::util::environment_map& env);
    };

    // RAII helper for C++ tests that temporarily override fields on the shared Context
    // singleton (see context()). Tests often need to point at a temp prefix, tweak channels,
    // or flip feature flags; without restoration, later tests inherit stale state.
    //
    // Save the value of each touched field on first use and restore it when the guard is
    // destroyed. Repeated calls to the same setter only change the live value — the original
    // snapshot is always what gets restored.
    //
    // Example:
    //   auto& ctx = mambatests::context();
    //   mambatests::ScopedContextChange context_change{ ctx };
    //   context_change.set_channels({ "conda-forge" }).set_offline(false);
    class ScopedContextChange
    {
    public:

        explicit ScopedContextChange(mamba::Context& ctx)
            : m_ctx(ctx)
        {
        }

        ~ScopedContextChange()
        {
            for (auto it = m_restorers.rbegin(); it != m_restorers.rend(); ++it)
            {
                (*it)();
            }
        }

        ScopedContextChange& set_target_prefix(const mamba::fs::u8path& prefix)
        {
            touch(
                &mamba::Context::prefix_params,
                [&](auto& params) { params.target_prefix = prefix; }
            );
            return *this;
        }

        ScopedContextChange& set_prefix_data_interoperability(bool value)
        {
            touch(&mamba::Context::prefix_data_interoperability, [&](auto& field) { field = value; });
            return *this;
        }

        ScopedContextChange& set_channels(std::vector<std::string> channels)
        {
            touch(&mamba::Context::channels, [&](auto& field) { field = std::move(channels); });
            return *this;
        }

        ScopedContextChange& set_use_sharded_repodata(bool value)
        {
            touch(&mamba::Context::use_sharded_repodata, [&](auto& field) { field = value; });
            return *this;
        }

        ScopedContextChange& set_offline(bool value)
        {
            touch(&mamba::Context::offline, [&](auto& field) { field = value; });
            return *this;
        }

        ScopedContextChange& set_platform(std::string platform)
        {
            touch(&mamba::Context::platform, [&](auto& field) { field = std::move(platform); });
            return *this;
        }

        template <typename T>
        ScopedContextChange& preserve(T mamba::Context::* member)
        {
            touch(member, [](auto&) {});
            return *this;
        }

        ScopedContextChange(const ScopedContextChange&) = delete;
        ScopedContextChange& operator=(const ScopedContextChange&) = delete;
        ScopedContextChange(ScopedContextChange&&) = delete;
        ScopedContextChange& operator=(ScopedContextChange&&) = delete;

    private:

        template <typename T, typename F>
        void touch(T mamba::Context::* member, F&& mutator)
        {
            auto& field = m_ctx.*member;
            if (m_saved_fields.insert(static_cast<const void*>(&field)).second)
            {
                const T initial = field;
                m_restorers.push_back([&field, initial] { field = initial; });
            }
            mutator(field);
        }

        mamba::Context& m_ctx;
        std::vector<std::function<void()>> m_restorers;
        std::unordered_set<const void*> m_saved_fields;
    };

    /******************************************
     *  Implementation of EnvironmentCleaner  *
     ******************************************/

    inline EnvironmentCleaner::EnvironmentCleaner()
        : m_env(mamba::util::get_env_map())
    {
    }

    template <typename... Func>
    EnvironmentCleaner::EnvironmentCleaner(Func&&... cleaner)
        : EnvironmentCleaner()
    {
        ((cleaner(const_cast<const mamba::util::environment_map&>(m_env))), ...);
    }

    inline EnvironmentCleaner::~EnvironmentCleaner()
    {
        mamba::util::set_env_map(m_env);
    }

    /*************************************
     *  Implementation of CleanMambaEnv  *
     *************************************/

    inline void CleanMambaEnv::operator()(const mamba::util::environment_map& env)
    {
        for (const auto& [key, val] : env)
        {
            if (mamba::util::starts_with_any(key, prefixes))
            {
                mamba::util::unset_env(key);
            }
        }
    }

}
#endif
