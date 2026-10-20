#include "BackingSet.h"
#include "BackingWav.h"

#include <cassert>
#include <cstdio>
#include <cstring>

static const char *kValidSet
    = "{\"version\":1,\"songs\":[{\"file\":\"TRACKS/a.wav\","
      "\"bpm\":98.5,\"meter\":[6,8],\"first_downbeat_ms\":0,"
      "\"advance\":\"wait\"}]}";

static bool ReadMemory(void *context,
                       uint32_t offset,
                       uint8_t *destination,
                       size_t length)
{
    const auto *file = static_cast<const uint8_t *>(context);
    if(offset > 128 || length > 128 - offset)
        return false;
    std::memcpy(destination, file + offset, length);
    return true;
}

static bool ReadFile(void *context,
                     uint32_t offset,
                     uint8_t *destination,
                     size_t length)
{
    auto *file = static_cast<std::FILE *>(context);
    return std::fseek(file, static_cast<long>(offset), SEEK_SET) == 0
           && std::fread(destination, 1, length, file) == length;
}

static void PutU16(uint8_t *bytes, uint16_t value)
{
    bytes[0] = static_cast<uint8_t>(value);
    bytes[1] = static_cast<uint8_t>(value >> 8);
}

static void PutU32(uint8_t *bytes, uint32_t value)
{
    bytes[0] = static_cast<uint8_t>(value);
    bytes[1] = static_cast<uint8_t>(value >> 8);
    bytes[2] = static_cast<uint8_t>(value >> 16);
    bytes[3] = static_cast<uint8_t>(value >> 24);
}

static void PutFourCC(uint8_t *bytes, const char *value)
{
    std::memcpy(bytes, value, 4);
}

static void MakeWav(uint8_t *file, uint16_t channels)
{
    std::memset(file, 0, 128);
    PutFourCC(file, "RIFF");
    PutU32(file + 4, 52 + (channels == 2 ? 4 : 0));
    PutFourCC(file + 8, "WAVE");
    PutFourCC(file + 12, "JUNK");
    PutU32(file + 16, 1);
    file[20] = 0x55;
    file[21] = 0;
    PutFourCC(file + 22, "fmt ");
    PutU32(file + 26, 18);
    PutU16(file + 30, 1);
    PutU16(file + 32, channels);
    PutU32(file + 34, 48000);
    PutU32(file + 38, 48000 * channels * 2);
    PutU16(file + 42, channels * 2);
    PutU16(file + 44, 16);
    PutU16(file + 46, 0);
    PutFourCC(file + 48, "data");
    PutU32(file + 52, channels * 4);
}

int main(int argc, char **argv)
{
    chompi::BackingSet set = {};
    assert(chompi::ParseBackingSet(kValidSet, std::strlen(kValidSet), set));
    assert(set.count == 1);
    assert(std::strcmp(set.songs[0].file, "TRACKS/a.wav") == 0);
    assert(set.songs[0].bpm == 98.5f);
    assert(set.songs[0].meter_numerator == 6);
    assert(set.songs[0].meter_denominator == 8);
    assert(!set.songs[0].advance_auto);

    const char *duplicate
        = "{\"version\":1,\"version\":1,\"songs\":[{\"file\":\"a.wav\","
          "\"bpm\":120,\"meter\":[4,4],\"first_downbeat_ms\":0,"
          "\"advance\":\"auto\"}]}";
    assert(!chompi::ParseBackingSet(duplicate, std::strlen(duplicate), set));
    const char *traversal
        = "{\"version\":1,\"songs\":[{\"file\":\"../a.wav\","
          "\"bpm\":120,\"meter\":[4,4],\"first_downbeat_ms\":0,"
          "\"advance\":\"auto\"}]}";
    assert(!chompi::ParseBackingSet(traversal, std::strlen(traversal), set));
        const char *outside_tracks
                = "{\"version\":1,\"songs\":[{\"file\":\"audio/a.wav\","
                    "\"bpm\":120,\"meter\":[4,4],\"first_downbeat_ms\":0,"
                    "\"advance\":\"auto\"}]}";
        assert(!chompi::ParseBackingSet(outside_tracks,
                                                                     std::strlen(outside_tracks), set));

    uint8_t wav[128];
    MakeWav(wav, 1);
    chompi::BackingWavInfo info = {};
    assert(chompi::ParseBackingWav(ReadMemory, wav, 60, 0, info));
    assert(info.channels == 1 && info.frame_count == 2);
    MakeWav(wav, 2);
    assert(chompi::ParseBackingWav(ReadMemory, wav, 64, 0, info));
    assert(info.channels == 2 && info.frame_count == 2);
    assert(!chompi::ParseBackingWav(ReadMemory, wav, 64, 1, info));
    PutU32(wav + 4, 64);
    assert(!chompi::ParseBackingWav(ReadMemory, wav, 64, 0, info));
    PutU32(wav + 4, 56);
    assert(chompi::ParseBackingWav(ReadMemory, wav, 64, 0, info));

    for(int argument = 1; argument < argc; ++argument)
    {
        std::FILE *file = std::fopen(argv[argument], "rb");
        assert(file != NULL);
        assert(std::fseek(file, 0, SEEK_END) == 0);
        const long bytes = std::ftell(file);
        const bool valid = bytes > 0
                           && static_cast<uint64_t>(bytes) <= UINT32_MAX
                           && chompi::ParseBackingWav(
                               ReadFile, file, static_cast<uint32_t>(bytes),
                               0, info);
        std::fclose(file);
        std::printf("%s: %s\n", argv[argument], valid ? "valid" : "INVALID");
        if(!valid)
            return 1;
    }

    std::puts("backing_format_test: all assertions passed");
    return 0;
}