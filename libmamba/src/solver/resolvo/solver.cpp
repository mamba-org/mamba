// Copyright (c) 2024, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include "mamba/solver/resolvo/database.hpp"
#include "mamba/solver/resolvo/solver.hpp"
#include "mamba/util/variant_cmp.hpp"

namespace mamba::solver::resolvo
{
    namespace
    {
        /**
         * An arbitrary comparison function to get determinist output.
         */
        auto make_request_cmp()
        {
            return util::make_variant_cmp(
                /** index_cmp= */
                [](auto lhs, auto rhs) { return lhs < rhs; },
                /** alternative_cmp= */
                [](const auto& lhs, const auto& rhs)
                {
                    using Itm = std::decay_t<decltype(lhs)>;
                    if constexpr (!std::is_same_v<Itm, Request::UpdateAll>)
                    {
                        return lhs.spec.name().str() < rhs.spec.name().str();
                    }
                    return false;
                }
            );
        }

        auto request_to_requirements(const Request& request, Database& database)
            -> std::vector<::resolvo::VersionSetId>
        {
            std::vector<::resolvo::VersionSetId> requirements;
            requirements.reserve(request.jobs.size());

            for (const auto& job : request.jobs)
            {
                std::visit(
                    [&](const auto& j)
                    {
                        using T = std::decay_t<decltype(j)>;
                        if constexpr (std::is_same_v<T, Request::Install>)
                        {
                            requirements.push_back(database.alloc_version_set(j.spec.name().str()));
                        }
                    },
                    job
                );
            }
            return requirements;
        }

        auto request_to_constraints(const Request& request, Database& database)
            -> std::vector<::resolvo::VersionSetId>
        {
            std::vector<::resolvo::VersionSetId> constraints;
            constraints.reserve(request.jobs.size());

            for (const auto& job : request.jobs)
            {
                std::visit(
                    [&](const auto& j)
                    {
                        using T = std::decay_t<decltype(j)>;
                        if constexpr (std::is_same_v<T, Request::Remove>)
                        {
                            constraints.push_back(database.alloc_version_set(j.spec.name().str()));
                        }
                    },
                    job
                );
            }
            return constraints;
        }

        auto
        result_to_solution(const ::resolvo::Vector<::resolvo::SolvableId>& result, Database& database, const Request&)
            -> Solution
        {
            Solution solution;
            solution.actions.reserve(result.size());

            for (const auto& solvable_id : result)
            {
                const auto& solvable = database.solvable_pool[solvable_id];
                specs::PackageInfo pkg;
                pkg.name = solvable.name;
                pkg.version = solvable.version;
                pkg.build_string = solvable.build_string;
                pkg.build_number = solvable.build_number;
                pkg.channel = solvable.channel;
                pkg.md5 = solvable.md5;
                pkg.sha256 = solvable.sha256;
                pkg.track_features = solvable.track_features;
                pkg.dependencies = solvable.dependencies;
                pkg.constrains = solvable.constrains;
                pkg.timestamp = solvable.timestamp;
                pkg.license = solvable.license;
                pkg.size = solvable.size;

                solution.actions.emplace_back(Solution::Install{ std::move(pkg) });
            }

            return solution;
        }
    }

    auto Solver::solve_impl(Database& database, const Request& request) -> expected_t<Outcome>
    {
        auto requirements = request_to_requirements(request, database);
        auto constraints = request_to_constraints(request, database);
        ::resolvo::Vector<::resolvo::SolvableId> result;

        ::resolvo::Vector<::resolvo::VersionSetId> req_vec;
        for (const auto& req : requirements)
        {
            req_vec.push_back(req);
        }

        ::resolvo::Vector<::resolvo::VersionSetId> constr_vec;
        for (const auto& constr : constraints)
        {
            constr_vec.push_back(constr);
        }

        ::resolvo::String reason = ::resolvo::solve(database, req_vec, constr_vec, result);

        if (reason != "")
        {
            // Get the length from a string view of the reason
            std::string_view reason_str_view = reason;
            std::string reason_str(reason.data(), reason_str_view.size());
            return make_unexpected(mamba_error("resolvo", reason_str));
        }

        return Outcome{ result_to_solution(result, database, request) };
    }

    auto Solver::solve(Database& database, Request&& request) -> expected_t<Outcome>
    {
        if (request.flags.order_request)
        {
            std::sort(request.jobs.begin(), request.jobs.end(), make_request_cmp());
        }
        return solve_impl(database, request);
    }

    auto Solver::solve(Database& database, const Request& request) -> expected_t<Outcome>
    {
        if (request.flags.order_request)
        {
            auto sorted_request = request;
            std::sort(sorted_request.jobs.begin(), sorted_request.jobs.end(), make_request_cmp());
            return solve_impl(database, sorted_request);
        }
        return solve_impl(database, request);
    }
}
