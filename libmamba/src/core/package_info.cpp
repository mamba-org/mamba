// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include "mamba/core/package_info.hpp"

#include <algorithm>
#include <functional>
#include <iterator>
#include <map>
#include <tuple>

#include "mamba/core/channel.hpp"
#include "mamba/core/util_string.hpp"

#include "solv-cpp/queue.hpp"

namespace mamba
{
    namespace
    {
        template <class T>
        std::string get_package_info_field(const PackageInfo&, T PackageInfo::*field);

        template <>
        std::string
        get_package_info_field<std::string>(const PackageInfo& pkg, std::string PackageInfo::*field)
        {
            return pkg.*field;
        }

        template <>
        std::string get_package_info_field<size_t>(const PackageInfo& pkg, size_t PackageInfo::*field)
        {
            return std::to_string(pkg.*field);
        }

        template <class T>
        PackageInfo::field_getter build_field_getter(T PackageInfo::*field)
        {
            return std::bind(get_package_info_field<T>, std::placeholders::_1, field);
        }

        using field_getter_map = std::map<std::string, PackageInfo::field_getter>;

        field_getter_map build_field_getter_map()
        {
            field_getter_map res;
            res["name"] = build_field_getter(&PackageInfo::name);
            res["version"] = build_field_getter(&PackageInfo::version);
            res["build_string"] = build_field_getter(&PackageInfo::build_string);
            res["build_number"] = build_field_getter(&PackageInfo::build_number);
            res["channel"] = build_field_getter(&PackageInfo::channel);
            res["url"] = build_field_getter(&PackageInfo::url);
            res["subdir"] = build_field_getter(&PackageInfo::subdir);
            res["fn"] = build_field_getter(&PackageInfo::fn);
            res["license"] = build_field_getter(&PackageInfo::license);
            res["size"] = build_field_getter(&PackageInfo::size);
            res["timestamp"] = build_field_getter(&PackageInfo::timestamp);
            return res;
        }

        field_getter_map& get_field_getter_map()
        {
            static field_getter_map m = build_field_getter_map();
            return m;
        }
    }  // namespace

    PackageInfo::field_getter PackageInfo::get_field_getter(const std::string& name)
    {
        auto it = get_field_getter_map().find(name);
        if (it == get_field_getter_map().end())
        {
            throw std::runtime_error("field_getter function not found");
        }
        return it->second;
    }

    PackageInfo::compare_fun PackageInfo::less(const std::string& member)
    {
        auto getter = get_field_getter(member);
        return [getter](const PackageInfo& lhs, const PackageInfo& rhs)
        { return getter(lhs) < getter(rhs); };
    }

    PackageInfo::compare_fun PackageInfo::equal(const std::string& member)
    {
        auto getter = get_field_getter(member);
        return [getter](const PackageInfo& lhs, const PackageInfo& rhs)
        { return getter(lhs) == getter(rhs); };
    }

    PackageInfo::PackageInfo(Solvable* s)
    {
        // Note: this function (especially the checksum part) is NOT YET threadsafe!
        Pool* pool = s->repo->pool;
        Id check_type;

        name = pool_id2str(pool, s->name);
        version = pool_id2str(pool, s->evr);
        const char* str = solvable_lookup_str(s, SOLVABLE_BUILDFLAVOR);
        if (str)
        {
            build_string = str;
        }
        str = solvable_lookup_str(s, SOLVABLE_BUILDVERSION);
        if (str)
        {
            build_number = std::stoull(str);
        }

        static Id real_repo_key = pool_str2id(pool, "solvable:real_repo_url", 1);
        if (solvable_lookup_str(s, real_repo_key))
        {
            url = solvable_lookup_str(s, real_repo_key);
            channel = make_channel(url).canonical_name();
        }
        else
        {
            if (!s->repo || strcmp(s->repo->name, "__explicit_specs__") == 0)
            {
                url = solvable_lookup_location(s, 0);
                channel = make_channel(url).canonical_name();
            }
            else
            {
                channel = s->repo->name;  // note this can and should be <unknown> when e.g.
                                          // installing from a tarball
                url = channel + "/" + raw_str_or_empty(solvable_lookup_str(s, SOLVABLE_MEDIAFILE));
            }
        }

        subdir = raw_str_or_empty(solvable_lookup_str(s, SOLVABLE_MEDIADIR));
        fn = raw_str_or_empty(solvable_lookup_str(s, SOLVABLE_MEDIAFILE));
        str = raw_str_or_empty(solvable_lookup_str(s, SOLVABLE_LICENSE));
        if (str)
        {
            license = str;
        }
        size = solvable_lookup_num(s, SOLVABLE_DOWNLOADSIZE, 0);
        timestamp = solvable_lookup_num(s, SOLVABLE_BUILDTIME, 0);
        str = solvable_lookup_checksum(s, SOLVABLE_PKGID, &check_type);
        if (str)
        {
            md5 = str;
        }
        str = solvable_lookup_checksum(s, SOLVABLE_CHECKSUM, &check_type);
        if (str)
        {
            sha256 = str;
        }
        signatures = raw_str_or_empty(solvable_lookup_str(s, SIGNATURE_DATA));
        if (signatures.empty())
        {
            signatures = "{}";
        }

        solv::ObjQueue q = {};

        if (!solvable_lookup_deparray(s, SOLVABLE_REQUIRES, q.raw(), -1))
        {
            defaulted_keys.insert("depends");
        }
        depends.reserve(q.size());
        std::transform(
            q.begin(),
            q.end(),
            std::back_inserter(depends),
            [&pool](Id id) { return pool_dep2str(pool, id); }
        );

        q.clear();
        if (!solvable_lookup_deparray(s, SOLVABLE_CONSTRAINS, q.raw(), -1))
        {
            defaulted_keys.insert("constrains");
        }
        constrains.reserve(q.size());
        std::transform(
            q.begin(),
            q.end(),
            std::back_inserter(constrains),
            [&pool](Id id) { return pool_dep2str(pool, id); }
        );

        q.clear();
        solvable_lookup_idarray(s, SOLVABLE_TRACK_FEATURES, q.raw());
        for (auto iter = q.begin(), end = q.end(); iter < end; ++iter)
        {
            track_features += pool_id2str(pool, *iter);
            if (iter < end - 1)
            {
                track_features += ",";
            }
        }

        Id extra_keys_id = pool_str2id(pool, "solvable:extra_keys", 0);
        Id extra_values_id = pool_str2id(pool, "solvable:extra_values", 0);

        if (extra_keys_id && extra_values_id)
        {
            // Get extra signed keys
            q.clear();
            solvable_lookup_idarray(s, extra_keys_id, q.raw());
            std::vector<std::string> extra_keys = {};
            extra_keys.reserve(q.size());
            std::transform(
                q.begin(),
                q.end(),
                std::back_inserter(extra_keys),
                [&pool](Id id) { return pool_dep2str(pool, id); }
            );

            // Get extra signed values
            q.clear();
            solvable_lookup_idarray(s, extra_values_id, q.raw());
            std::vector<std::string> extra_values = {};
            extra_values.reserve(q.size());
            std::transform(
                q.begin(),
                q.end(),
                std::back_inserter(extra_values),
                [&pool](Id id) { return pool_dep2str(pool, id); }
            );

            // Build a JSON string for extra signed metadata
            if (!extra_keys.empty() && (extra_keys.size() == extra_values.size()))
            {
                std::vector<std::string> extra;
                for (std::size_t i = 0; i < extra_keys.size(); ++i)
                {
                    extra.push_back("\"" + extra_keys[i] + "\":" + extra_values[i]);
                }
                extra_metadata = "{" + join(",", extra) + "}";
            }
        }
        else
        {
            extra_metadata = "{}";
        }
    }

    PackageInfo::PackageInfo(nlohmann::json&& j)
    {
        using namespace std::string_literals;  // NOLINT(build/namespaces)
        assign_or(j, "name", name, ""s);
        assign_or(j, "version", version, ""s);
        assign_or(j, "channel", channel, ""s);
        assign_or(j, "url", url, ""s);
        assign_or(j, "subdir", subdir, ""s);
        assign_or(j, "fn", fn, ""s);
        assign_or(j, "size", size, size_t(0));
        assign_or(j, "timestamp", timestamp, size_t(0));
        std::string bs;
        assign_or(j, "build", bs, "<UNKNOWN>"s);
        if (bs == "<UNKNOWN>")
        {
            assign_or(j, "build_string", bs, ""s);
        }
        build_string = bs;
        assign_or(j, "build_number", build_number, size_t(0));
        assign_or(j, "license", license, ""s);
        assign_or(j, "md5", md5, ""s);
        assign_or(j, "sha256", sha256, ""s);
        assign_or(j, "track_features", track_features, ""s);

        // add the noarch type if we know it (only known for installed packages)
        if (j.contains("noarch"))
        {
            if (j["noarch"].type() == nlohmann::json::value_t::boolean)
            {
                noarch = "generic_v1";
            }
            else
            {
                assign_or(j, "noarch", noarch, ""s);
            }
        }

        if (j.contains("depends"))
        {
            depends = j["depends"].get<std::vector<std::string>>();
        }
        if (j.contains("constrains"))
        {
            constrains = j["constrains"].get<std::vector<std::string>>();
        }
    }

    PackageInfo::PackageInfo(const std::string& n)
        : name(n)
    {
    }

    PackageInfo::PackageInfo(std::string&& n)
        : name(std::move(n))
    {
    }

    PackageInfo::PackageInfo(const std::string& n, const std::string& v, const std::string b, std::size_t bn)
        : name(n)
        , version(v)
        , build_string(b)
        , build_number(bn)
    {
    }

    bool PackageInfo::operator==(const PackageInfo& other) const
    {
        auto attrs = [](const PackageInfo& p)
        {
            return std::tie(
                p.name,
                p.version,
                p.build_string,
                p.noarch,
                p.build_number,
                p.channel,
                p.url,
                p.subdir,
                p.fn,
                p.license,
                p.size,
                p.timestamp,
                p.md5,
                p.sha256,
                p.track_features,
                p.depends,
                p.constrains,
                p.signatures,
                p.extra_metadata,
                p.defaulted_keys
            );
        };
        return attrs(*this) == attrs(other);
    }

    nlohmann::json PackageInfo::json_record() const
    {
        nlohmann::json j;
        j["name"] = name;
        j["version"] = version;
        j["channel"] = channel;
        j["url"] = url;
        j["subdir"] = subdir;
        j["fn"] = fn;
        j["size"] = size;
        j["timestamp"] = timestamp;
        j["build"] = build_string;
        j["build_string"] = build_string;
        j["build_number"] = build_number;
        j["license"] = license;
        j["track_features"] = track_features;
        if (!md5.empty())
        {
            j["md5"] = md5;
        }
        if (!sha256.empty())
        {
            j["sha256"] = sha256;
        }
        if (depends.empty())
        {
            j["depends"] = nlohmann::json::array();
        }
        else
        {
            j["depends"] = depends;
        }

        if (constrains.empty())
        {
            j["constrains"] = nlohmann::json::array();
        }
        else
        {
            j["constrains"] = constrains;
        }
        return j;
    }

    nlohmann::json PackageInfo::json_signable() const
    {
        nlohmann::json j;

        // Mandatory keys
        j["name"] = name;
        j["version"] = version;
        j["subdir"] = subdir;
        j["size"] = size;
        j["timestamp"] = timestamp;
        j["build"] = build_string;
        j["build_number"] = build_number;
        j["license"] = license;
        j["md5"] = md5;
        j["sha256"] = sha256;

        // Defaulted keys to empty arrays
        if (depends.empty())
        {
            if (defaulted_keys.find("depends") == defaulted_keys.end())
            {
                j["depends"] = nlohmann::json::array();
            }
        }
        else
        {
            j["depends"] = depends;
        }
        if (constrains.empty())
        {
            if (defaulted_keys.find("constrains") == defaulted_keys.end())
            {
                j["constrains"] = nlohmann::json::array();
            }
        }
        else
        {
            j["constrains"] = constrains;
        }

        // Optional keys that may be part of signed metadata
        j.merge_patch(nlohmann::json::parse(extra_metadata));

        return j;
    }

    std::string PackageInfo::str() const
    {
        return concat(name, "-", version, "-", build_string);
    }

    std::string PackageInfo::long_str() const
    {
        // TODO channel contains subdir right now?!
        return concat(channel, "::", name, "-", version, "-", build_string);
    }
}  // namespace mamba
