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
        version = pool_id2str(pool, s->evr);
        str = solvable_lookup_str(s, SOLVABLE_BUILDFLAVOR);
        if (str)
            build = str;
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
        name = j["name"];
        version = j["version"];
        channel = j["channel"];
        url = j["url"];
        subdir = j["subdir"];
        fn = j["fn"];
        size = j["size"];
        timestamp = j["timestamp"];
        if (j.contains("build"))
            build = j["build"];
        if (j.contains("build_number"))
            build_number = j["build_number"];
        if (j.contains("license"))
            license = j["license"];
        if (j.contains("md5"))
            md5 = j["md5"];
        if (j.contains("sha256"))
            sha256 = j["sha256"];
    }

    PackageInfo::PackageInfo(const std::string& n, const std::string& v,
                             const std::string b, std::size_t bn)
        : name(n), version(v), build(b), build_number(bn)
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
        j["build"] = build;
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

    std::tuple<std::string, std::string> MatchSpec::parse_version_and_build(const std::string& s)
    {
        std::size_t pos = s.find_last_of(" =");
        if (pos == s.npos || pos == 0)
        {
            std::string tmp = s;
            replace_all(tmp, " ", "");
            return { tmp, "" };
        }
        else
        {
            char c = s[pos];
            if (c == '=')
            {
                std::size_t pm1 = pos - 1;
                char d = s[pm1];
                if (d == '=' || d == '!' || d == '|' ||
                    d == ',' || d == '<' || d == '>' || d == '~')
                {
                    std::string tmp = s;
                    replace_all(tmp, " ", "");
                    return { tmp , "" };
                }
            }
            // c is either ' ' or pm1 is none of the forbidden chars

            std::string v = s.substr(0, pos),
                        b = s.substr(pos + 1);
            replace_all(v, " ", "");
            replace_all(b, " ", "");
            return { v, b };
        }
    };


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

        auto extract_kv = [&spec_str](const std::string& kv_string, auto& map)
        {
            static std::regex kv_re("([a-zA-Z0-9_-]+?)=([\"\']?)([^\'\"]*?)(\\2)(?:[\'\", ]|$)");
            std::cmatch kv_match;
            const char* text_iter = kv_string.c_str();

            while (std::regex_search(text_iter, kv_match, kv_re))
            {
                auto key = kv_match[1].str();
                auto value = kv_match[3].str();
                if (key.size() == 0 || value.size() == 0) 
                {
                    throw std::runtime_error("key-value mismatch in brackets " + spec_str);
                }
                text_iter += kv_match.position() + kv_match.length();
                map[key] = value;
            }
        };

        std::smatch match;

        // Step 3. strip off brackets portion
        static std::regex brackets_re(".*(?:(\\[.*\\]))");
        if (std::regex_search(spec_str, match, brackets_re))
        {
            auto brackets_str = match[1].str();
            brackets_str = brackets_str.substr(1, brackets_str.size() - 2);
            extract_kv(brackets_str, brackets);
            spec_str.erase(match.position(1), match.length(1));
        }

        // Step 4. strip off parens portion
        static std::regex parens_re(".*(?:(\\(.*\\)))");
        if (std::regex_search(spec_str, match, parens_re))
        {
            auto parens_str = match[1].str();
            parens_str = parens_str.substr(1, parens_str.size() - 2);
            extract_kv(parens_str, this->parens);
            if (parens_str.find("optional") != parens_str.npos)
            {
                optional = true;
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
        static std::regex version_build_re("([^ =<>!~]+)?([><!=~ ].+)?");
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

        // # Step 7. otherwise sort out version + build
        // spec_str = spec_str and spec_str.strip()
        if (!version.empty())
        {
            if (version.find("[") != version.npos)
            {
                throw std::runtime_error("Invalid match spec: multiple bracket sections not allowed " + spec);
            }

            version = std::string(strip(version));
            auto [pv, pb] = parse_version_and_build(std::string(strip(version)));

            version = pv;
            build = pb;

            // translate version '=1.2.3' to '1.2.3*'
            // is it a simple version starting with '='? i.e. '=1.2.3'
            if (version.size() >= 2 && version[0] == '=')
            {
                auto rest = version.substr(1);
                if (version[1] == '=' and build.empty())
                {
                    version = version.substr(2);
                }
                else if (rest.find_first_of("=,|") == rest.npos)
                {
                    if (build.empty() && version[version.size() - 1] != '*')
                    {
                        version = concat(version, "*");
                    }
                    else
                    {
                        version = rest;
                    }
                }
            }
        }
        else
        {
            version = "";
            build = "";
        }

        // TODO think about using a hash function here, (and elsewhere), like:
        // https://hbfs.wordpress.com/2017/01/10/strings-in-c-switchcase-statements/
        for (auto& [k, v] : brackets)
        {
            if (k == "build_number")
            {
                build_number = v;
            }
            else if (k == "build")
            {
                build = v;
            }
            else if (k == "version")
            {
                version = v;
            }
            else if (k == "channel")
            {
                channel = v;
            }
            else if (k == "subdir")
            {
                subdir = v;
            }
            else if (k == "url")
            {
                is_file = true;
                fn = v;
            }
            else if (k == "fn")
            {
                fn = v;
            }
        }
    }

    std::string MatchSpec::conda_build_form() const
    {
        assert(!name.empty());
        assert(!version.empty());
        std::stringstream res;
        res << name << " " << version;
        if (!build.empty())
            res << " " << build;
        return res.str();
    }

    std::string MatchSpec::str() const
    {
        std::stringstream res;
        // builder = []
        // brackets = []

        // channel_matcher = self._match_components.get('channel')
        // if channel_matcher and channel_matcher.exact_value:
        //     builder.append(text_type(channel_matcher))
        // elif channel_matcher and not channel_matcher.matches_all:
        //     brackets.append("channel=%s" % text_type(channel_matcher))

        // subdir_matcher = self._match_components.get('subdir')
        // if subdir_matcher:
        //     if channel_matcher and channel_matcher.exact_value:
        //         builder.append('/%s' % subdir_matcher)
        //     else:
        //         brackets.append("subdir=%s" % subdir_matcher)

        if (!channel.empty())
        {
            res << channel;
            if (!subdir.empty())
            {
                res << "/" << subdir;
            }
            res << "::";
        }
        // TODO when namespaces are implemented!
        // if (!ns.empty())
        // {
        //     res << ns;
        //     res << ":";
        // }
        res << (!name.empty() ? name : "*");
        std::vector<std::string> brackets;
        bool version_exact = false;

        auto is_complex_relation = [](const std::string& s)
        {
            return s.find_first_of("><$^|,") != s.npos;
        };

        if (!version.empty())
        {
            if (is_complex_relation(version))
            {
                brackets.push_back(concat("version='", version, "'"));
            }
            else if (starts_with(version, "!=") || starts_with(version, "~="))
            {
                if (!build.empty())
                {
                    brackets.push_back(concat("version='", version, "'"));
                }
                else
                {
                    res << " " << version;
                }
            }
            else if (ends_with(version, ".*"))
            {
                res << "=" + version.substr(0, version.size() - 2);
            }
            else if (ends_with(version, "*"))
            {
                res << "=" + version.substr(0, version.size() - 1);
            }
            else if (starts_with(version, "=="))
            {
                res << version;
                version_exact = true;
            }
            else
            {
                res << "==" << version;
                version_exact = true;
            }
        }

        if (!build.empty())
        {
            if (is_complex_relation(build))
            {
                brackets.push_back(concat("build='", build, "'"));
            }
            else if (build.find("*") != build.npos)
            {
                brackets.push_back(concat("build=", build));
            }
            else if (version_exact)
            {
                res << "=" << build;
            }
            else
            {
                brackets.push_back(concat("build=", build));
            }
        }

        // _skip = {'channel', 'subdir', 'name', 'version', 'build'}
        // if 'url' in self._match_components and 'fn' in self._match_components:
        //     _skip.add('fn')
        // for key in self.FIELD_NAMES:
        //     if key not in _skip and key in self._match_components:
        //         if key == 'url' and channel_matcher:
        //             # skip url in canonical str if channel already included
        //             continue
        //         value = text_type(self._match_components[key])
        //         if any(s in value for s in ', ='):
        //             brackets.append("%s='%s'" % (key, value))
        //         else:
        //             brackets.append("%s=%s" % (key, value))

        if (brackets.size())
        {
            res << "[" << join(",", brackets) << "]";
        }
        return res.str();
    }
}
