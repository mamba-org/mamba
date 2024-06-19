// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef LIBMAMBATESTS_HPP
#define LIBMAMBATESTS_HPP

#include <array>
#include <string_view>

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

#ifndef MAMBA_TEST_LOCK_EXE
#error "MAMBA_TEST_LOCK_EXE must be defined pointing to testing_libmamba_lock"
#endif
    inline static const mamba::fs::u8path testing_libmamba_lock_exe = MAMBA_TEST_LOCK_EXE;

    struct Singletons
    {
        // mamba::MainExecutor main_executor; // FIXME: reactivate once the tests are not indirectly
        // using this anymore
        mamba::Context context{ {
                /* .enable_logging = */ true,
                /* .enable_signal_handling = */ true
        } };
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
