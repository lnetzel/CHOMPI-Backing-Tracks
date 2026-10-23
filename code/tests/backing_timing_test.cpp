#include "BackingFeedback.h"
#include "BackingTiming.h"

#include <cassert>
#include <cmath>
#include <cstdio>

int main()
{
    chompi::BackingSong song = {};
    song.bpm_hundredths = 12000;
    song.meter_numerator = 4;
    song.meter_denominator = 4;
    song.first_downbeat_ms = 1;
    chompi::BackingTiming timing;
    timing.Start(song);

    chompi::BackingTimingEvent event = {};
    for(int frame = 0; frame < 48; ++frame)
    {
        event = timing.AdvanceFrame();
        assert(!event.midi_start && !event.click && !event.midi_clock);
    }
    event = timing.AdvanceFrame();
    assert(event.midi_start && event.click && event.downbeat);

    int clock_ticks = 0;
    int click_count = 0;
    int downbeats = 1;
    int previous_click_frame = 0;
    int first_click_interval = 0;
    for(int frame = 1; frame <= 48000; ++frame)
    {
        event = timing.AdvanceFrame();
        if(event.midi_clock)
            ++clock_ticks;
        if(event.click)
        {
            ++click_count;
            if(first_click_interval == 0)
                first_click_interval = frame - previous_click_frame;
            previous_click_frame = frame;
        }
        if(event.downbeat)
            ++downbeats;
    }
    assert(clock_ticks == 48);
    assert(click_count == 2);
    assert(downbeats == 1);
    assert(std::abs(first_click_interval - 24000) <= 1);

    chompi::BackingSong rounded_song = {};
    rounded_song.bpm_hundredths = 13346;
    rounded_song.meter_numerator = 4;
    rounded_song.meter_denominator = 4;
    chompi::BackingSong explicit_song = rounded_song;
    chompi::BackingTiming rounded_timing;
    chompi::BackingTiming explicit_timing;
    rounded_timing.Start(rounded_song);
    explicit_timing.Start(explicit_song);
    for(int frame = 0; frame < 96000; ++frame)
    {
        const chompi::BackingTimingEvent rounded_event
            = rounded_timing.AdvanceFrame();
        const chompi::BackingTimingEvent explicit_event
            = explicit_timing.AdvanceFrame();
        assert(rounded_event.midi_start == explicit_event.midi_start);
        assert(rounded_event.midi_clock == explicit_event.midi_clock);
        assert(rounded_event.click == explicit_event.click);
        assert(rounded_event.downbeat == explicit_event.downbeat);
    }
    assert(chompi::BackingMetronomeBlinkVisible(79u, 0u));
    assert(!chompi::BackingMetronomeBlinkVisible(80u, 0u));
    assert(!chompi::BackingMetronomeBlinkVisible(100u, 20u));
    assert(!chompi::BackingMetronomeBlinkVisible(100u, 19u));

    song.bpm_hundredths = 9850;
    song.meter_numerator = 6;
    song.meter_denominator = 8;
    song.first_downbeat_ms = 0;
    timing.Start(song);
    event = timing.AdvanceFrame();
    assert(event.midi_start && event.click && event.downbeat);
    int eighth_clicks = 0;
    for(int frame = 0; frame < 30000; ++frame)
    {
        event = timing.AdvanceFrame();
        if(event.click)
            ++eighth_clicks;
    }
    assert(eighth_clicks >= 1);

    song.bpm_hundredths = 12000;
    song.meter_numerator = 4;
    song.meter_denominator = 4;
    timing.Start(song);
    event = timing.AdvanceFrame();
    uint64_t resume_phase = 0;
    assert(timing.PositionInBar(12000, resume_phase));
    bool resumed_at_saved_phase = false;
    for(int frame = 0; frame < 12002; ++frame)
    {
        timing.AdvanceFrame();
        if(timing.CrossedBarPhase(resume_phase))
        {
            resumed_at_saved_phase = true;
            assert(frame >= 11998 && frame <= 12001);
            break;
        }
    }
    assert(resumed_at_saved_phase);

    for(int denominator = 4; denominator <= 8; denominator += 4)
    {
        song.meter_numerator = denominator == 4 ? 4 : 6;
        song.meter_denominator = static_cast<uint8_t>(denominator);
        timing.Start(song);
        int beats = 0;
        while(beats < song.meter_numerator * 3)
        {
            event = timing.AdvanceFrame();
            if(!event.click)
                continue;
            assert(event.downbeat == (beats % song.meter_numerator == 0));
            ++beats;
        }
    }

    int crossings[2] = {};
    for(int accent = 0; accent < 2; ++accent)
    {
        chompi::BackingClick click;
        assert(click.NextSample() == 0.f);
        click.Trigger(accent != 0);
        assert(click.NextSample() == 0.f);
        float peak = 0.f;
        float previous = 0.f;
        float early_energy = 0.f;
        float late_energy = 0.f;
        for(uint32_t frame = 1;
            frame < chompi::BackingClick::kDurationFrames; ++frame)
        {
            const float sample = click.NextSample();
            assert(std::isfinite(sample) && std::fabs(sample) <= .8f);
            if(previous <= 0.f && sample > 0.f)
                ++crossings[accent];
            previous = sample;
            if(std::fabs(sample) > peak)
                peak = std::fabs(sample);
            if(frame < 480)
                early_energy += sample * sample;
            if(frame >= 1440)
                late_energy += sample * sample;
            if(frame < 12)
                assert(std::fabs(sample) < .1f);
        }
        assert(peak > .5f);
        assert(early_energy > late_energy * 10.f);
        assert(std::fabs(previous) < .00001f);
        assert(click.NextSample() == 0.f);
        click.Trigger(accent == 0);
        assert(click.NextSample() == 0.f);
        assert(click.NextSample() != 0.f);
        click.Trigger(accent != 0);
        assert(click.NextSample() == 0.f);
    }
    assert(std::abs(crossings[0] - 35) <= 1);
    assert(std::abs(crossings[1] - 53) <= 1);
    assert(crossings[1] > crossings[0]);

    std::puts("backing_timing_test: all assertions passed");
    return 0;
}