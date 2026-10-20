/** @file midi_cc_map_test.cpp
 *  @brief Native tests for MidiCcMap.h - the CC number map and the
 *  0-127 -> engine-value conversions behind the PIPES MIDI
 *  implementation (firmware/chompi-pipes/midi.md).
 *
 *  Build & run: make -C firmware/chompi-pipes/code/tests test
 */
#include "MidiCcMap.h"
#include <cassert>
#include <cstdio>

using namespace chompi::MidiCcMap;

static void test_cc_map_layout()
{
    /* the original blocks are untouched */
    assert(kVibratoCc == 1);
    assert(kLevelCcBase == 70);
    assert(kCharacterCcBase == 74);

    /* the per-pipe blocks are 4 consecutive CCs each, contiguous
     * 78..105 */
    const uint8_t bases[] = {kAttackCcBase, kReleaseCcBase,  kMuteCcBase,
                             kLengthCcBase, kVolModCcBase,   kCharModCcBase,
                             kLengthModCcBase};
    assert(kAttackCcBase == 78);
    for(size_t i = 1; i < sizeof(bases); i++)
        assert(bases[i] == bases[i - 1] + 4);

    /* the globals are contiguous 106..116 and stay clear of the
     * reserved channel-mode CCs 120-127 */
    const uint8_t globals[] = {kVibratoDelayCc, kRotaryCc,     kDriveCc,
                               kDriveToneCc,    kReverbMixCc,  kReverbDecayCc,
                               kPercussionCc,   kDelayMixCc,   kDelayTimeCc,
                               kDriveModCc,     kDelayModCc};
    assert(kVibratoDelayCc == 106);
    for(size_t i = 1; i < sizeof(globals); i++)
        assert(globals[i] == globals[i - 1] + 1);
    assert(kDriveModCc == 115);
    assert(kDelayModCc == 116);
}

static void test_steps_endpoints()
{
    assert(CcToPosition(0) == 0);
    assert(CcToPosition(64) == 16);
    assert(CcToPosition(127) == 32);
}

static void test_steps_rounding()
{
    bool seen[33] = {};
    int previous = -1;
    for(int v = 0; v < 128; v++)
    {
        const int position = CcToPosition(static_cast<uint8_t>(v));
        assert(position >= previous && position >= 0 && position <= 32);
        seen[position] = true;
        previous = position;
    }
    for(int position = 0; position <= 32; position++)
    {
        assert(seen[position]);
        assert(CcToPosition(PositionToCc(position)) == position);
    }
}

static void test_toggle()
{
    assert(!CcToToggle(0));
    assert(!CcToToggle(63));
    assert(CcToToggle(64));
    assert(CcToToggle(127));
}

static void test_length_anchors()
{
    /* the five anchors are exact */
    assert(CcToLengthOctave(0) == -2);
    assert(CcToLengthOctave(32) == -1);
    assert(CcToLengthOctave(64) == 0);
    assert(CcToLengthOctave(96) == 1);
    assert(CcToLengthOctave(127) == 2);
    /* MIDI first selects a shared position, then snaps to the octave. */
    assert(CcToLengthOctave(33) == -1);
    assert(CcToLengthOctave(95) == 1);
    assert(CcToLengthOctave(79) == 1);
    assert(CcToLengthOctave(80) == 1);
    /* every value lands inside -2..+2 */
    for(int v = 0; v < 128; v++)
    {
        const int oct = CcToLengthOctave(v);
        assert(oct >= -2 && oct <= 2);
        const int position = CcToPosition(static_cast<uint8_t>(v));
        assert(oct == chompi::ParameterSteps::PipeLengthOctave(position));
    }
}

static void test_delay_time()
{
    assert(chompi::ParameterSteps::DelayTimeMs(CcToPosition(0)) == 40);
    assert(chompi::ParameterSteps::DelayTimeMs(CcToPosition(127)) == 500);
    int prev = -1;
    for(int v = 1; v < 128; v++)
    {
        const int ms = chompi::ParameterSteps::DelayTimeMs(
            CcToPosition(static_cast<uint8_t>(v)));
        assert(ms >= prev);
        assert(ms >= 40 && ms <= 500);
        prev = ms;
    }
}

static void test_program_slots()
{
    /* PC numbers 0-14 (documented as slots 1-15) map to slots 0-14 */
    assert(ProgramToPresetSlot(0) == 0);
    assert(ProgramToPresetSlot(14) == 14);
    /* everything else is not a slot */
    assert(ProgramToPresetSlot(15) == -1);
    assert(ProgramToPresetSlot(127) == -1);
}

int main()
{
    test_cc_map_layout();
    test_steps_endpoints();
    test_steps_rounding();
    test_toggle();
    test_length_anchors();
    test_delay_time();
    test_program_slots();
    std::puts("midi_cc_map_test: all assertions passed");
    return 0;
}
