#include "BackingMidi.h"

#include <cassert>
#include <cstdio>
#include <vector>

using chompi::BackingMidi;
using chompi::BackingTimingEvent;

static bool Capture(void *context, uint8_t byte)
{
    std::vector<uint8_t> *bytes = static_cast<std::vector<uint8_t> *>(context);
    bytes->push_back(byte);
    return true;
}

static bool Reject(void *, uint8_t)
{
    return false;
}

struct CapturingSink
{
    std::vector<uint8_t> &bytes;
    bool operator()(uint8_t byte) const { return Capture(&bytes, byte); }
};

struct RejectingSink
{
    bool operator()(uint8_t byte) const
    {
        (void)byte;
        return Reject(NULL, byte);
    }
};

int main()
{
    std::vector<uint8_t> bytes;
    CapturingSink sink{bytes};
    BackingMidi midi;
    BackingTimingEvent event = {};
    event.midi_start = true;
    assert(midi.Process(event, true, sink));
    assert((bytes == std::vector<uint8_t>{0xFA}));
    assert(midi.Started());

    std::vector<uint8_t> usb_bytes;
    auto usb_sink = [&](uint8_t byte) {
        usb_bytes.push_back(byte);
        return true;
    };
    assert(chompi::SendMidiBytePair(0xF8, sink, usb_sink));
    assert((bytes == std::vector<uint8_t>{0xFA, 0xF8}));
    assert((usb_bytes == std::vector<uint8_t>{0xF8}));
    bytes.clear();
    usb_bytes.clear();

    event = {};
    event.midi_clock = true;
    assert(midi.Process(event, true, sink));
    assert((bytes == std::vector<uint8_t>{0xF8}));
    bytes.clear();
    assert(midi.Pause(sink));
    assert((bytes == std::vector<uint8_t>{0xFC}));
    assert(midi.Started());
    bytes.clear();
    assert(midi.Process(event, false, sink));
    assert((bytes == std::vector<uint8_t>{0xF8}));
    bytes.clear();
    assert(midi.Continue(sink));
    assert((bytes == std::vector<uint8_t>{0xFB}));

    bytes.clear();
    assert(midi.SetEnabled(false, true, sink));
    assert((bytes == std::vector<uint8_t>{0xFC}));
    assert(!midi.Started() && !midi.RestartArmed());
    bytes.clear();
    event.midi_start = true;
    assert(midi.Process(event, true, sink));
    assert(bytes.empty());

    assert(midi.SetEnabled(true, true, sink));
    assert(midi.RestartArmed());
    event = {};
    event.click = true;
    event.downbeat = true;
    event.midi_clock = true;
    assert(midi.Process(event, true, sink));
    assert((bytes == std::vector<uint8_t>{0xFA}));
    assert(midi.Started() && !midi.RestartArmed());
    event = {};
    assert(midi.Process(event, true, sink));
    assert((bytes == std::vector<uint8_t>{0xFA, 0xF8}));

    bytes.clear();
    assert(midi.SetEnabled(false, true, sink));
    assert((bytes == std::vector<uint8_t>{0xFC}));
    bytes.clear();
    assert(midi.SetEnabled(true, false, sink));
    assert(!midi.RestartArmed());
    event = {};
    event.midi_start = true;
    assert(midi.Process(event, true, sink));
    assert((bytes == std::vector<uint8_t>{0xFA}));

    bytes.clear();
    assert(midi.SetEnabled(false, true, sink));
    assert((bytes == std::vector<uint8_t>{0xFC}));
    bytes.clear();
    assert(midi.SetEnabled(true, true, sink));
    event = {};
    event.click = true;
    event.downbeat = true;
    assert(midi.Process(event, false, sink));
    assert(bytes.empty() && midi.RestartArmed());
    assert(midi.Process(event, true, sink));
    assert((bytes == std::vector<uint8_t>{0xFA}));

    bytes.clear();
    assert(midi.SetEnabled(false, true, sink));
    bytes.clear();
    assert(midi.SetEnabled(true, true, sink));
    assert(midi.RestartArmed());
    assert(midi.Stop(sink));
    event = {};
    event.click = true;
    event.downbeat = true;
    assert(midi.Process(event, true, sink));
    assert(bytes.empty() && !midi.RestartArmed());

    bytes.clear();
    BackingMidi failed;
    event = {};
    event.midi_start = true;
    assert(!failed.Process(event, true, RejectingSink{}));
    assert(!failed.Started());
    bytes.clear();
    assert(failed.Process(event, true, sink));
    assert(failed.Started());
    assert(!failed.SetEnabled(false, true, RejectingSink{}));
    assert(!failed.Started());

    bool previous = false;
    assert(!chompi::BackingMidiToggleOnPress(false, true, true, previous));
    assert(previous);
    assert(!chompi::BackingMidiToggleOnPress(true, true, true, previous));
    assert(!chompi::BackingMidiToggleOnPress(true, false, true, previous));
    assert(chompi::BackingMidiToggleOnPress(true, true, true, previous));
    assert(!chompi::BackingMidiToggleOnPress(true, true, true, previous));
    assert(!chompi::BackingMidiToggleOnPress(true, false, false, previous));
    assert(!chompi::BackingMidiToggleOnPress(true, true, false, previous));

    BackingMidi entry_state;
    bytes.clear();
    assert(entry_state.Process(BackingTimingEvent{true, false, true, true},
                               true, sink));
    assert(entry_state.Started());
    assert(entry_state.ApplyEntry(false, sink));
    assert((bytes == std::vector<uint8_t>{0xFA, 0xFC}));
    assert(!entry_state.Enabled() && !entry_state.Started());
    assert(entry_state.ApplyEntry(true, sink));
    assert(entry_state.Enabled() && !entry_state.RestartArmed());

    chompi::BackingSong song = {};
    song.bpm = 120.f;
    song.meter_numerator = 4;
    song.meter_denominator = 4;
    chompi::BackingTiming timing;
    timing.Start(song);
    BackingMidi clocked;
    bytes.clear();
    event = timing.AdvanceFrame();
    assert(clocked.Process(event, true, sink));
    for(int frame = 0; frame < 96000; ++frame)
    {
        event = timing.AdvanceFrame();
        assert(clocked.Process(event, true, sink));
    }
    int clocks_per_bar = 0;
    for(uint8_t byte : bytes)
        clocks_per_bar += byte == 0xF8;
    assert(clocks_per_bar == 96);

    std::puts("backing_midi_test: all assertions passed");
    return 0;
}
