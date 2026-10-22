#pragma once

#include "BackingTransport.h"

namespace chompi
{

inline float BackingLevelBrightness(uint8_t step)
{
    return static_cast<float>(step > 32 ? 32 : step) / 32.f;
}

inline uint8_t BackingLevelBar(uint8_t step)
{
    return static_cast<uint8_t>((static_cast<uint32_t>(step > 32 ? 32 : step)
                                * 15u + 31u) / 32u);
}

inline bool BackingLevelVisible(uint32_t now, uint32_t changed_at)
{
    return now - changed_at < 1500u;
}

inline bool BackingMetronomeBlinkVisible(uint32_t now, uint32_t beat_at)
{
    return now - beat_at < 80u;
}

inline bool BackingBeatVisible(uint32_t now, uint32_t beat_at)
{
    return BackingMetronomeBlinkVisible(now, beat_at);
}

inline bool BackingPlayLight(BackingTransport::State state, uint32_t now)
{
    return state == BackingTransport::State::Paused
           || state == BackingTransport::State::PendingResume
           || (state == BackingTransport::State::Playing
               && ((now / 250u) & 1u) == 0);
}

inline bool BackingStopLight(BackingTransport::State state)
{
    return state == BackingTransport::State::Stopped;
}

} // namespace chompi