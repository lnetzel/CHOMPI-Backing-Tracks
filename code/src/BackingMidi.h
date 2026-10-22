#pragma once

#include "BackingTiming.h"

namespace chompi
{

class BackingMidi
{
  public:
    template <typename Sink>
    bool ApplyEntry(bool enabled, Sink sink)
    {
        const bool stopped = Stop(sink);
        enabled_ = enabled;
        restart_armed_ = false;
        return stopped;
    }

    template <typename Sink>
    bool SetEnabled(bool enabled,
                    bool timeline_started,
                    Sink sink)
    {
        if(enabled_ == enabled)
            return true;
        enabled_ = enabled;
        pending_clock_ = false;
        if(!enabled)
        {
            restart_armed_ = false;
            const bool stopped = !midi_started_ || sink(0xFC);
            midi_started_ = false;
            return stopped;
        }
        restart_armed_ = timeline_started;
        return true;
    }

    template <typename Sink>
    bool Pause(Sink sink)
    {
        return !enabled_ || !midi_started_ || sink(0xFC);
    }

    template <typename Sink>
    bool Continue(Sink sink)
    {
        return !enabled_ || !midi_started_ || sink(0xFB);
    }

    template <typename Sink>
    bool Stop(Sink sink)
    {
        const bool stopped = !midi_started_ || sink(0xFC);
        midi_started_ = false;
        restart_armed_ = false;
        pending_clock_ = false;
        return stopped;
    }

    template <typename Sink>
    bool Process(const BackingTimingEvent &event,
                 bool audio_playing,
                 Sink sink)
    {
        if(pending_clock_)
        {
            pending_clock_ = false;
            if(!sink(0xF8))
                return false;
        }
        if(!enabled_)
            return true;

        bool start_now = false;
        if(restart_armed_ && audio_playing && event.click && event.downbeat)
        {
            restart_armed_ = false;
            start_now = true;
        }
        else if(!restart_armed_ && event.midi_start)
            start_now = true;

        if(start_now && !midi_started_)
        {
            if(!sink(0xFA))
                return false;
            midi_started_ = true;
        }
        if(event.midi_clock && midi_started_)
        {
            if(start_now)
                pending_clock_ = true;
            else if(!sink(0xF8))
                return false;
        }
        return true;
    }

    bool Enabled() const { return enabled_; }
    bool Started() const { return midi_started_; }
    bool RestartArmed() const { return restart_armed_; }

  private:
    bool enabled_ = true;
    bool midi_started_ = false;
    bool restart_armed_ = false;
    bool pending_clock_ = false;
};

template <typename SinkA, typename SinkB>
bool SendMidiBytePair(uint8_t byte,
                      SinkA &&sink_a,
                      SinkB &&sink_b)
{
    return sink_a(byte) && sink_b(byte);
}

inline bool BackingMidiToggleOnPress(bool modifier_down,
                                     bool key_down,
                                     bool valid_entry,
                                     bool &previous_key_down)
{
    const bool pressed = key_down && !previous_key_down;
    previous_key_down = key_down;
    return pressed && modifier_down && valid_entry;
}

} // namespace chompi
