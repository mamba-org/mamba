// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <spdlog/spdlog.h>

#include "mamba/util/string.hpp"

#include "compression.hpp"

namespace mamba::download
{
    /*********************
     * CompressionStream *
     *********************/

    CompressionStream::CompressionStream(writer&& func)
        : m_writer(std::move(func))
    {
    }

    size_t CompressionStream::write(char* in, size_t size)
    {
        return write_impl(in, size);
    }

    size_t CompressionStream::invoke_writer(char* in, size_t size)
    {
        return m_writer(in, size);
    }

    /*************************
     * ZstdCompressionStream *
     *************************/

    class ZstdCompressionStream : public CompressionStream
    {
    public:

        using base_type = CompressionStream;
        using writer = base_type::writer;

        explicit ZstdCompressionStream(writer&& func);
        virtual ~ZstdCompressionStream();

    private:

        size_t write_impl(char* in, size_t size) override;

        static constexpr size_t BUFFER_SIZE = 256 * 1024;

        ZSTD_DCtx* p_stream;
        char m_buffer[BUFFER_SIZE];
    };

    ZstdCompressionStream::ZstdCompressionStream(writer&& func)
        : base_type(std::move(func))
        , p_stream(ZSTD_createDCtx())
    {
        ZSTD_initDStream(p_stream);
    }

    ZstdCompressionStream::~ZstdCompressionStream()
    {
        ZSTD_freeDCtx(p_stream);
    }

    size_t ZstdCompressionStream::write_impl(char* in, size_t size)
    {
        ZSTD_inBuffer input = { in, size, 0 };
        ZSTD_outBuffer output = { m_buffer, BUFFER_SIZE, 0 };

        while (input.pos < input.size)
        {
            auto ret = ZSTD_decompressStream(p_stream, &output, &input);
            if (ZSTD_isError(ret))
            {
                // This is temporary...
                // TODO Remove dependency on spdlog after deciding on what to do with logging
                spdlog::error("ZSTD decompression error: {}", ZSTD_getErrorName(ret));
                return size + 1;
            }
            if (output.pos > 0)
            {
                size_t wcb_res = base_type::invoke_writer(m_buffer, output.pos);
                if (wcb_res != output.pos)
                {
                    return size + 1;
                }
                output.pos = 0;
            }
        }
        return size;
    }

    /**************************
     * Bzip2CompressionStream *
     **************************/

    class Bzip2CompressionStream : public CompressionStream
    {
    public:

        using base_type = CompressionStream;
        using writer = base_type::writer;

        explicit Bzip2CompressionStream(writer&& func);
        virtual ~Bzip2CompressionStream();

    private:

        size_t write_impl(char* in, size_t size) override;

        static constexpr size_t BUFFER_SIZE = 256 * 1024;

        bz_stream m_stream;
        char m_buffer[BUFFER_SIZE];
    };

    Bzip2CompressionStream::Bzip2CompressionStream(writer&& func)
        : base_type(std::move(func))
    {
        m_stream.bzalloc = nullptr;
        m_stream.bzfree = nullptr;
        m_stream.opaque = nullptr;

        int error = BZ2_bzDecompressInit(&m_stream, 0, false);
        if (error != BZ_OK)
        {
            throw std::runtime_error("BZ2_bzDecompressInit failed");
        }
    }

    Bzip2CompressionStream::~Bzip2CompressionStream()
    {
        BZ2_bzDecompressEnd(&m_stream);
    }

    size_t Bzip2CompressionStream::write_impl(char* in, size_t size)
    {
        m_stream.next_in = in;
        m_stream.avail_in = static_cast<unsigned int>(size);

        while (m_stream.avail_in > 0)
        {
            m_stream.next_out = m_buffer;
            m_stream.avail_out = Bzip2CompressionStream::BUFFER_SIZE;

            int ret = BZ2_bzDecompress(&m_stream);
            if (ret != BZ_OK && ret != BZ_STREAM_END)
            {
                // This is temporary...
                // TODO Remove dependency on spdlog after deciding on what to do with logging
                spdlog::error("Bzip2 decompression error: {}", ret);
                return size + 1;
            }

            size_t wcb_res = base_type::invoke_writer(m_buffer, BUFFER_SIZE - m_stream.avail_out);
            if (wcb_res != BUFFER_SIZE - m_stream.avail_out)
            {
                return size + 1;
            }
        }
        return size;
    }

    /***********************
     * NoCompressionStream *
     ***********************/

    class NoCompressionStream : public CompressionStream
    {
    public:

        using base_type = CompressionStream;
        using writer = base_type::writer;

        explicit NoCompressionStream(writer&& func);
        virtual ~NoCompressionStream() = default;

    private:

        size_t write_impl(char* in, size_t size) override;
    };

    NoCompressionStream::NoCompressionStream(writer&& func)
        : base_type(std::move(func))
    {
    }

    size_t NoCompressionStream::write_impl(char* in, size_t size)
    {
        return base_type::invoke_writer(in, size);
    }

    std::unique_ptr<CompressionStream> make_compression_stream(
        const std::string& url,
        bool is_repodata_zst_from_oci_reg,
        CompressionStream::writer&& func
    )
    {
        // In the case of fetching from an OCI registry,
        // the url doesn't end with `.json.zst` extension.
        // Compressed repodata is rather handled internally
        // in OCIMirror implementation, and is reflected
        // by `is_repodata_zst_from_oci_reg`
        if (util::ends_with(url, ".json.zst") || is_repodata_zst_from_oci_reg)
        {
            return std::make_unique<ZstdCompressionStream>(std::move(func));
        }
        else if (util::ends_with(url, "json.bz2"))
        {
            return std::make_unique<Bzip2CompressionStream>(std::move(func));
        }
        else
        {
            return std::make_unique<NoCompressionStream>(std::move(func));
        }
    }

    // TODO: remove XXXStreams and rename XXXCompressionStream into XXXStream

    /*************
     * Old stuff *
     *************/

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
