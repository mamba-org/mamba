// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_CORE_REPO_HPP
#define MAMBA_CORE_REPO_HPP

#include <string_view>
#include <tuple>


extern "C"
{
    using Repo = struct s_Repo;
}

namespace mamba
{
    class MPool;
    class MTransaction;

    /**
     * A wrapper class of libsolv Repo.
     * Represents a channel subdirectory and
     * is built using a ready-to-use index/metadata
     * file (see ``MSubdirData``).
     */
    class MRepo
    {
    public:

        enum class RepodataParser
        {
            Automatic,
            Mamba,
            Libsolv,
        };

        enum class LibsolvCache : bool
        {
            No = false,
            Yes = true,
        };

        enum class PipAsPythonDependency : bool
        {
            No = false,
            Yes = true,
        };

        using RepoId = int;

        MRepo(const MRepo&) = delete;
        MRepo(MRepo&&) = default;
        auto operator=(const MRepo&) -> MRepo& = delete;
        auto operator=(MRepo&&) -> MRepo& = default;

        void set_priority(int priority, int subpriority);

        [[nodiscard]] auto id() const -> RepoId;

        [[nodiscard]] auto repo() const -> Repo*;

        [[nodiscard]] auto name() const -> std::string_view;

        [[nodiscard]] auto package_count() const -> std::size_t;

        [[deprecated]] auto py_priority() const -> std::tuple<int, int>;

    private:

        ::Repo* m_repo = nullptr;  // This is a view managed by libsolv pool

        explicit MRepo(::Repo* repo);

        friend class MPool;
        friend class MTransaction;  // As long as MTransaction leaks libsolv impl
    };
}
#endif
