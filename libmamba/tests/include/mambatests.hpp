// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef LIBMAMBATESTS_HPP
#define LIBMAMBATESTS_HPP

#include <array>
#include <functional>
#include <ranges>
#include <string_view>
#include <unordered_set>
#include <vector>

#include "mamba/core/context.hpp"
#include "mamba/core/invoke.hpp"
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
    // singleton (see context()). A future improvement would be to give each test its own
    // Context instance instead of mutating the singleton (see singletons() FIXME). Until then,
    // tests often need to point at a temp prefix, tweak channels, or flip feature flags;
    // without restoration, later tests inherit stale state.
    //
    // Save the value of each touched field on first use and restore it when the guard is
    // destroyed. Repeated calls to the same setter only change the live value — the original
    // snapshot is always what gets restored.
    //
    // Use set_* when the test assigns a known value. Use preserve() when the field will be
    // changed later by code under test, by direct assignment (ctx.field = …), or by APIs such
    // as Configuration::load() — preserve() only snapshots the current value for restoration.
    //
    // Example:
    //   auto& ctx = mambatests::context();
    //   mambatests::ScopedContextChange context_change{ ctx };
    //   context_change.set_channels({ "conda-forge" }).set_offline(false);
    //
    //   context_change.preserve(ctx.use_sharded_repodata);
    //   ctx.use_sharded_repodata = false;  // restored to the pre-preserve value at scope end
    class ScopedContextChange
    {
    public:

        explicit ScopedContextChange(mamba::Context& ctx)
            : m_ctx(ctx)
        {
        }

        ~ScopedContextChange()
        {
            for (auto& restorer : std::ranges::reverse_view(m_restorers))
            {
                [[maybe_unused]] auto result = mamba::safe_invoke(restorer);
            }
        }

        ScopedContextChange& set_target_prefix(const mamba::fs::u8path& prefix)
        {
            touch(m_ctx.prefix_params, [&](auto& params) { params.target_prefix = prefix; });
            return *this;
        }

        ScopedContextChange& set_root_prefix(const mamba::fs::u8path& prefix)
        {
            touch(m_ctx.prefix_params, [&](auto& params) { params.root_prefix = prefix; });
            return *this;
        }

        ScopedContextChange& set_envs_dirs(std::vector<mamba::fs::u8path> dirs)
        {
            touch(m_ctx.envs_dirs, [&](auto& field) { field = std::move(dirs); });
            return *this;
        }

        ScopedContextChange& set_pkgs_dirs(std::vector<mamba::fs::u8path> dirs)
        {
            touch(m_ctx.pkgs_dirs, [&](auto& field) { field = std::move(dirs); });
            return *this;
        }

        ScopedContextChange& set_prefix_data_interoperability(bool value)
        {
            touch(m_ctx.prefix_data_interoperability, [&](auto& field) { field = value; });
            return *this;
        }

        ScopedContextChange& set_channels(std::vector<std::string> channels)
        {
            touch(m_ctx.channels, [&](auto& field) { field = std::move(channels); });
            return *this;
        }

        ScopedContextChange& set_use_sharded_repodata(bool value)
        {
            touch(m_ctx.use_sharded_repodata, [&](auto& field) { field = value; });
            return *this;
        }

        ScopedContextChange& set_offline(bool value)
        {
            touch(m_ctx.offline, [&](auto& field) { field = value; });
            return *this;
        }

        ScopedContextChange& set_platform(std::string platform)
        {
            touch(m_ctx.platform, [&](auto& field) { field = std::move(platform); });
            return *this;
        }

        // Snapshot member for restoration without assigning a new value.
        template <typename T>
        ScopedContextChange& preserve(T& member)
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
        void touch(T& member, F&& mutator)
        {
            if (m_saved_fields.insert(static_cast<const void*>(&member)).second)
            {
                m_restorers.push_back([&member, initial = member]() mutable
                                      { member = std::move(initial); });
            }
            std::invoke(std::forward<F>(mutator), member);
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
