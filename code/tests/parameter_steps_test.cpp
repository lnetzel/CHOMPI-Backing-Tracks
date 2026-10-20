#include "ParameterSteps.h"
#include <cassert>
#include <cmath>
#include <cstdio>

using namespace chompi::ParameterSteps;

static void test_position_and_midi_roundtrips()
{
    assert(CcToPosition(0) == 0);
    assert(CcToPosition(64) == 16);
    assert(CcToPosition(127) == 32);
    int previous = -1;
    bool seen[33] = {};
    for(int value = 0; value <= 127; ++value)
    {
        const int position = CcToPosition(static_cast<uint8_t>(value));
        assert(position >= previous && position >= 0 && position <= 32);
        seen[position] = true;
        previous = position;
    }
    for(int position = 0; position <= 32; ++position)
    {
        assert(seen[position]);
        assert(CcToPosition(PositionToCc(position)) == position);
    }
}

static void test_turns_and_physical_mappings()
{
    int position = 0;
    for(int tick = 0; tick < 32; ++tick)
        position = Advance(position, 1);
    assert(position == 32);
    for(int tick = 0; tick < 32; ++tick)
        position = Advance(position, -1);
    assert(position == 0);
    assert(Advance(0, -100) == 0);
    assert(Advance(32, 100) == 32);
    assert(Advance(32, -1) == 31);
    assert(Advance(0, 1) == 1);

    assert(FineTuneCents(0) == -50);
    assert(FineTuneCents(16) == 0);
    assert(FineTuneCents(32) == 50);
    for(int step = 0; step <= 16; ++step)
        assert(FineTuneCents(step) == -FineTuneCents(32 - step));
    assert(ScaledInteger(32, 50) == 50);
    assert(DelayTimeMs(0) == 40 && DelayTimeMs(32) == 500);
    assert(std::fabs(VibratoDelayMs(32) - 5000.f) < 0.01f);
    assert(PipeLengthOctave(0) == -2);
    assert(PipeLengthOctave(8) == -1);
    assert(PipeLengthOctave(16) == 0);
    assert(PipeLengthOctave(24) == 1);
    assert(PipeLengthOctave(32) == 2);
    assert(LegacyDelayPosition(250) == 15);
    assert(DelayTimeMs(LegacyDelayPosition(250)) == 256);
}

static void test_all_physical_mappings()
{
    float previous_gain = -1.f;
    float previous_decay = 0.f;
    float previous_delay = -1.f;
    int previous_character = -1;
    int previous_vibrato = -1;
    int previous_tone = -1;
    int previous_mix = -1;
    int previous_time = 0;
    int previous_fine = -51;
    for(int position = 0; position <= 32; ++position)
    {
        const float gain = PipeLevelGain(position);
        const float decay = ReverbDecaySeconds(position);
        const float delay = VibratoDelayMs(position);
        const int character = CharacterDspStep(position);
        const int vibrato = VibratoDepthCents(position);
        const int tone = DriveTonePercent(position);
        const int mix = DelayCombinedPercent(position);
        const int time = DelayTimeMs(position);
        const int fine = FineTuneCents(position);
        assert(gain > previous_gain);
        assert(decay > previous_decay);
        assert(delay > previous_delay);
        assert(character > previous_character);
        assert(vibrato > previous_vibrato);
        assert(tone > previous_tone);
        assert(mix > previous_mix);
        assert(time > previous_time);
        assert(fine > previous_fine);
        previous_gain = gain;
        previous_decay = decay;
        previous_delay = delay;
        previous_character = character;
        previous_vibrato = vibrato;
        previous_tone = tone;
        previous_mix = mix;
        previous_time = time;
        previous_fine = fine;
    }
}

int main()
{
    test_position_and_midi_roundtrips();
    test_turns_and_physical_mappings();
    test_all_physical_mappings();
    std::puts("parameter_steps_test: all assertions passed");
    return 0;
}