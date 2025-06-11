// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_SOLVER_LIBSOLV_PARAMETERS_HPP
#define MAMBA_SOLVER_LIBSOLV_PARAMETERS_HPP

#include <string>

#include <nlohmann/json_fwd.hpp>

namespace mamba::solver::libsolv
{
    enum class RepodataParser
    {
        Mamba,
        Libsolv,
    };

    enum class MatchSpecParser
    {
        Mixed,
        Mamba,
        Libsolv,
    };

    enum class PipAsPythonDependency : bool
    {
        No = false,
        Yes = true,
    };

    enum class PackageTypes
    {
        CondaOnly,
        TarBz2Only,
        CondaAndTarBz2,
        CondaOrElseTarBz2,
    };

    enum class VerifyPackages : bool
    {
        No = false,
        Yes = true,
    };

    enum class LogLevel
    {
        Debug,
        Warning,
        Error,
        Fatal,
    };

    struct Priorities
    {
        using value_type = int;

        value_type priority = 0;
        value_type subpriority = 0;
    };

    [[nodiscard]] auto operator==(const Priorities& lhs, const Priorities& rhs) -> bool;
    [[nodiscard]] auto operator!=(const Priorities& lhs, const Priorities& rhs) -> bool;

    /**
     * Metadata serialized with a repository index.
     *
     * This is used to identify if the binary serialization is out of date with the expected
     * index.
     */
    struct RepodataOrigin
    {
        std::string url = {};
        std::string etag = {};
        std::string mod = {};
    };

    auto operator==(const RepodataOrigin& lhs, const RepodataOrigin& rhs) -> bool;
    auto operator!=(const RepodataOrigin& lhs, const RepodataOrigin& rhs) -> bool;

    void to_json(nlohmann::json& j, const RepodataOrigin& m);
    void from_json(const nlohmann::json& j, RepodataOrigin& p);
}
#endif
