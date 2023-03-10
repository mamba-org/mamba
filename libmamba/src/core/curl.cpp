// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include "curl.hpp"

#include <spdlog/spdlog.h>

namespace mamba
{
    const std::pair<std::string_view, CURL_LOG_LEVEL> init_curl_ssl_session()
    {
        CURL* handle = curl_easy_init();
        if (handle)
        {
            std::pair<std::string_view, CURL_LOG_LEVEL> log;
            const struct curl_tlssessioninfo* info = NULL;
            CURLcode res = curl_easy_getinfo(handle, CURLINFO_TLS_SSL_PTR, &info);
            if (info && !res)
            {
                if (info->backend == CURLSSLBACKEND_OPENSSL)
                {
                    log = std::make_pair(std::string_view("Using OpenSSL backend"), CURL_LOG_LEVEL::INFO);
                }
                else if (info->backend == CURLSSLBACKEND_SECURETRANSPORT)
                {
                    log = std::make_pair(
                        std::string_view("Using macOS SecureTransport backend"),
                        CURL_LOG_LEVEL::INFO
                    );
                }
                else if (info->backend == CURLSSLBACKEND_SCHANNEL)
                {
                    log = std::make_pair(
                        std::string_view("Using Windows Schannel backend"),
                        CURL_LOG_LEVEL::INFO
                    );
                }
                else if (info->backend != CURLSSLBACKEND_NONE)
                {
                    log = std::make_pair(
                        std::string_view("Using an unknown (to mamba) SSL backend"),
                        CURL_LOG_LEVEL::INFO
                    );
                }
                else if (info->backend == CURLSSLBACKEND_NONE)
                {
                    log = std::make_pair(
                        std::string_view(
                            "No SSL backend found! Please check how your cURL library is configured."
                        ),
                        CURL_LOG_LEVEL::WARNING
                    );
                }
            }
            curl_easy_cleanup(handle);
            return log;
        }
        return std::make_pair(
            std::string_view("CURL handle was not properly intialized"),
            CURL_LOG_LEVEL::ERROR
        );
    }

    size_t ZstdStream::write(char* in, size_t size)
    {
        ZSTD_inBuffer input = { in, size, 0 };
        ZSTD_outBuffer output = { buffer, BUFFER_SIZE, 0 };

        while (input.pos < input.size)
        {
            auto ret = ZSTD_decompressStream(stream, &output, &input);
            if (ZSTD_isError(ret))
            {
                // This is temporary...
                // TODO Remove dependency on spdlog after deciding on what to do with logging
                spdlog::error("ZSTD decompression error: {}", ZSTD_getErrorName(ret));
                return size + 1;
            }
            if (output.pos > 0)
            {
                size_t wcb_res = m_write_callback(buffer, 1, output.pos, m_write_callback_data);
                if (wcb_res != output.pos)
                {
                    return size + 1;
                }
                output.pos = 0;
            }
        }
        return size;
    }

    size_t Bzip2Stream::write(char* in, size_t size)
    {
        bz_stream* stream = static_cast<bz_stream*>(m_write_callback_data);
        stream->next_in = in;
        stream->avail_in = static_cast<unsigned int>(size);

        while (stream->avail_in > 0)
        {
            stream->next_out = buffer;
            stream->avail_out = Bzip2Stream::BUFFER_SIZE;

            int ret = BZ2_bzDecompress(stream);
            if (ret != BZ_OK && ret != BZ_STREAM_END)
            {
                // This is temporary...
                // TODO Remove dependency on spdlog after deciding on what to do with logging
                spdlog::error("Bzip2 decompression error: {}", ret);
                return size + 1;
            }

            size_t wcb_res = m_write_callback(
                buffer,
                1,
                BUFFER_SIZE - stream->avail_out,
                m_write_callback_data
            );
            if (wcb_res != BUFFER_SIZE - stream->avail_out)
            {
                return size + 1;
            }
        }
        return size;
    }

}  // namespace mamba
