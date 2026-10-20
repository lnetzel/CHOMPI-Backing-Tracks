#pragma once

#include <algorithm>

namespace chompi
{

struct BackingMixGains
{
    float track = 1.f;
    float line = 1.f;
    float click = 0.f;
    float headphones = 1.f;
    float main = 1.f;
};

struct BackingStereoFrame
{
    float left;
    float right;
};

struct BackingQuadFrame
{
    float headphone_left;
    float headphone_right;
    float main_left;
    float main_right;
};

inline float BackingProtect(float sample)
{
    return std::max(-1.f, std::min(1.f, sample));
}

inline BackingQuadFrame BackingMixFrame(const BackingStereoFrame &track,
                                        const BackingStereoFrame &line,
                                        const BackingStereoFrame &click,
                                        const BackingMixGains &gains)
{
    const float base_left = track.left * gains.track + line.left * gains.line;
    const float base_right = track.right * gains.track + line.right * gains.line;
    const float phone_left
        = (base_left + click.left * gains.click) * gains.headphones;
    const float phone_right
        = (base_right + click.right * gains.click) * gains.headphones;

    return {BackingProtect(phone_left), BackingProtect(phone_right),
            BackingProtect(base_left * gains.main),
            BackingProtect(base_right * gains.main)};
}

} // namespace chompi