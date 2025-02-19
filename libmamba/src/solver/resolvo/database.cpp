#include <resolvo/resolvo_dependency_provider.h>
#include <resolvo/resolvo_pool.h>
#include <simdjson.h>

#include "mamba/core/util.hpp"  // for LockFile
#include "mamba/solver/resolvo/database.hpp"
#include "mamba/solver/solution.hpp"
#include "mamba/specs/channel.hpp"
#include "mamba/specs/package_info.hpp"

namespace mamba::solver::resolvo
{
    using logger_type = std::function<void(LogLevel, std::string_view)>;

    void PackageDatabase::set_logger(logger_type callback)
    {
    }

    auto PackageDatabase::add_repo_from_repodata_json(
        const mamba::fs::u8path& path,
        std::string_view url,
        const std::string& channel_id,
        PipAsPythonDependency add,
        PackageTypes package_types,
        VerifyPackages verify_packages,
        RepodataParser parser
    ) -> expected_t<RepoInfo>
    {
    }

    auto PackageDatabase::add_repo_from_native_serialization(
        const mamba::fs::u8path& path,
        const RepodataOrigin& expected,
        const std::string& channel_id,
        PipAsPythonDependency add
    ) -> expected_t<RepoInfo>
    {
    }

    template <typename Iter>
    auto PackageDatabase::add_repo_from_packages(
        Iter first_package,
        Iter last_package,
        std::string_view name,
        PipAsPythonDependency add
    ) -> RepoInfo
    {
    }

    template <typename Range>
    auto PackageDatabase::add_repo_from_packages(
        const Range& packages,
        std::string_view name,
        PipAsPythonDependency add
    ) -> RepoInfo
    {
    }

    auto PackageDatabase::native_serialize_repo(
        const RepoInfo& repo,
        const mamba::fs::u8path& path,
        const RepodataOrigin& metadata
    ) -> expected_t<RepoInfo>
    {
    }

    [[nodiscard]] auto PackageDatabase::installed_repo() const -> std::optional<RepoInfo>
    {
    }

    void PackageDatabase::set_installed_repo(RepoInfo repo)
    {
    }

    void PackageDatabase::set_repo_priority(RepoInfo repo, Priorities priorities)
    {
    }

    void PackageDatabase::remove_repo(RepoInfo repo)
    {
    }

    [[nodiscard]] auto PackageDatabase::repo_count() const -> std::size_t
    {
    }

    [[nodiscard]] auto PackageDatabase::package_count() const -> std::size_t
    {
    }

    template <typename Func>
    void PackageDatabase::for_each_package_in_repo(RepoInfo repo, Func&&) const
    {
    }

    template <typename Func>
    void PackageDatabase::for_each_package_matching(const specs::MatchSpec& ms, Func&&)
    {
    }

    template <typename Func>
    void PackageDatabase::for_each_package_depending_on(const specs::MatchSpec& ms, Func&&)
    {
    }

    // TODO: reuse it from `mamba/solver/libsolv/helpers.cpp`
    auto lsplit_track_features(std::string_view features)
    {
        constexpr auto is_sep = [](char c) -> bool { return (c == ',') || util::is_space(c); };
        auto [_, tail] = util::lstrip_if_parts(features, is_sep);
        return util::lstrip_if_parts(tail, [&](char c) { return !is_sep(c); });
    }

    // TODO: factorise with the implementation from `set_solvable` in
    // `mamba/solver/libsolv/helpers.cpp`
    bool parse_packageinfo_json(
        const std::string_view& filename,
        const simdjson::dom::element& pkg,
        const CondaURL& repo_url,
        const std::string& channel_id,
        PackageDatabase& database
    )
    {
        PackageInfo package_info;

        package_info.channel = channel_id;
        package_info.filename = filename;
        package_info.package_url = (repo_url / filename).str(specs::CondaURL::Credentials::Show);

        if (auto fn = pkg["fn"].get_string(); !fn.error())
        {
            package_info.name = fn.value_unsafe();
        }
        else
        {
            // Fallback from key entry
            package_info.name = filename;
        }

        if (auto name = pkg["name"].get_string(); !name.error())
        {
            package_info.name = name.value_unsafe();
        }
        else
        {
            return false;
        }

        if (auto version = pkg["version"].get_string(); !version.error())
        {
            package_info.version = version.value_unsafe();
        }
        else
        {
            return false;
        }

        if (auto build_string = pkg["build"].get_string(); !build_string.error())
        {
            package_info.build_string = build_string.value_unsafe();
        }
        else
        {
            return false;
        }

        if (auto build_number = pkg["build_number"].get_uint64(); !build_number.error())
        {
            package_info.build_number = build_number.value_unsafe();
        }
        else
        {
            return false;
        }

        if (auto subdir = pkg["subdir"].get_c_str(); !subdir.error())
        {
            package_info.platform = subdir.value_unsafe();
        }
        else
        {
        }

        if (auto size = pkg["size"].get_uint64(); !size.error())
        {
            package_info.size = size.value_unsafe();
        }

        if (auto md5 = pkg["md5"].get_c_str(); !md5.error())
        {
            package_info.md5 = md5.value_unsafe();
        }

        if (auto sha256 = pkg["sha256"].get_c_str(); !sha256.error())
        {
            package_info.sha256 = sha256.value_unsafe();
        }

        if (auto elem = pkg["noarch"]; !elem.error())
        {
            // TODO: is the following right?
            if (auto val = elem.get_bool(); !val.error() && val.value_unsafe())
            {
                package_info.noarch = NoArchType::No;
            }
            else if (auto noarch = elem.get_c_str(); !noarch.error())
            {
                package_info.noarch = NoArchType::No;
            }
        }

        if (auto license = pkg["license"].get_c_str(); !license.error())
        {
            package_info.license = license.value_unsafe();
        }

        // TODO conda timestamp are not Unix timestamp.
        // Libsolv normalize them this way, we need to do the same here otherwise the current
        // package may get arbitrary priority.
        if (auto timestamp = pkg["timestamp"].get_uint64(); !timestamp.error())
        {
            const auto time = timestamp.value_unsafe();
            // TODO: reuse it from `mamba/solver/libsolv/helpers.cpp`
            constexpr auto MAX_CONDA_TIMESTAMP = 253402300799ULL;
            package_info.timestamp = (time > MAX_CONDA_TIMESTAMP) ? (time / 1000) : time;
        }

        if (auto depends = pkg["depends"].get_array(); !depends.error())
        {
            for (auto elem : depends)
            {
                if (auto dep = elem.get_c_str(); !dep.error())
                {
                    package_info.dependencies.emplace_back(dep.value_unsafe());
                }
            }
        }

        if (auto constrains = pkg["constrains"].get_array(); !constrains.error())
        {
            for (auto elem : constrains)
            {
                if (auto cons = elem.get_c_str(); !cons.error())
                {
                    package_info.constrains.emplace_back(cons.value_unsafe());
                }
            }
        }

        if (auto obj = pkg["track_features"]; !obj.error())
        {
            if (auto track_features_arr = obj.get_array(); !track_features_arr.error())
            {
                for (auto elem : track_features_arr)
                {
                    if (auto feat = elem.get_string(); !feat.error())
                    {
                        package_info.track_features.emplace_back(feat.value_unsafe());
                    }
                }
            }
            else if (auto track_features_str = obj.get_string(); !track_features_str.error())
            {
                auto splits = lsplit_track_features(track_features_str.value_unsafe());
                while (!splits[0].empty())
                {
                    package_info.track_features.emplace_back(splits[0]);
                    splits = lsplit_track_features(splits[1]);
                }
            }
        }

        database.alloc_solvable(package_info);
        return true;
    }

    void parse_repodata_json(
        PackageDatabase& database,
        const fs::u8path& filename,
        const std::string& repo_url,
        const std::string& channel_id,
        bool verify_artifacts
    )
    {
        auto parser = simdjson::dom::parser();
        const auto lock = LockFile(filename);
        const auto repodata = parser.load(filename);

        // An override for missing package subdir is found at the top level
        auto default_subdir = std::string();
        if (auto subdir = repodata.at_pointer("/info/subdir").get_string(); !subdir.error())
        {
            default_subdir = std::string(subdir.value_unsafe());
        }

        // Get `base_url` in case 'repodata_version': 2
        // cf. https://github.com/conda-incubator/ceps/blob/main/cep-15.md
        auto base_url = repo_url;
        if (auto repodata_version = repodata["repodata_version"].get_int64();
            !repodata_version.error())
        {
            if (repodata_version.value_unsafe() == 2)
            {
                if (auto url = repodata.at_pointer("/info/base_url").get_string(); !url.error())
                {
                    base_url = std::string(url.value_unsafe());
                }
            }
        }

        const auto parsed_url = specs::CondaURL::parse(base_url)
                                    .or_else([](specs::ParseError&& err) { throw std::move(err); })
                                    .value();

        auto signatures = std::optional<simdjson::dom::object>(std::nullopt);
        if (auto maybe_sigs = repodata["signatures"].get_object();
            !maybe_sigs.error() && verify_artifacts)
        {
            signatures = std::move(maybe_sigs).value();
        }

        auto added = util::flat_set<std::string_view>();
        if (auto pkgs = repodata["packages.conda"].get_object(); !pkgs.error())
        {
            std::cout << "CondaOrElseTarBz2 packages.conda" << std::endl;

            for (auto [key, value] : pkgs.value())
            {
                parse_packageinfo_json(key, value, parsed_url, channel_id, database);
            }
        }
        if (auto pkgs = repodata["packages"].get_object(); !pkgs.error())
        {
            std::cout << "CondaOrElseTarBz2 packages" << std::endl;

            for (auto [key, value] : pkgs.value())
            {
                parse_packageinfo_json(key, value, parsed_url, channel_id, database);
            }
        }
    }

    // from `src/test_solver.cpp`
    auto find_actions_with_name(const Solution& solution, std::string_view name)
        -> std::vector<Solution::Action>;
    auto find_actions(const Solution& solution) -> std::vector<Solution::Action>;
    auto extract_package_to_install(const Solution& solution) -> std::vector<specs::PackageInfo>;

}
