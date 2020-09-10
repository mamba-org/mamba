// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <functional>
#include <map>

#include "mamba/package_info.hpp"
#include "mamba/output.hpp"
#include "mamba/util.hpp"

namespace mamba
{
    namespace
    {
        template <class T>
        std::string get_package_info_field(const PackageInfo&, T PackageInfo::*field);

        template <>
        std::string get_package_info_field<std::string>(const PackageInfo& pkg,
                                                        std::string PackageInfo::*field)
        {
            return pkg.*field;
        }

        template <>
        std::string get_package_info_field<size_t>(const PackageInfo& pkg,
                                                   size_t PackageInfo::*field)
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
        return [getter](const PackageInfo& lhs, const PackageInfo& rhs) {
            return getter(lhs) < getter(rhs);
        };
    }

    PackageInfo::compare_fun PackageInfo::equal(const std::string& member)
    {
        auto getter = get_field_getter(member);
        return [getter](const PackageInfo& lhs, const PackageInfo& rhs) {
            return getter(lhs) == getter(rhs);
        };
    }

    PackageInfo::PackageInfo(Solvable* s)
    {
        // Note: this function (especially the checksum part) is NOT YET threadsafe!
        Pool* pool = s->repo->pool;
        const char* str;
        int n;
        Id check_type;
        Queue q;

        name = pool_id2str(pool, s->name);
        version = pool_id2str(pool, s->evr);
        str = solvable_lookup_str(s, SOLVABLE_BUILDFLAVOR);
        if (str)
            build_string = str;
        str = solvable_lookup_str(s, SOLVABLE_BUILDVERSION);
        if (str)
        {
            n = std::stoi(str);
            build_number = n;
        }

        Id real_repo_key = pool_str2id(pool, "solvable:real_repo_url", 1);
        if (solvable_lookup_str(s, real_repo_key))
        {
            channel = solvable_lookup_str(s, real_repo_key);
        }
        else
        {
            channel = s->repo->name;  // note this can and should be <unknown> when e.g.
                                      // installing from a tarball
        }

        url = channel + "/" + check_char(solvable_lookup_str(s, SOLVABLE_MEDIAFILE));
        subdir = check_char(solvable_lookup_str(s, SOLVABLE_MEDIADIR));
        fn = check_char(solvable_lookup_str(s, SOLVABLE_MEDIAFILE));
        str = check_char(solvable_lookup_str(s, SOLVABLE_LICENSE));
        if (str)
            license = str;
        size = solvable_lookup_num(s, SOLVABLE_DOWNLOADSIZE, -1);
        timestamp = solvable_lookup_num(s, SOLVABLE_BUILDTIME, 0) * 1000;
        str = solvable_lookup_checksum(s, SOLVABLE_PKGID, &check_type);
        if (str)
            md5 = str;
        str = solvable_lookup_checksum(s, SOLVABLE_CHECKSUM, &check_type);
        if (str)
            sha256 = str;

        queue_init(&q);
        solvable_lookup_deparray(s, SOLVABLE_REQUIRES, &q, -1);
        depends.resize(q.count);
        for (int i = 0; i < q.count; ++i)
        {
            depends[i] = pool_dep2str(pool, q.elements[i]);
        }
        queue_empty(&q);
        solvable_lookup_deparray(s, SOLVABLE_CONSTRAINS, &q, -1);
        constrains.resize(q.count);
        for (int i = 0; i < q.count; ++i)
        {
            constrains[i] = pool_dep2str(pool, q.elements[i]);
        }
        queue_free(&q);
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

    PackageInfo::PackageInfo(const std::string& n,
                             const std::string& v,
                             const std::string b,
                             std::size_t bn)
        : name(n)
        , version(v)
        , build_string(b)
        , build_number(bn)
    {
    }

    nlohmann::json PackageInfo::json() const
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
        j["md5"] = md5;
        j["sha256"] = sha256;
        if (!depends.empty())
            j["depends"] = depends;
        if (!constrains.empty())
            j["constrains"] = constrains;
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
