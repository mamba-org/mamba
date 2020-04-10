///////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2008-2010, Sony Pictures Imageworks Inc
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// Redistributions of source code must retain the above copyright notice,
// this list of conditions and the following disclaimer.
// Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
// Neither the name of the organization Sony Pictures Imageworks nor the
// names of its contributors
// may be used to endorse or promote products derived from this software
// without specific prior written permission.
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS
// IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
// TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
// PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER
// OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
///////////////////////////////////////////////////////////////////////////////

#ifndef PYSTRING_HPP
#define PYSTRING_HPP


#include <algorithm>
#include <cctype>
#include <cstring>
#include <iostream>
#include <sstream>
#include <vector>

#include "pystring_utils.hpp"

#include "pystring_header.hpp"

namespace pystring
{

using namespace std::string_literals;

#if defined(_WIN32) || defined(_WIN64) || defined(_WINDOWS) || defined(_MSC_VER)
#ifndef WINDOWS
#define WINDOWS
#endif
#endif

// This definition codes from configure.in in the python src.
// Strictly speaking this limits us to str sizes of 2**31.
// Should we wish to handle this limit, we could use an architecture
// specific #defines and read from ssize_t (unistd.h) if the header exists.
// But in the meantime, the use of std::ptrdiff_t assures maximum arch compatibility.
// This must also equal the size used in the end = MAX_32BIT_INT default arg.

/* helper macro to fixup start/end slice values */
#define ADJUST_INDICES(start, end, len)     \
    if (end > len)                          \
        end = len;                          \
    else if (end < 0) {                     \
        end += len;                         \
        if (end < 0)                        \
        end = 0;                            \
    }                                       \
    if (start < 0) {                        \
        start += len;                       \
        if (start < 0)                      \
        start = 0;                          \
    }

    enum strip_dir
    {
        left,
        right,
        both
    };

    namespace {

        void split_whitespace(const string_view& str, std::vector<string_view>& result, std::ptrdiff_t maxsplit)
        {
            std::size_t i, j, len = str.size();

            for (i = j = 0; i < len;) {
                while (i < len && ::isspace(str[i]))
                {
                    i++;
                }

                j = i;

                while (i < len && !::isspace(str[i]))
                {
                    i++;
                }

                if (j < i) {
                    if (maxsplit-- <= 0)
                    {
                        break;
                    }

                    result.push_back(str.substr(j, i - j));

                    while (i < len && ::isspace(str[i])) {
                        i++;
                    }
                    j = i;
                }
            }
            if (j < len) {
                result.push_back(str.substr(j, len - j));
            }
        }

        void rsplit_whitespace(const string_view& str, std::vector<string_view>& result, std::ptrdiff_t maxsplit)
        {
            std::size_t len = str.size();
            std::size_t i, j;
            for (i = j = len; i > 0;)
            {
                while (i > 0 && ::isspace(str[i - 1]))
                {
                    i--;
                }
                j = i;

                while (i > 0 && !::isspace(str[i - 1]))
                {
                    i--;
                }

                if (j > i)
                {
                    if (maxsplit-- <= 0)
                        break;

                    result.push_back(str.substr(i, j - i));

                    while (i > 0 && ::isspace(str[i - 1]))
                        i--;
                    j = i;
                }
            }
            if (j > 0)
            {
                result.push_back(str.substr(0, j));
            }

            std::reverse(result.begin(), result.end());
        }
    } // namespace

    std::vector<string_view> split(const string_view& str, const string_view& sep, std::ptrdiff_t maxsplit)
    {
        std::vector<string_view> result;

        if (maxsplit < 0)
        {
            maxsplit = std::numeric_limits<std::ptrdiff_t>::max(); // result.max_size();
        }

        if (detail::size(sep) == 0)
        {
            split_whitespace(str, result, maxsplit);
            return result;
        }

        std::size_t i, j, len = str.size(), n = sep.size();

        i = j = 0;

        while (i + n <= len)
        {
            if (str[i] == sep[0] && str.substr(i, n) == sep)
            {
                if (maxsplit-- <= 0)
                {
                    break;
                }

                result.push_back(str.substr(j, i - j));
                i = j = i + n;
            }
            else
            {
                i++;
            }
        }

        result.push_back(str.substr(j, len - j));

        return result;
    }

    std::vector<string_view> rsplit(const string_view& str, const string_view& sep, std::ptrdiff_t maxsplit)
    {
        if (maxsplit < 0)
        {
            return split(str, sep, maxsplit);
        }

        std::vector<string_view> result;

        if (sep.size() == 0)
        {
            rsplit_whitespace(str, result, maxsplit);
            return result;
        }

        std::ptrdiff_t i, j, len = static_cast<std::ptrdiff_t>(str.size()),
                             n = static_cast<std::ptrdiff_t>(sep.size());

        i = j = len;

        while (i >= n)
        {
            if (str[i - 1] == sep[n - 1] && str.substr(i - n, n) == sep)
            {
                if (maxsplit-- <= 0)
                {
                    break;
                }

                result.push_back(str.substr(i, j - i));
                i = j = i - n;
            }
            else
            {
                i--;
            }
        }

        result.push_back(str.substr(0, j));
        std::reverse(result.begin(), result.end());

        return result;
    }

    template <class S = std::string>
    string_view do_strip(const string_view& str, strip_dir striptype, const string_view& chars)
    {
        std::ptrdiff_t i, j;
        std::ptrdiff_t len = static_cast<std::ptrdiff_t>(str.size());
		std::ptrdiff_t charslen = static_cast<std::ptrdiff_t>(chars.size());

        if (charslen == 0)
        {
            i = 0;
            if (striptype != strip_dir::right)
            {
                while (i < len && ::isspace(str[i]))
                {
                    i++;
                }
            }

            j = len;
            if (striptype != strip_dir::left)
            {
                do 
                {
                    j--;
                }
                while (j >= i && ::isspace(str[j]));

                j++;
            }

        }
        else
        {
        	// TODO check that this still works without \0 termination
            const char* sep = &chars[0];

            i = 0;
            if (striptype != strip_dir::right)
            {
                while (i < len && memchr(sep, str[i], charslen))
                {
                    i++;
                }
            }

            j = len;
            if (striptype != strip_dir::left)
            {
                do
                {
                    j--;
                }
                while (j >= i && memchr(sep, str[j], charslen));
                j++;
            }
        }

        if (i == 0 && j == len)
        {
            return str;
        }
        else
        {
            return str.substr(i, j - i);
        }
    }

    template <class S>
    std::array<string_view, 3> partition(const string_view& str, const string_view& sep)
    {
        std::array<string_view, 3> result;
        std::ptrdiff_t index = find(str, sep);

        if (index < 0)
        {
            result[0] = str;
            result[1] = "";
            result[2] = "";
        }
        else
        {
            result[0] = str.substr(0, index);
            result[1] = sep;
            result[2] = str.substr(index + sep.size(), str.size());
        }
        return result;
    }

    template <class S>
    std::array<string_view, 3> rpartition(const string_view& str, const string_view& sep)
    {
        std::array<string_view, 3> result;
        std::ptrdiff_t index = rfind(str, sep);

        if (index < 0)
        {
            result[0] = "";
            result[1] = "";
            result[2] = str;
        }
        else
        {
            result[0] = str.substr(0, index);
            result[1] = sep;
            result[2] = str.substr(index + sep.size(), str.size());
        }

        return result;
    }

    string_view strip(const string_view& str, const string_view& chars)
    {
        return do_strip(str, strip_dir::both, chars);
    }

    string_view lstrip(const string_view& str, const string_view& chars)
    {
        return do_strip(str, strip_dir::left, chars);
    }

    string_view rstrip(const string_view& str, const string_view& chars)
    {
        return do_strip(str, strip_dir::right, chars);
    }

    template <class S, class C>
   	string join(const S& str, const C& seq)
    {
        std::size_t seqlen = detail::size(seq), i;

        if (seqlen == 0)
        {
            return "";
        }

        std::ostringstream buf;
        buf << seq[0];
        for (i = 1; i < seqlen; ++i)
        {
            buf << str << seq[i];
        }

        return buf.str();
    }

    namespace {

        /******************************************************************** 
         * Matches the end (direction >= 0) or start (direction < 0) of self
         * against substr, using the start and end arguments. Returns
         * -1 on error, 0 if not found and 1 if found.
         ********************************************************************/
        template <class S, class C>
        std::ptrdiff_t _string_tailmatch(const S& self, const C& substr,
                                         std::ptrdiff_t start, std::ptrdiff_t end, std::ptrdiff_t direction)
        {
            std::ptrdiff_t len = static_cast<std::ptrdiff_t>(detail::size(self));
            std::ptrdiff_t slen = static_cast<std::ptrdiff_t>(detail::size(substr));

            const char* sub = detail::c_str(substr);
            const char* str = detail::c_str(self);

            ADJUST_INDICES(start, end, len);

            if (direction < 0)
            {
                // startswith
                if (start + slen > len)
                {
                    return 0;
                }
            }
            else
            {
                // endswith
                if (end - start < slen || start > len)
                {
                    return 0;
                }
                if (end - slen > start)
                {
                    start = end - slen;
                }
            }
            if (end - start >= slen)
            {
                return (!std::memcmp(str + start, sub, slen));
            }

            return 0;
        }
    } // namespace

    template <class S = std::string, class C = S>
    bool endswith(const S& str, const C& suffix, std::ptrdiff_t start, std::ptrdiff_t end)
    {
        std::ptrdiff_t result = _string_tailmatch(str, suffix, (std::ptrdiff_t)start, (std::ptrdiff_t)end, +1);
        // if (result == -1) // TODO: Error condition

        return static_cast<bool>(result);
    }

    template <class S = std::string, class C>
    bool startswith(const S& str, const C& prefix, std::ptrdiff_t start, std::ptrdiff_t end)
    {
        std::ptrdiff_t result = _string_tailmatch(str, prefix, (std::ptrdiff_t)start, (std::ptrdiff_t)end, -1);
        // if (result == -1) // TODO: Error condition

        return static_cast<bool>(result);
    }

    template <class S = std::string>
    bool isalnum(const S& str)
    {
        auto it = std::begin(str),
             end = detail::fast_end(str);
        if (it == end)
        {
            return false;
        }

        for (; it != end; ++it)
        {
            if (::isalnum(*it) == 0)
            {
                return false;
            }
        }
        return true;
    }

    template <class S = std::string>
    bool isalpha(const S& str)
    {
        auto it = std::begin(str),
             end = detail::fast_end(str);
        if (it == end)
        {
            return false;
        }

        for (; it != end; ++it)
        {
            if (!::isalpha(*it))
            {
                return false;
            }
        }
        return true;
    }

    bool isdigit(const string_view& str)
    {
        std::size_t len = str.size(), i;
        if (len == 0)
        {
            return false;
        }

        for (i = 0; i < len; ++i)
        {
            if (!::isdigit(str[i]))
                return false;
        }

        return true;
    }

    bool islower(const string_view& str)
    {
        std::size_t len = str.size(), i;
        if (len == 0)
        {
            return false;
        }

        for (i = 0; i < len; ++i)
        {
            if (!::islower(str[i]))
            {
                return false;
            }
        }
        return true;
    }

    bool isspace(const string_view& str)
    {
        std::size_t len = str.size(), i;
        if (len == 0)
        {
            return false;
        }

        for (i = 0; i < len; ++i)
        {
            if (!::isspace(str[i]))
        	{
		        return false;        		
        	}
        }
        return true;
    }

    bool istitle(const string_view& str)
    {
        std::size_t len = str.size(), i;

        if (len == 0)
        {
            return false;
        }
        if (len == 1)
        {
            return ::isupper(str[0]);
        }

        bool cased = false, previous_is_cased = false;

        for (i = 0; i < len; ++i)
        {
            if (::isupper(str[i]))
            {
                if (previous_is_cased)
                {
                    return false;
                }

                previous_is_cased = true;
                cased = true;
            }
            else if (::islower(str[i]))
            {
                if (!previous_is_cased)
                {
                    return false;
                }

                previous_is_cased = true;
                cased = true;

            } else
            {
                previous_is_cased = false;
            }
        }

        return cased;
    }

    bool isupper(const string_view& str)
    {
        std::size_t len = str.size(), i;
        if (len == 0)
        {
            return false;
        }

        for (i = 0; i < len; ++i)
        {
            if (!::isupper(str[i]))
            {
                return false;
            }
        }
        return true;
    }

    template <class S>
    S capitalize(const S& str)
    {
        S s(str);
        std::size_t len = detail::size(s), i;

        if (len > 0 && ::islower(s[0]))
        {
            s[0] = (char)::toupper(s[0]);
        }

        for (i = 1; i < len; ++i)
        {
            if (::isupper(s[i]))
            {
                s[i] = (char)::tolower(s[i]);
            }
        }

        return s;
    }

    template <class S>
    S lower(const S& str)
    {
        S s(str);
        std::size_t len = detail::size(s), i;

        for (i = 0; i < len; ++i)
        {
            if (::isupper(s[i]))
            {
                s[i] = (char)::tolower(s[i]);
            }
        }

        return s;
    }

    template <class S>
    S upper(const S& str)
    {
        S s(str);
        std::size_t len = detail::size(s), i;

        for (i = 0; i < len; ++i)
        {
            if (::islower(s[i]))
            {
                s[i] = (char)::toupper(s[i]);
            }
        }

        return s;
    }

    template <class S>
    S swapcase(const S& str)
    {
        S s(str);
        std::size_t len = detail::size(s), i;

        for (i = 0; i < len; ++i)
        {
            if (::islower(s[i]))
            {
                s[i] = (char)::toupper(s[i]);
            }
            else if (::isupper(s[i]))
            {
                s[i] = (char)::tolower(s[i]);
            }
        }

        return s;
    }

    template <class S>
    S title(const S& str)
    {
        S s(str);
        std::size_t len = detail::size(s), i;
        bool previous_is_cased = false;

        for (i = 0; i < len; ++i)
        {
            std::ptrdiff_t c = s[i];
            if (::islower(c))
            {
                if (!previous_is_cased)
                {
                    s[i] = (char)::toupper(c);
                }
                previous_is_cased = true;
            }
            else if (::isupper(c))
            {
                if (previous_is_cased)
                {
                    s[i] = (char)::tolower(c);
                }
                previous_is_cased = true;
            }
            else
            {
                previous_is_cased = false;
            }
        }

        return s;
    }

    template <class S>
    string translate(const S& str, const string_view& table, const string_view& deletechars)
    {
        string s;
        std::size_t len = detail::size(str), del_len = deletechars.size();

        if (table.size() != 256)
        {
            // TODO : raise exception instead
            return str;
        }

        // if nothing is deleted, use faster code
        if (del_len == 0)
        {
            s = str;
            for (std::size_t i = 0; i < len; ++i)
            {
                s[i] = table[s[i]];
            }
            return s;
        }

        std::ptrdiff_t trans_table[256];
        for (int i = 0; i < 256; i++)
        {
            trans_table[i] = table[i];
        }

        for (std::size_t i = 0; i < del_len; i++)
        {
            trans_table[(std::size_t) deletechars[i]] = -1;
        }

        for (std::size_t i = 0; i < len; ++i)
        {
            if (trans_table[(std::size_t) str[i]] != -1)
            {
                s += table[str[i]];
            }
        }

        return s;
    }

    struct fillstream
    {
    	char c;
    	std::ptrdiff_t n;
    };

	std::ostream& operator<<(std::ostream& str, const fillstream& fs)
	{
		for (std::ptrdiff_t i = 0; i < fs.n; ++i)
		{
			str << fs.c;
		}
		return str;
	}

    string zfill(const string_view& str, std::ptrdiff_t width)
    {
        std::ptrdiff_t len = (std::ptrdiff_t)str.size();

        if (len >= width)
        {
            return to_string(str);
        }

        std::ostringstream os;

        std::ptrdiff_t fill = width - len;

        os << fillstream{'0', fill} << str;
        std::string s = os.str();

        if (s[fill] == '+' || s[fill] == '-')
        {
            s[0] = s[fill];
            s[fill] = '0';
        }

        return s;
    }

    string ljust(const string_view& str, std::ptrdiff_t width)
    {
        std::ptrdiff_t len = static_cast<std::ptrdiff_t>(str.size());
        std::ostringstream os;
        os << str << fillstream{' ', width - len};
        return os.str();
    }

    string rjust(const string_view& str, std::ptrdiff_t width)
    {
        std::ptrdiff_t len = static_cast<std::ptrdiff_t>(str.size());
        std::ostringstream os;
        os << fillstream{' ', width - len} << str;
        return os.str();
    }

    string center(const string_view& str, std::ptrdiff_t width)
    {
        std::ptrdiff_t len = static_cast<std::ptrdiff_t>(str.size());
        std::ptrdiff_t marg, left;
        std::ostringstream os;
        marg = width - len;
        left = marg / 2 + (marg & width & 1);
        os << fillstream{' ', left} << str << fillstream{' ', marg - left};
        return os.str();
    }

    string_view slice(const string_view& str, std::ptrdiff_t start, std::ptrdiff_t end)
    {
        ADJUST_INDICES(start, end, (std::ptrdiff_t) str.size());
        if (start >= end)
            return string_view(nullptr, 0);
        return str.substr(start, end - start);
    }

    template <class S>
    std::ptrdiff_t find(const string_view& str, const S& sub, std::ptrdiff_t start, std::ptrdiff_t end)
    {
        ADJUST_INDICES(start, end, (std::ptrdiff_t) detail::size(str));

        if (str.size() == 0 && detail::size(sub) == 0)
        {
        	return 0;
        }

        string_view::size_type result = str.find(sub, start);

        // If we cannot find the string, or if the end-point of our found substring is past
        // the allowed end limit, return that it can't be found.
        if(result == string_view::npos || (result + detail::size(sub) > (std::string::size_type) end))
        {
            return -1;
        }
        return (ptrdiff_t) result;
    }

    template <class S>
    std::ptrdiff_t index(const string_view& str, const S& sub, std::ptrdiff_t start, std::ptrdiff_t end)
    {
        return find(str, sub, start, end);
    }

    template <class S>
    std::ptrdiff_t rfind(const string_view& str, const S& sub, std::ptrdiff_t start, std::ptrdiff_t end)
    {
        ADJUST_INDICES(start, end, (std::ptrdiff_t) str.size());

        string::size_type result = str.rfind(sub, end);

        if (result == std::string::npos || result < (string::size_type) start || (result + detail::size(sub) > (string::size_type) end))
        {
            return -1;
        }

        return (std::ptrdiff_t) result;
    }

    template <class S>
    std::ptrdiff_t rindex(const string_view& str, const S& sub, std::ptrdiff_t start, std::ptrdiff_t end)
    {
        return rfind(str, sub, start, end);
    }

    string expandtabs(const string_view& str, std::ptrdiff_t tabsize)
    {
    	std::ostringstream os;
        std::size_t len = str.size(), i = 0;
        std::ptrdiff_t j = 0;

        for (i = 0; i < len; ++i)
        {
            if (str[i] == '\t')
            {
                if (tabsize > 0)
                {
                    std::ptrdiff_t fillsize = tabsize - (j % tabsize);
                    j += fillsize;
                	os << fillstream{' ', fillsize};
                }
            }
            else
            {
                j++;
                if (str[i] == '\n' || str[i] == '\r')
                {
                    j = 0;
                }
                os << str[i];
            }
        }

        return os.str();
    }

    std::ptrdiff_t count(const string_view& str, const string_view& substr, std::ptrdiff_t start, std::ptrdiff_t end)
    {
        std::ptrdiff_t nummatches = 0;
        std::ptrdiff_t cursor = start;

        while (1) {
            cursor = find(str, substr, cursor, end);

            if (cursor < 0)
                break;

            cursor += (std::ptrdiff_t) detail::size(substr);
            nummatches += 1;
        }

        return nummatches;
    }

    template <class S, class T, class U>
    string replace(const S& str, const T& oldstr, const U& newstr, std::ptrdiff_t count)
    {
        std::ptrdiff_t sofar = 0;
        std::ptrdiff_t cursor = 0;
        string s(str);

        std::size_t oldlen = detail::size(oldstr), newlen = detail::size(newstr);

        cursor = find(s, oldstr, cursor);

        while (cursor != -1 && cursor <= (std::ptrdiff_t) s.size()) {
            if (count > -1 && sofar >= count) {
                break;
            }

            s.replace(cursor, oldlen, newstr);
            cursor += (std::ptrdiff_t)newlen;

            if (oldlen != 0) {
                cursor = find(s, oldstr, cursor);
            } else {
                ++cursor;
            }

            ++sofar;
        }

        return s;
    }

    std::vector<string_view> splitlines(const string_view& str, bool keepends)
    {
        std::vector<string_view> result;
        std::size_t len = str.size(), i, j, eol;

        string_view stv(str);
        for (i = j = 0; i < len;)
        {
            while (i < len && stv[i] != '\n' && stv[i] != '\r')
                i++;

            eol = i;
            if (i < len) {
                if (stv[i] == '\r' && i + 1 < len && stv[i + 1] == '\n') {
                    i += 2;
                } else {
                    i++;
                }
                if (keepends)
                    eol = i;
            }

            result.push_back(stv.substr(j, eol - j));
            j = i;
        }

        if (j < len)
        {
            result.push_back(stv.substr(j, len - j));
        }

        return result;
    }

    template <class S>
    string mul(const string_view& str, std::ptrdiff_t n)
    {
        // Early exits
        if (n <= 0)
            return "";

        if (n == 1)
        {
        	return pystring::to_string(str);
        }

        std::ostringstream os;
        for (int i = 0; i < n; ++i) {
            os << str;
        }
        return os.str();
    }

// namespace os
// {
// namespace path
// {
    
//     //////////////////////////////////////////////////////////////////////////////////////////////
//     ///
//     ///
//     /// These functions are C++ ports of the python2.6 versions of os.path,
//     /// and come from genericpath.py, ntpath.py, posixpath.py

//     /// Split a pathname into drive and path specifiers.
//     /// Returns drivespec, pathspec. Either part may be empty.
//     void splitdrive_nt(std::string & drivespec, std::string & pathspec,
//                        const std::string & p)
//     {
//         if(pystring::slice(p, 1, 2) == ":")
//         {
//             std::string path = p; // In case drivespec == p
//             drivespec = pystring::slice(path, 0, 2);
//             pathspec = pystring::slice(path, 2);
//         }
//         else
//         {
//             drivespec = "";
//             pathspec = p;
//         }
//     }

//     // On Posix, drive is always empty
//     void splitdrive_posix(std::string & drivespec, std::string & pathspec,
//                           const std::string & path)
//     {
//         drivespec = "";
//         pathspec = path;
//     }

//     void splitdrive(std::string & drivespec, std::string & pathspec,
//                     const std::string & path)
//     {
// #ifdef WINDOWS
//         return splitdrive_nt(drivespec, pathspec, path);
// #else
//         return splitdrive_posix(drivespec, pathspec, path);
// #endif
//     }

//     //////////////////////////////////////////////////////////////////////////////////////////////
//     ///
//     ///

//     // Test whether a path is absolute
//     // In windows, if the character to the right of the colon
//     // is a forward or backslash it's absolute.
//     bool isabs_nt(const std::string & path)
//     {
//         std::string drivespec, pathspec;
//         splitdrive_nt(drivespec, pathspec, path);
//         if(pathspec.empty()) return false;
//         return ((pathspec[0] == '/') || (pathspec[0] == '\\'));
//     }

//     bool isabs_posix(const std::string & s)
//     {
//         return pystring::startswith(s, "/"s);
//     }

//     bool isabs(const std::string & path)
//     {
// #ifdef WINDOWS
//         return isabs_nt(path);
// #else
//         return isabs_posix(path);
// #endif
//     }


//     //////////////////////////////////////////////////////////////////////////////////////////////
//     ///
//     ///
    
//     std::string abspath_nt(const std::string & path, const std::string & cwd)
//     {
//         std::string p = path;
//         if(!isabs_nt(p)) p = join_nt(cwd, p);
//         return normpath_nt(p);
//     }
    
//     std::string abspath_posix(const std::string & path, const std::string & cwd)
//     {
//         std::string p = path;
//         if(!isabs_posix(p)) p = join_posix(cwd, p);
//         return normpath_posix(p);
//     }
    
//     std::string abspath(const std::string & path, const std::string & cwd)
//     {
// #ifdef WINDOWS
//         return abspath_nt(path, cwd);
// #else
//         return abspath_posix(path, cwd);
// #endif
//     }
    

//     //////////////////////////////////////////////////////////////////////////////////////////////
//     ///
//     ///

//     std::string join_nt(const std::vector< std::string > & paths)
//     {
//         if(paths.empty()) return "";
//         if(paths.size() == 1) return paths[0];
        
//         std::string path = paths[0];
        
//         for(std::size_t i = 1; i < paths.size(); ++i)
//         {
//             std::string b = paths[i];
            
//             bool b_nts = false;
//             if(path.empty())
//             {
//                 b_nts = true;
//             }
//             else if(isabs_nt(b))
//             {
//                 // This probably wipes out path so far.  However, it's more
//                 // complicated if path begins with a drive letter:
//                 //     1. join('c:', '/a') == 'c:/a'
//                 //     2. join('c:/', '/a') == 'c:/a'
//                 // But
//                 //     3. join('c:/a', '/b') == '/b'
//                 //     4. join('c:', 'd:/') = 'd:/'
//                 //     5. join('c:/', 'd:/') = 'd:/'
                
//                 if( (pystring::slice(path, 1, 2) != ":") ||
//                     (pystring::slice(b, 1, 2) == ":") )
//                 {
//                     // Path doesnt start with a drive letter
//                     b_nts = true;
//                 }
//                 // Else path has a drive letter, and b doesn't but is absolute.
//                 else if((path.size()>3) || 
//                         ((path.size()==3) && !pystring::endswith(path, "/"s) && !pystring::endswith(path, "\\"s)))
//                 {
//                     b_nts = true;
//                 }
//             }
            
//             if(b_nts)
//             {
//                 path = b;
//             }
//             else
//             {
//                 // Join, and ensure there's a separator.
//                 // assert len(path) > 0
//                 if( pystring::endswith(path, "/"s) || pystring::endswith(path, "\\"s))
//                 {
//                     if(pystring::startswith(b,"/"s) || pystring::startswith(b,"\\"s))
//                     {
//                         path += pystring::slice(b, 1);
//                     }
//                     else
//                     {
//                         path += b;
//                     }
//                 }
//                 else if(pystring::endswith(path, ":"))
//                 {
//                     path += b;
//                 }
//                 else if(!b.empty())
//                 {
//                     if(pystring::startswith(b,"/") || pystring::startswith(b,"\\"))
//                     {
//                         path += b;
//                     }
//                     else
//                     {
//                         path += "\\" + b;
//                     }
//                 }
//                 else
//                 {
//                     // path is not empty and does not end with a backslash,
//                     // but b is empty; since, e.g., split('a/') produces
//                     // ('a', ''), it's best if join() adds a backslash in
//                     // this case.
//                     path += "\\";
//                 }
//             }
//         }
        
//         return path;
//     }
    
//     // Join two or more pathname components, inserting "\\" as needed.
//     std::string join_nt(const std::string & a, const std::string & b)
//     {
//         std::vector< std::string > paths(2);
//         paths[0] = a;
//         paths[1] = b;
//         return join_nt(paths);
//     }

//     // Join pathnames.
//     // If any component is an absolute path, all previous path components
//     // will be discarded.
//     // Ignore the previous parts if a part is absolute.
//     // Insert a '/' unless the first part is empty or already ends in '/'.

//     std::string join_posix(const std::vector< std::string > & paths)
//     {
//         if(paths.empty()) return "";
//         if(paths.size() == 1) return paths[0];
        
//         std::string path = paths[0];
        
//         for(std::size_t i = 1; i < paths.size(); ++i)
//         {
//             std::string b = paths[i];
//             if(pystring::startswith(b, "/"s))
//             {
//                 path = b;
//             }
//             else if(path.empty() || pystring::endswith(path, "/"s))
//             {
//                 path += b;
//             }
//             else
//             {
//                 path += "/" + b;
//             }
//         }
        
//         return path;
//     }

//     std::string join_posix(const std::string & a, const std::string & b)
//     {
//         std::vector< std::string > paths(2);
//         paths[0] = a;
//         paths[1] = b;
//         return join_posix(paths);
//     }
    
//     std::string join(const std::string & path1, const std::string & path2)
//     {
// #ifdef WINDOWS
//         return join_nt(path1, path2);
// #else
//         return join_posix(path1, path2);
// #endif
//     }


//     std::string join(const std::vector< std::string > & paths)
//     {
// #ifdef WINDOWS
//         return join_nt(paths);
// #else
//         return join_posix(paths);
// #endif
//     }
    
//     //////////////////////////////////////////////////////////////////////////////////////////////
//     ///
//     ///

        
//     // Split a pathname.
//     // Return (head, tail) where tail is everything after the final slash.
//     // Either part may be empty

//     void split_nt(std::string & head, std::string & tail, const std::string & path)
//     {
//         std::string d, p;
//         splitdrive_nt(d, p, path);
        
//         // set i to index beyond p's last slash
//         std::ptrdiff_t i = (std::ptrdiff_t)p.size();
        
//         while(i>0 && (p[i-1] != '\\') && (p[i-1] != '/'))
//         {
//             i = i - 1;
//         }

//         head = pystring::slice(p,0,i);
//         tail = pystring::slice(p,i); // now tail has no slashes
        
//         // remove trailing slashes from head, unless it's all slashes
//         std::string head2 = head;
//         while(!head2.empty() && ((pystring::slice(head2,-1) == "/") ||
//                                  (pystring::slice(head2,-1) == "\\")))
//         {
//             head2 = pystring::slice(head,0,-1);
//         }
        
//         if(!head2.empty()) head = head2;
//         head = d + head;
//     }


//     // Split a path in head (everything up to the last '/') and tail (the
//     // rest).  If the path ends in '/', tail will be empty.  If there is no
//     // '/' in the path, head  will be empty.
//     // Trailing '/'es are stripped from head unless it is the root.

//     void split_posix(std::string & head, std::string & tail, const std::string & p)
//     {
//         std::ptrdiff_t i = pystring::rfind(p, "/"s) + 1;
        
//         head = pystring::slice(p,0,i);
//         tail = pystring::slice(p,i);
        
//         if(!head.empty() && (head != pystring::mul("/"s, (std::ptrdiff_t) head.size())))
//         {
//             head = pystring::rstrip(head, "/"s);
//         }
//     }

//     void split(std::string & head, std::string & tail, const std::string & path)
//     {
// #ifdef WINDOWS
//         return split_nt(head, tail, path);
// #else
//         return split_posix(head, tail, path);
// #endif
//     }


//     //////////////////////////////////////////////////////////////////////////////////////////////
//     ///
//     ///

//     std::string basename_nt(const std::string & path)
//     {
//         std::string head, tail;
//         split_nt(head, tail, path);
//         return tail;
//     }

//     std::string basename_posix(const std::string & path)
//     {
//         std::string head, tail;
//         split_posix(head, tail, path);
//         return tail;
//     }

//     std::string basename(const std::string & path)
//     {
// #ifdef WINDOWS
//         return basename_nt(path);
// #else
//         return basename_posix(path);
// #endif
//     }

//     std::string dirname_nt(const std::string & path)
//     {
//         std::string head, tail;
//         split_nt(head, tail, path);
//         return head;
//     }
    
//     std::string dirname_posix(const std::string & path)
//     {
//         std::string head, tail;
//         split_posix(head, tail, path);
//         return head;
//     }
    
//     std::string dirname(const std::string & path)
//     {
// #ifdef WINDOWS
//         return dirname_nt(path);
// #else
//         return dirname_posix(path);
// #endif
//     }


//     //////////////////////////////////////////////////////////////////////////////////////////////
//     ///
//     ///

//     // Normalize a path, e.g. A//B, A/./B and A/foo/../B all become A\B.
//     std::string normpath_nt(const std::string & p)
//     {
//         std::string path = p;
//         path = pystring::replace(path, "/"s, "\\"s);
        
//         std::string prefix;
//         splitdrive_nt(prefix, path, path);
        
//         // We need to be careful here. If the prefix is empty, and the path starts
//         // with a backslash, it could either be an absolute path on the current
//         // drive (\dir1\dir2\file) or a UNC filename (\\server\mount\dir1\file). It
//         // is therefore imperative NOT to collapse multiple backslashes blindly in
//         // that case.
//         // The code below preserves multiple backslashes when there is no drive
//         // letter. This means that the invalid filename \\\a\b is preserved
//         // unchanged, where a\\\b is normalised to a\b. It's not clear that there
//         // is any better behaviour for such edge cases.
        
//         if(prefix.empty())
//         {
//             // No drive letter - preserve initial backslashes
//             while(pystring::slice(path,0,1) == "\\")
//             {
//                 prefix = prefix + "\\";
//                 path = pystring::slice(path, 1);
//             }
//         }
//         else
//         {
//             // We have a drive letter - collapse initial backslashes
//             if(pystring::startswith(path, "\\"s))
//             {
//                 prefix = prefix + "\\";
//                 path = pystring::lstrip(path, "\\"s);
//             }
//         }
        
//         std::vector<std::string> comps = pystring::split(path, "\\"s);
        
//         std::ptrdiff_t i = 0;
        
//         while(i < (std::ptrdiff_t) comps.size())
//         {
//             if(comps[i].empty() || comps[i] == ".")
//             {
//                 comps.erase(comps.begin()+i);
//             }
//             else if(comps[i] == "..")
//             {
//                 if(i>0 && comps[i-1] != "..")
//                 {
//                     comps.erase(comps.begin()+i-1, comps.begin()+i+1);
//                     i -= 1;
//                 }
//                 else if(i == 0 && pystring::endswith(prefix, "\\"s))
//                 {
//                     comps.erase(comps.begin()+i);
//                 }
//                 else
//                 {
//                     i += 1;
//                 }
//             }
//             else
//             {
//                 i += 1;
//             }
//         }
        
//         // If the path is now empty, substitute '.'
//         if(prefix.empty() && comps.empty())
//         {
//             comps.push_back(".");
//         }
        
//         return prefix + pystring::join("\\"s, comps);
//     }

//     // Normalize a path, e.g. A//B, A/./B and A/foo/../B all become A/B.
//     // It should be understood that this may change the meaning of the path
//     // if it contains symbolic links!
//     // Normalize path, eliminating double slashes, etc.

//     std::string normpath_posix(const std::string & p)
//     {
//         if(p.empty()) return ".";
        
//         std::string path = p;
        
//         std::ptrdiff_t initial_slashes = pystring::startswith(path,"/") ? 1 : 0;
        
//         // POSIX allows one or two initial slashes, but treats three or more
//         // as single slash.
        
//         if (initial_slashes && pystring::startswith(path,"//")
//             && !pystring::startswith(path,"///"))
//             initial_slashes = 2;
        
//         std::vector<std::string> new_comps;
//         std::vector<std::string> comps = pystring::split(path, "/"s);
        
//         for(std::size_t i=0; i<comps.size(); ++i)
//         {
//             std::string comp = comps[i];
//             if(comp.empty() || comp == ".")
//                 continue;
            
//             if( (comp != "..") || ((initial_slashes == 0) && new_comps.empty()) ||
//                 (!new_comps.empty() && new_comps[new_comps.size()-1] == ".."))
//             {
//                 new_comps.push_back(comp);
//             }
//             else if (!new_comps.empty())
//             {
//                 new_comps.pop_back();
//             }
//         }
        
//         path = pystring::join("/"s, new_comps);
        
//         if (initial_slashes > 0)
//             path = pystring::mul("/"s,initial_slashes) + path;
        
//         if(path.empty()) return ".";
//         return path;
//     }
    
//     std::string normpath(const std::string & path)
//     {
// #ifdef WINDOWS
//         return normpath_nt(path);
// #else
//         return normpath_posix(path);
// #endif
//     }

//     //////////////////////////////////////////////////////////////////////////////////////////////
//     ///
//     ///

//     // Split the extension from a pathname.
//     // Extension is everything from the last dot to the end, ignoring
//     // leading dots.  Returns "(root, ext)"; ext may be empty.
//     // It is always true that root + ext == p

//     void splitext_generic(std::string & root, std::string & ext,
//                           const std::string & p,
//                           const std::string & sep,
//                           const std::string & altsep,
//                           const std::string & extsep)
//     {
//         std::ptrdiff_t sepIndex = pystring::rfind(p, sep);
//         if(!altsep.empty())
//         {
//             std::ptrdiff_t altsepIndex = pystring::rfind(p, altsep);
//             sepIndex = std::max(sepIndex, altsepIndex);
//         }

//         std::ptrdiff_t dotIndex = pystring::rfind(p, extsep);
//         if(dotIndex > sepIndex)
//         {
//             // Skip all leading dots
//             std::ptrdiff_t filenameIndex = sepIndex + 1;

//             while(filenameIndex < dotIndex)
//             {
//                 if(pystring::slice(p,filenameIndex) != extsep)
//                 {
//                     root = pystring::slice(p, 0, dotIndex);
//                     ext = pystring::slice(p, dotIndex);
//                     return;
//                 }

//                 filenameIndex += 1;
//             }
//         }

//         root = p;
//         ext = "";
//     }

//     void splitext_nt(std::string & root, std::string & ext, const std::string & path)
//     {
//         return splitext_generic(root, ext, path,
//                                 "\\", "/", ".");
//     }

//     void splitext_posix(std::string & root, std::string & ext, const std::string & path)
//     {
//         return splitext_generic(root, ext, path,
//                                 "/", "", ".");
//     }

//     void splitext(std::string & root, std::string & ext, const std::string & path)
//     {
// #ifdef WINDOWS
//         return splitext_nt(root, ext, path);
// #else
//         return splitext_posix(root, ext, path);
// #endif
//     }

// } // namespace path
// } // namespace os


}//namespace pystring


#endif