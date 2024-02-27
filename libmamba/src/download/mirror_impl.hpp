// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_DL_MIRROR_IMPL_HPP
#define MAMBA_DL_MIRROR_IMPL_HPP

#include "mamba/download/mirror.hpp"

namespace mamba::download
{
    // TODO: This class will be renamed FileMirror when
    // other mirrors have been plugged. It is used
    // for everything to ensure a smooth transition
    class PassThroughMirror : public Mirror
    {
    public:

        PassThroughMirror();

        static MirrorID make_id();

    private:

        using request_generator_list = Mirror::request_generator_list;
        request_generator_list get_request_generators_impl() const override;
    };

    class HTTPMirror : public Mirror
    {
    public:

        explicit HTTPMirror(std::string url);

        static MirrorID make_id(std::string url);

    private:

        using request_generator_list = Mirror::request_generator_list;
        request_generator_list get_request_generators_impl() const override;

        std::string m_url;
    };
}

#endif
