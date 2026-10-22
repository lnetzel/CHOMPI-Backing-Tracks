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
    assert(set.songs[0].advance == chompi::BackingAdvance::Wait);
    assert(set.songs[0].midi_clock);

    const char *midi_defaults
        = "{\"version\":1,\"songs\":["
          "{\"file\":\"TRACKS/a.wav\",\"bpm\":120,"
          "\"meter\":[4,4],\"first_downbeat_ms\":0,"
          "\"advance\":\"auto\",\"midi_clock\":false},"
          "{\"file\":\"TRACKS/a.wav\",\"bpm\":120,"
          "\"meter\":[4,4],\"first_downbeat_ms\":0,"
          "\"advance\":\"wait\",\"midi_clock\":true},"
          "{\"file\":\"TRACKS/b.wav\",\"bpm\":120,"
          "\"meter\":[4,4],\"first_downbeat_ms\":0,"
          "\"advance\":\"auto\"}]}";
    assert(chompi::ParseBackingSet(midi_defaults,
                                   std::strlen(midi_defaults), set));
    assert(set.count == 3);
    assert(!set.songs[0].midi_clock);
    assert(set.songs[1].midi_clock);
    assert(set.songs[2].midi_clock);

    const char *loop_mode
        = "{\"version\":1,\"songs\":[{\"file\":\"TRACKS/a.wav\","
          "\"bpm\":120,\"meter\":[4,4],\"first_downbeat_ms\":0,"
          "\"advance\":\"loop\"}]}";
    assert(chompi::ParseBackingSet(loop_mode, std::strlen(loop_mode), set));
    assert(set.songs[0].advance == chompi::BackingAdvance::Loop);

    char rewritten[512];
    std::memcpy(rewritten, midi_defaults, std::strlen(midi_defaults) + 1);
    assert(chompi::RewriteBackingAdvance(
        rewritten, std::strlen(rewritten), 1, chompi::BackingAdvance::Loop));
    assert(std::strstr(rewritten, "\"advance\":\"loop\"") != NULL);
    assert(std::strstr(rewritten, "\"midi_clock\":true") != NULL);
    chompi::BackingSet rewritten_set = {};
    assert(chompi::ParseBackingSet(rewritten, std::strlen(rewritten),
                                   rewritten_set));
    assert(rewritten_set.songs[1].advance == chompi::BackingAdvance::Loop);
    assert(rewritten_set.songs[0].advance == chompi::BackingAdvance::Auto);
    assert(!chompi::RewriteBackingAdvance(
        rewritten, std::strlen(rewritten), 3, chompi::BackingAdvance::Wait));
    assert(chompi::NextBackingAdvance(chompi::BackingAdvance::Wait)
           == chompi::BackingAdvance::Auto);
    assert(chompi::NextBackingAdvance(chompi::BackingAdvance::Auto)
           == chompi::BackingAdvance::Loop);
    assert(chompi::NextBackingAdvance(chompi::BackingAdvance::Loop)
           == chompi::BackingAdvance::Wait);
    const chompi::BackingAdvanceDecision wait_decision
        = chompi::BackingAdvanceAtEof(chompi::BackingAdvance::Wait, 0, 2);
    assert(wait_decision.selected_song == 1 && !wait_decision.begin_load);
    const chompi::BackingAdvanceDecision auto_decision
        = chompi::BackingAdvanceAtEof(chompi::BackingAdvance::Auto, 0, 2);
    assert(auto_decision.selected_song == 1 && auto_decision.begin_load);
    const chompi::BackingAdvanceDecision loop_decision
        = chompi::BackingAdvanceAtEof(chompi::BackingAdvance::Loop, 1, 2);
    assert(loop_decision.selected_song == 1 && loop_decision.begin_load);
    const chompi::BackingAdvanceDecision final_auto_decision
        = chompi::BackingAdvanceAtEof(chompi::BackingAdvance::Auto, 1, 2);
    assert(final_auto_decision.selected_song == 1
           && !final_auto_decision.begin_load);
    assert(chompi::BackingAdvanceAtEof(chompi::BackingAdvance::Wait, 2, 2)
               .selected_song
           == -1);

    const char *duplicate_midi_clock
        = "{\"version\":1,\"songs\":[{\"file\":\"TRACKS/a.wav\","
          "\"bpm\":120,\"meter\":[4,4],\"first_downbeat_ms\":0,"
          "\"advance\":\"auto\",\"midi_clock\":true,"
          "\"midi_clock\":false}]}";
    assert(!chompi::ParseBackingSet(duplicate_midi_clock,
                                    std::strlen(duplicate_midi_clock), set));
    const char *invalid_midi_clock[] = {
        "null", "0", "\"true\"", "[]", "{}"};
    for(const char *value : invalid_midi_clock)
    {
        char invalid[256];
        const int length = std::snprintf(
            invalid, sizeof(invalid),
            "{\"version\":1,\"songs\":[{\"file\":\"TRACKS/a.wav\","
            "\"bpm\":120,\"meter\":[4,4],\"first_downbeat_ms\":0,"
            "\"advance\":\"auto\",\"midi_clock\":%s}]}", value);
        assert(length > 0 && static_cast<size_t>(length) < sizeof(invalid));
        assert(!chompi::ParseBackingSet(invalid,
                                       static_cast<size_t>(length), set));
    }

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