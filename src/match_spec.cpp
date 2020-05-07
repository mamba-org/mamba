#include "match_spec.hpp"
#include "output.hpp"
#include "url.hpp"
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
        json["name"] = name;

        version = pool_id2str(pool, s->evr);
        json["version"] = version;

        str = solvable_lookup_str(s, SOLVABLE_BUILDFLAVOR);
        if (str)
        {
            build = str;
            json["build"] = str;
        }

        str = solvable_lookup_str(s, SOLVABLE_BUILDVERSION);
        if (str)
        {
            n = std::stoi(str);
            json["build_number"] = n;
            build_number = n;
        }

        channel = s->repo->name;  // note this can and should be <unknown> when e.g. installing from a tarball
        json["channel"] = channel;

        url = channel + "/" + solvable_lookup_str(s, SOLVABLE_MEDIAFILE);

        subdir = solvable_lookup_str(s, SOLVABLE_MEDIADIR);
        json["subdir"] = subdir;

        fn = solvable_lookup_str(s, SOLVABLE_MEDIAFILE);
        json["fn"] = fn;

        str = solvable_lookup_str(s, SOLVABLE_LICENSE);
        if (str)
        {
            license = str;
            json["license"] = str;
        }

        size = solvable_lookup_num(s, SOLVABLE_DOWNLOADSIZE, -1);
        json["size"] = size;

        timestamp = solvable_lookup_num(s, SOLVABLE_BUILDTIME, 0) * 1000;
        json["timestamp"] = timestamp;

        str = solvable_lookup_checksum(s, SOLVABLE_PKGID, &check_type);
        if (str)
        {
            md5 = str;
            json["md5"] = str;
        }

        str = solvable_lookup_checksum(s, SOLVABLE_CHECKSUM, &check_type);
        if (str)
        {
            sha256 = str;
            json["sha256"] = str;
        }

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
        json["depends"] = depends;
        json["constrains"] = constrains;
    }

    PackageInfo::PackageInfo(nlohmann::json&& j)
            : json(std::move(j))
    {
        name = j["name"];

        version = j["version"];

        if (j.find("build") != j.end())
        {
            build = j["build"];
        }

        if (j.find("build_number") != j.end())
        {
            build_number = j["build_number"];
        }

        channel = j["channel"];

        url = j["url"];

        subdir = j["subdir"];

        fn = j["fn"];

        if (j.find("license") != j.end())
        {
            license = j["license"];
        }

        size = j["size"];

        timestamp = j["timestamp"];

        if (j.find("md5") != j.end())
        {
            md5 = j["md5"];
        }

        if (j.find("sha256") != j.end())
        {
            sha256 = j["sha256"];
        }
    }

    PackageInfo::PackageInfo(const std::string& n, const std::string& v,
                             const std::string b, std::size_t bn)
        : name(n), version(v), build(b), build_number(bn)
    {
        json["name"] = n;
        json["version"] = v;
        json["build"] = b;
        json["build_number"] = bn;
    }

    std::string PackageInfo::str() const
    {
        return concat(name, "-", version, "-", build);
    }

    std::string PackageInfo::long_str() const
    {
        return concat(channel, "/", subdir, "::", name, "-", version, "-", build);
    }

    MatchSpec::MatchSpec(const std::string& i_spec)
        : spec(i_spec)
    {
        parse();
    }

    void MatchSpec::parse()
    {
        LOG_INFO << "Parsing MatchSpec " << spec;
        std::string spec_str = spec;

        std::size_t idx = spec_str.find('#');
        if (idx != std::string::npos)
        {
            spec_str = spec_str.substr(0, idx);
            spec_str = strip(spec_str);
        }

        if (is_package_file(spec_str))
        {
            if (!is_url(spec_str))
            {
                LOG_INFO << "need to expand path!";
                // spec_str = unquote(path_to_url(expand(spec_str)))
            }
            LOG_INFO << "Got a package file: " << spec_str << std::endl;
        }

        // # Step 3. strip off brackets portion
        // brackets = {}
        // m3 = re.match(r'.*(?:(\[.*\]))', spec_str)
        // if m3:
        //     brackets_str = m3.groups()[0]
        //     spec_str = spec_str.replace(brackets_str, '')
        //     brackets_str = brackets_str[1:-1]
        //     m3b = re.finditer(r'([a-zA-Z0-9_-]+?)=(["\']?)([^\'"]*?)(\2)(?:[, ]|$)', brackets_str)
        //     for match in m3b:
        //         key, _, value, _ = match.groups()
        //         if not key or not value:
        //             raise InvalidMatchSpec(original_spec_str, "key-value mismatch in brackets")
        //         brackets[key] = value

        std::regex brackets_re(".*(?:(\\[.*\\]))");
        std::smatch match;
        if (std::regex_match(spec_str, match, brackets_re))
        {
            auto brackets_str = match[1].str();
            brackets_str = brackets_str.substr(1, brackets_str.size() - 2);
            std::regex kv_re("([a-zA-Z0-9_-]+?)=([\"\']?)([^\'\"]*?)(\\2)(?:[\'\", ]|$)");

            std::cmatch kv_match;
            const char* text_iter = brackets_str.c_str();
            while (std::regex_search(text_iter, kv_match, kv_re))
            {
                auto key = kv_match[1].str();
                auto value = kv_match[3].str();
                if (key.size() == 0 || value.size() == 0) 
                {
                    throw std::runtime_error("key-value mismatch in brackets " + spec);
                }
                text_iter += kv_match.position() + kv_match.length();
                brackets[key] = value;
            }
            spec_str.erase(match.position(1), match.length(1));
        }

        auto m5 = rsplit(spec_str, ":", 2);
        auto m5_len = m5.size();
        std::string channel_str;
        if (m5_len == 3)
        {
            channel = m5[0];
            ns = m5[1];
            spec_str = m5[2];
        }
        else if (m5_len == 2)
        {
            ns = m5[0];
            spec_str = m5[1];
        }
        else if (m5_len == 1)
        {
            spec_str = m5[0];
        }
        else
        {
            throw std::runtime_error("Parsing of channel / namespace / subdir failed.");
        }
        // TODO implement Channel, and parsing of the channel here!
        // channel = subdir = channel_str;
        // channel, subdir = _parse_channel(channel_str)
        // if 'channel' in brackets:
        //     b_channel, b_subdir = _parse_channel(brackets.pop('channel'))
        //     if b_channel:
        //         channel = b_channel
        //     if b_subdir:
        //         subdir = b_subdir
        // if 'subdir' in brackets:
        //     subdir = brackets.pop('subdir')

        // TODO This is #6 of the spec parsing -- we still need to port the others!
        std::regex version_build_re("([^ =<>!~]+)?([><!=~ ].+)?");
        std::smatch vb_match;
        if (std::regex_match(spec_str, vb_match, version_build_re))
        {
            name = vb_match[1].str();
            version = strip(vb_match[2].str());
            if (name.size() == 0)
            {
                throw std::runtime_error("Invalid spec, no package name found: " + spec_str);
            }
        }
        else
        {
            throw std::runtime_error("Invalid spec, no package name found: " + spec_str);
        }
    }
} 
