#ifndef MAMBA_PARSING
#define MAMBA_PARSING

#include <array>
#include <string>
#include <stdexcept>
#include <string_view>
#include <cassert>

extern "C"
{
    #include "solv/pool.h"
    #include "solv/repo.h"
    #include "solv/queue.h"
    #include "solv/solver.h"
}

#include "thirdparty/pystring/pystring.hpp"

#include <iostream>

struct parsed_relation {
    int relation = -1;
    bool fuzzy = false;
    std::string_view version;
};

// TODO better concat
std::string normalize_version(std::string_view version, int build_number)
{
    static const std::string zero = "0";
    auto split = pystring::split(version, ".");
    while (split.size() < 3) split.push_back(zero);

    std::string out = pystring::join(".", split);
    out += ".";
    out += std::to_string(build_number);
    return out;
}

std::string normalize_version(std::string_view version, int build_number, std::string_view build)
{
    static const std::string zero = "0";
    auto split = pystring::split(version, ".");
    while (split.size() < 3) split.push_back(zero);

    std::string out = pystring::join(".", split);
    out += ".";
    out += std::to_string(build_number);
    out += "-";
    out += build;
    return out;
}

parsed_relation get_relation(const std::string_view& vs)
{
    // std::cout << "VS: " << vs << std::endl;
    parsed_relation res;
    std::size_t start = 0, end;

    if (vs[0] == '<' || vs[0] == '>')
    {
        res.relation = (vs[0] == '<') ? REL_LT : REL_GT;
        if (vs[1] == '=')
        {
            res.relation |= REL_EQ;
            start = 2;
        }
        else
        {
            start = 1;
        }
    }
    else if (vs[0] == '=' && vs[1] == '=')
    {
        res.relation = REL_EQ;
        start = 2;
    }

    if (vs[vs.size() - 1] == '*')
    {
        res.fuzzy = true;
        end = vs.size() - 1;
    }
    else
    {
        end = vs.size();
    }

    if (res.relation == -1)
    {
        res.relation = REL_EQ;
    }

    if (res.fuzzy && res.relation != REL_EQ)
    {
        if (res.relation == (REL_GT | REL_EQ))
        {
            res.relation = REL_EQ;
        }
        else
        {
            if (res.relation & REL_LT)
            {
                // just remove * from end, do nothing
                res.fuzzy = false;
            }
            else
            {
                std::cout << vs << std::endl;
                throw std::runtime_error("Cannot match fuzzy version with other than `==`");
            }

            // TODO fix this intelligently with build string comparison ... ?
        }
    }

    res.version = std::string_view(&vs[start], end - start);
    return res;
}

std::ostream& operator<<(std::ostream& os, const parsed_relation& rel)
{
    if (rel.fuzzy) { os << "fzy "; }
    switch (rel.relation){
        case REL_GT: os << ">"; break;
        case REL_GT | REL_EQ: os << ">="; break;
        case REL_LT: os << "<"; break;
        case REL_LT | REL_EQ: os << "<="; break;
        case REL_EQ: os << "=="; break;
    }
    os << "  " << rel.version;
    return os;
}

Id get_between(Id name_id, const std::string_view& v_lo, const std::string_view& v_hi, Pool* pool)
{
    auto evr1 = pool_strn2id(pool, &v_lo[0], v_lo.size(), 1);
    auto evr2 = pool_strn2id(pool, &v_hi[0], v_hi.size(), 1);

    auto xr1 = pool_rel2id(pool, name_id, evr1, REL_GT | REL_EQ, 1);
    auto xr2 = pool_rel2id(pool, name_id, evr2, REL_LT, 1);

    return pool_rel2id(pool, xr1, xr2, REL_WITH, 1);
}

Id get_fuzzy_relation(Id name_id, const std::string_view& vnumber, Pool* pool)
{
    int idx_back, idx_front;
    std::size_t lversion = 0;
    if (vnumber.back() == '.')
    {
        // remove last dot!
        idx_back = vnumber.size() - 1;
    }
    else
    {
        idx_back = vnumber.size();
    }

    for (idx_front = idx_back - 1; idx_front >= 0; --idx_front)
    {
        if (vnumber[idx_front] == '.')
        {
            auto* end_ptr = const_cast<char*>(&vnumber[idx_back]);
            lversion = std::strtoul(&vnumber[idx_front + 1], &end_ptr, 10);
            break;
        }
    }
    if (idx_front == -1)
    {
        auto* end_ptr = const_cast<char*>(&vnumber[idx_back]);
        lversion = std::strtoul(&vnumber[idx_front + 1], &end_ptr, 10);
    }

    // lower_version = vnumber[0] -> vnumber[idx_back];
    std::string higher_version(vnumber.begin(), vnumber.begin() + idx_front + 1);
    higher_version += std::to_string(lversion + 1);

    return get_between(name_id, std::string_view(&vnumber[0], idx_back),
                       higher_version, pool);
}

Id parse_version_relation(Id name_id,
                          const std::string_view& version_rel,
                          Pool* pool)
{
    if (version_rel.size() == 1)
    {
        if (version_rel[0] == '*')
        {
            return name_id;
        }
        else
        {
            // TODO make sure that char is alpha num
            auto v_lo = normalize_version(version_rel, 0);
            auto v_hi = normalize_version(version_rel, 9999);

            return get_between(name_id, v_lo, v_hi, pool);
        }
    }

    // now we know that the size of `c` == 2 or larger!
    auto rel = get_relation(version_rel);

    if (!rel.fuzzy)
    {
        if (rel.relation == REL_EQ)
        {
            if (4 == std::count_if(rel.version.begin(), rel.version.end(), [](char c) { return c == '.';}))
            {
                Id evr_id = pool_strn2id(pool, &rel.version[0], rel.version.size(), 1);
                return pool_rel2id(pool, name_id, evr_id, rel.relation, 1);
            }
            auto v_lo = normalize_version(rel.version, 0);
            auto v_hi = normalize_version(rel.version, 9999);

            return get_between(name_id, v_lo, v_hi, pool);
        }
        else
        {
            assert(rel.relation & REL_GT || rel.relation & REL_LT);
            Id evr_id = pool_strn2id(pool, &rel.version[0], rel.version.size(), 1);
            return pool_rel2id(pool, name_id, evr_id, rel.relation, 1);
        }
    }
    else
    {
        return get_fuzzy_relation(name_id, rel.version, pool);
    }
}

Id parse_rel_string(Id name_id, const std::string_view& str, Pool* pool)
{
    // TODO implement operator precedence
    for (std::size_t i = 0; i < str.size(); ++i)
    {
        switch (str[i])
        {
            case '|':
            {
                auto lhs_rel = parse_version_relation(name_id, std::string_view(&str[0], i), pool);
                return pool_rel2id(pool, lhs_rel,
                                  parse_rel_string(name_id, std::string_view(&str[i + 1], str.size() - (i + 1)), pool),
                                  REL_OR, 1);
            }
            case ',':
            {
                auto lhs_rel = parse_version_relation(name_id, std::string_view(&str[0], i), pool);
                return pool_rel2id(pool, lhs_rel,
                                  parse_rel_string(name_id, std::string_view(&str[i + 1], str.size() - (i + 1)), pool),
                                  REL_WITH, 1);
            }
            default:
                break;
        }
    }
    return parse_version_relation(name_id, str, pool);
}

Id parse_to_relation(const std::string_view& dep, Pool* pool) {
    auto pkg = pystring::split(dep, " ");

    if (pkg.size() == 1)
    {
        return pool_strn2id(pool, &dep[0], dep.size(), 1);
    }
    else
    {
        const auto& name = pkg[0];
        Id name_id;
        if (pkg.size() == 2)
        {
            name_id = pool_strn2id(pool, &name[0], name.size(), 1);
        }
        else
        {
            assert(pkg.size() == 3);
            // add feature to dependency
            std::stringstream os;
            os << name << "[" << pkg[2] << "]";

            std::string tmp_string = os.str();
            name_id = pool_strn2id(pool, tmp_string.c_str(), tmp_string.size(), 1);
        }

        // std::cout << "PARTS ";
        // for (auto& el : pkg)
        // {
        //     std::cout << el << " :: ";
        // }
        // std::cout << "\n";

        auto id = parse_rel_string(name_id, pkg[1], pool);
        return id;
    }
}

#endif
