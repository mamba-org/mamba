// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_COMPRESSION_HPP
#define MAMBA_COMPRESSION_HPP

#include <bzlib.h>
#include <zstd.h>

#include "curl.hpp"

namespace mamba
{
    struct ZstdStream
    {
        static constexpr size_t BUFFER_SIZE = 256000;
        ZstdStream(curl_write_callback lwrite_callback, void* write_callback_data)
            : stream(ZSTD_createDCtx())
            , m_write_callback(lwrite_callback)
            , m_write_callback_data(write_callback_data)
        {
            ZSTD_initDStream(stream);
        }

        ~ZstdStream()
        {
            ZSTD_freeDCtx(stream);
        }

        size_t write(char* in, size_t size);

        static size_t write_callback(char* ptr, size_t size, size_t nmemb, void* self)
        {
            return static_cast<ZstdStream*>(self)->write(ptr, size * nmemb);
        }

        ZSTD_DCtx* stream;
        char buffer[BUFFER_SIZE];

        // original curl callback
        curl_write_callback m_write_callback;
        void* m_write_callback_data;
    };

    struct Bzip2Stream
    {
        static constexpr size_t BUFFER_SIZE = 256000;

        Bzip2Stream(curl_write_callback lwrite_callback, void* write_callback_data)
            : m_write_callback(lwrite_callback)
            , m_write_callback_data(write_callback_data)
        {
            m_stream.bzalloc = nullptr;
            m_stream.bzfree = nullptr;
            m_stream.opaque = nullptr;

            error = BZ2_bzDecompressInit(&m_stream, 0, false);
            if (error != BZ_OK)
            {
                throw std::runtime_error("BZ2_bzDecompressInit failed");
            }
        }

        size_t write(char* in, size_t size);

        static size_t write_callback(char* ptr, size_t size, size_t nmemb, void* self)
        {
            return static_cast<Bzip2Stream*>(self)->write(ptr, size * nmemb);
        }

        ~Bzip2Stream()
        {
            BZ2_bzDecompressEnd(&m_stream);
        }

        int error;
        bz_stream m_stream;
        char buffer[BUFFER_SIZE];

        // original curl callback
        curl_write_callback m_write_callback;
        void* m_write_callback_data;
    };

    inline size_t get_zstd_buff_out_size()
    {
        return ZSTD_DStreamOutSize();
    }
}  // namespace mamba

#endif  // MAMBA_COMPRESSION_HPP
