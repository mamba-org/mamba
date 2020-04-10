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

#ifndef PYSTRING_UTILS
#define PYSTRING_UTILS

#ifndef PYSTRING_DEFAULT_STRING_VIEW
#include "string_view_lite.hpp"
namespace pystring
{
	using namespace nonstd;
	using string = std::string;
}
#endif

namespace pystring
{

namespace detail
{
	template <class S>
	auto size(const S& s)
	{
	    return s.size();
	}

	template <class T, std::size_t N>
	auto size(const T (&str)[N])
	{
	    return std::strlen(str);
	}

	struct char_substr
	{
	    const char* pos;
	    const char* end;

	    operator std::string()
	    {
	        return {pos, end};
	    }
	};

	auto size(const char_substr& str)
	{
	    return std::distance(str.pos, str.end);
	}

	template <class T>
	bool operator==(const char_substr& lhs, const T& rhs)
	{
	    return std::equal(lhs.pos, lhs.end, std::begin(rhs)) && size(lhs) == size(rhs);
	}

	template <class S>
	auto substr(S& str, std::size_t pos, std::size_t len)
	{
	    return str.substr(pos, len);
	}

	template <class T, std::size_t N>
	auto substr(T (&str)[N], std::size_t pos, std::size_t len)
	{
	    return char_substr{str + pos, str + pos + len};
	}

	using std::end;
	using std::begin;

	template <class C, std::size_t N>
	auto end(const C (&c)[N])
	{
		return &c[0] + std::strlen(c);
	}

    struct fast_end_t {};

    template <class C>
    auto fast_end(C& c)
    {
        return c.end();
    }

    template <class T, std::size_t N>
    auto fast_end(T (&)[N])
    {
        return fast_end_t();
    }

    template <class It>
    bool operator==(It it, fast_end_t)
    {
        return (*it == '\0');
    }

    template <class It>
    bool operator!=(It it, fast_end_t)
    {
        return !(it == fast_end_t());
    }

    const char* c_str(const std::string& str)
    {
        return str.c_str();
    }

    template <std::size_t N>
    const char* c_str(const char(&str)[N])
    {
        return str;
    }


} // ns detail
}

#endif