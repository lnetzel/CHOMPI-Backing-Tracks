#pragma once

#include <stddef.h>
#include <stdint.h>

namespace chompi
{

constexpr size_t kBackingMaxSongs = 30;
constexpr size_t kBackingMaxPath = 127;
constexpr size_t kBackingMaxSetBytes = 16 * 1024;

enum class BackingAdvance : uint8_t
{
    Wait,
    Auto,
    Loop
};

inline BackingAdvance NextBackingAdvance(BackingAdvance advance)
{
    return advance == BackingAdvance::Wait
               ? BackingAdvance::Auto
               : (advance == BackingAdvance::Auto ? BackingAdvance::Loop
                                                   : BackingAdvance::Wait);
}

inline const char *BackingAdvanceName(BackingAdvance advance)
{
    switch(advance)
    {
        case BackingAdvance::Wait: return "wait";
        case BackingAdvance::Auto: return "auto";
        case BackingAdvance::Loop: return "loop";
    }
    return NULL;
}

struct BackingAdvanceDecision
{
    int selected_song;
    bool begin_load;
};

inline BackingAdvanceDecision BackingAdvanceAtEof(BackingAdvance advance,
                                                  size_t current_song,
                                                  size_t song_count)
{
    if(current_song >= song_count)
        return {-1, false};
    if(advance == BackingAdvance::Loop)
        return {static_cast<int>(current_song), true};
    if(current_song + 1 >= song_count)
        return {static_cast<int>(current_song), false};
    return {static_cast<int>(current_song + 1),
            advance == BackingAdvance::Auto};
}

struct BackingSong
{
    char file[kBackingMaxPath + 1];
    float bpm;
    uint8_t meter_numerator;
    uint8_t meter_denominator;
    uint32_t first_downbeat_ms;
    BackingAdvance advance;
    bool midi_clock;
};

struct BackingSet
{
    BackingSong songs[kBackingMaxSongs];
    size_t count;
};

} // namespace chompi