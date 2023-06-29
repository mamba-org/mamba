// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <spdlog/spdlog.h>

#include "compression.hpp"

namespace mamba
{
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
