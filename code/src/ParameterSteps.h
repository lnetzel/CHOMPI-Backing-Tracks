#pragma once
#include <cstdint>
#include <cmath>

namespace chompi
{
namespace ParameterSteps
{
    constexpr int kMax = 32;

    inline int Clamp(int position)
    {
        return position < 0 ? 0 : (position > kMax ? kMax : position);
    }

    inline int Advance(int position, int delta)
    {
        return Clamp(Clamp(position) + delta);
    }

    inline int CcToPosition(uint8_t value)
    {
        return (static_cast<int>(value) * kMax + 63) / 127;
    }

    inline uint8_t PositionToCc(int position)
    {
        return static_cast<uint8_t>((Clamp(position) * 127 + 16) / kMax);
    }

    inline int ScaledInteger(int position, int maximum)
    {
        return (Clamp(position) * maximum + kMax / 2) / kMax;
    }

    inline float Normalized(int position)
    {
        return static_cast<float>(Clamp(position)) / kMax;
    }

    inline float PipeLevelGain(int position)
    {
        const float value = Normalized(position);
        return value * value;
    }

    inline int CharacterDspStep(int position)
    {
        return Clamp(position) * 2;
    }

    inline int VibratoDepthCents(int position)
    {
        return ScaledInteger(position, 50);
    }

    inline int DriveTonePercent(int position)
    {
        return ScaledInteger(position, 100);
    }

    inline float ReverbDecaySeconds(int position)
    {
        return .2f * std::pow(100.f, Normalized(position));
    }

    inline int DelayCombinedPercent(int position)
    {
        return ScaledInteger(position, 100);
    }

    inline int FineTuneCents(int position)
    {
        const int offset = Clamp(position) - kMax / 2;
        const int magnitude = (100 * (offset < 0 ? -offset : offset) + 16) / kMax;
        return offset < 0 ? -magnitude : magnitude;
    }

    inline float VibratoDelayMs(int position)
    {
        return 5000.f * Normalized(position);
    }

    inline int DelayTimeMs(int position)
    {
        return 40 + ScaledInteger(position, 460);
    }

    inline int PipeLengthOctave(int position)
    {
        return (Clamp(position) + 4) / 8 - 2;
    }

    inline int LegacyPosition(int value, int old_minimum, int old_maximum)
    {
        value = value < old_minimum ? old_minimum
                                    : (value > old_maximum ? old_maximum : value);
        return ((value - old_minimum) * kMax
                + (old_maximum - old_minimum) / 2)
               / (old_maximum - old_minimum);
    }

    inline int LegacyDelayPosition(int milliseconds)
    {
        return LegacyPosition(milliseconds, 40, 500);
    }
} // namespace ParameterSteps
} // namespace chompi