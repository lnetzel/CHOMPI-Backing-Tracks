#pragma once

#include <stddef.h>
#include <stdint.h>

namespace chompi
{

using BackingReadAt = bool (*)(void *context,
                               uint32_t offset,
                               uint8_t *destination,
                               size_t length);

struct BackingWavInfo
{
    uint32_t data_offset;
    uint32_t data_bytes;
    uint32_t frame_count;
    uint16_t channels;
    uint16_t block_align;
};

inline uint16_t BackingU16LE(const uint8_t *bytes)
{
    return static_cast<uint16_t>(bytes[0])
           | static_cast<uint16_t>(bytes[1] << 8);
}

inline uint32_t BackingU32LE(const uint8_t *bytes)
{
    return static_cast<uint32_t>(bytes[0])
           | (static_cast<uint32_t>(bytes[1]) << 8)
           | (static_cast<uint32_t>(bytes[2]) << 16)
           | (static_cast<uint32_t>(bytes[3]) << 24);
}

inline bool BackingFourCC(const uint8_t *bytes, const char *fourcc)
{
    return bytes[0] == static_cast<uint8_t>(fourcc[0])
           && bytes[1] == static_cast<uint8_t>(fourcc[1])
           && bytes[2] == static_cast<uint8_t>(fourcc[2])
           && bytes[3] == static_cast<uint8_t>(fourcc[3]);
}

inline bool ParseBackingWav(BackingReadAt read_at,
                           void *context,
                           uint32_t file_bytes,
                           uint32_t first_downbeat_ms,
                           BackingWavInfo &info)
{
    uint8_t header[12];
    if(read_at == NULL || file_bytes < sizeof(header)
       || !read_at(context, 0, header, sizeof(header))
       || !BackingFourCC(header, "RIFF")
       || !BackingFourCC(header + 8, "WAVE"))
        return false;
    const uint32_t riff_size = BackingU32LE(header + 4);
    if(riff_size < 4 || riff_size > UINT32_MAX - 8u)
        return false;
    const uint32_t riff_end = riff_size + 8u;
    if(riff_end > file_bytes)
        return false;

    bool have_format = false;
    bool have_data = false;
    uint16_t channels = 0;
    uint16_t block_align = 0;
    uint32_t data_offset = 0;
    uint32_t data_bytes = 0;
    uint32_t offset = 12;
    while(offset < riff_end)
    {
        uint8_t chunk[8];
        if(riff_end - offset < sizeof(chunk)
           || !read_at(context, offset, chunk, sizeof(chunk)))
            return false;
        const uint32_t chunk_bytes = BackingU32LE(chunk + 4);
        const uint32_t body = offset + 8u;
        const uint32_t padded_bytes = chunk_bytes + (chunk_bytes & 1u);
        if(padded_bytes < chunk_bytes || padded_bytes > riff_end - body)
            return false;

        if(BackingFourCC(chunk, "fmt "))
        {
            uint8_t format[16];
            if(have_format || chunk_bytes < sizeof(format)
               || !read_at(context, body, format, sizeof(format)))
                return false;
            const uint16_t encoding = BackingU16LE(format);
            channels = BackingU16LE(format + 2);
            const uint32_t sample_rate = BackingU32LE(format + 4);
            const uint32_t byte_rate = BackingU32LE(format + 8);
            block_align = BackingU16LE(format + 12);
            const uint16_t bits_per_sample = BackingU16LE(format + 14);
            const uint16_t expected_align
                = static_cast<uint16_t>(channels * 2u);
            if(encoding != 1 || (channels != 1 && channels != 2)
               || sample_rate != 48000 || bits_per_sample != 16
               || block_align != expected_align
               || byte_rate != sample_rate * block_align)
                return false;
            have_format = true;
        }
        else if(BackingFourCC(chunk, "data"))
        {
            if(have_data || chunk_bytes == 0)
                return false;
            data_offset = body;
            data_bytes = chunk_bytes;
            have_data = true;
        }
        offset = body + padded_bytes;
    }

    if(offset != riff_end || !have_format || !have_data
       || data_bytes % block_align != 0)
        return false;
    const uint32_t frames = data_bytes / block_align;
    const uint64_t downbeat_frame
        = static_cast<uint64_t>(first_downbeat_ms) * 48u;
    if(frames == 0 || downbeat_frame >= frames)
        return false;

    info.data_offset = data_offset;
    info.data_bytes = data_bytes;
    info.frame_count = frames;
    info.channels = channels;
    info.block_align = block_align;
    return true;
}

} // namespace chompi