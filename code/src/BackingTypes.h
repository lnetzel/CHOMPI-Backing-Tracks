#pragma once

#include <stddef.h>
#include <stdint.h>

namespace chompi
{

constexpr size_t kBackingMaxSongs = 30;
constexpr size_t kBackingMaxPath = 127;
constexpr size_t kBackingMaxSetBytes = 16 * 1024;

struct BackingSong
{
    char file[kBackingMaxPath + 1];
    float bpm;
    uint8_t meter_numerator;
    uint8_t meter_denominator;
    uint32_t first_downbeat_ms;
    bool advance_auto;
};

struct BackingSet
{
    BackingSong songs[kBackingMaxSongs];
    size_t count;
};

} // namespace chompi