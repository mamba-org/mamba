#include "package_info.hpp"
#include "util.hpp"

namespace mamba
{
    PackageInfo::PackageInfo(Solvable* s)
    {
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
        channel = s->repo->name;  // note this can and should be <unknown> when e.g. installing from a tarball
        url = channel + "/" + solvable_lookup_str(s, SOLVABLE_MEDIAFILE);
        subdir = solvable_lookup_str(s, SOLVABLE_MEDIADIR);
        fn = solvable_lookup_str(s, SOLVABLE_MEDIAFILE);
        str = solvable_lookup_str(s, SOLVABLE_LICENSE);
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
        using namespace std::string_literals;
        assign_or(j, "name", name, ""s);
        assign_or(j, "version", version, ""s);
        assign_or(j, "channel", channel, ""s);
        assign_or(j, "url", url, ""s);
        assign_or(j, "subdir", subdir, ""s);
        assign_or(j, "fn", fn, ""s);
        assign_or(j, "size", size, size_t(0));
        assign_or(j, "timestamp", timestamp, size_t(0));
        std::string bs;
        assign_or(j, "build_string", bs, "<UNKNOWN>"s);
        if (bs.size() && bs[0] == '<')
        {
            assign_or(j, "build", bs, "<UNKNOWN>"s);
        }
        build_string = bs;
        assign_or(j, "build_number", build_number, size_t(0));
        assign_or(j, "license", license, ""s);
        assign_or(j, "md5", md5, ""s);
        assign_or(j, "sha256", sha256, ""s);
    }

    PackageInfo::PackageInfo(const std::string& n, const std::string& v,
                             const std::string b, std::size_t bn)
        : name(n), version(v), build_string(b), build_number(bn)
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
}