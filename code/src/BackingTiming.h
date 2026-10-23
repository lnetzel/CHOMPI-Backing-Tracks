#pragma once

#include <math.h>
#include <stdint.h>

#include "BackingTypes.h"

namespace chompi
{

struct BackingTimingEvent
{
    bool midi_start;
    bool midi_clock;
    bool click;
    bool downbeat;
};

class BackingTiming
{
  public:
    static constexpr uint32_t kSampleRate = 48000;
    static constexpr uint64_t kPhaseOne = UINT64_C(1) << 48;

    void Start(const BackingSong &song)
    {
        bpm_ = static_cast<double>(song.bpm_hundredths) / 100.0;
        beat_count_ = song.meter_numerator;
        denominator_ = song.meter_denominator;
        lead_frames_ = static_cast<uint64_t>(song.first_downbeat_ms) * 48u;
        first_downbeat_frames_ = lead_frames_;
        clock_phase_ = 0;
        click_phase_ = 0;
        bar_phase_ = 0;
        previous_bar_phase_ = 0;
        beat_in_bar_ = 0;
        active_ = false;
        const double clock_rate
            = static_cast<double>(bpm_) * 24.0 / (60.0 * kSampleRate);
        const double click_rate
            = static_cast<double>(bpm_)
              * (denominator_ == 8 ? 2.0 : 1.0) / (60.0 * kSampleRate);
        clock_increment_ = static_cast<uint64_t>(clock_rate * kPhaseOne + .5);
        click_increment_ = static_cast<uint64_t>(click_rate * kPhaseOne + .5);
    }

    BackingTimingEvent AdvanceFrame()
    {
        BackingTimingEvent event = {};
        if(!active_)
        {
            if(lead_frames_ != 0)
            {
                --lead_frames_;
                return event;
            }
            active_ = true;
            event.midi_start = true;
            event.click = true;
            event.downbeat = true;
            beat_in_bar_ = 1;
            return event;
        }

        clock_phase_ += clock_increment_;
        if(clock_phase_ >= kPhaseOne)
        {
            clock_phase_ -= kPhaseOne;
            event.midi_clock = true;
        }

        click_phase_ += click_increment_;
        previous_bar_phase_ = bar_phase_;
        const uint64_t bar_length
            = static_cast<uint64_t>(beat_count_) * kPhaseOne;
        bar_phase_ += click_increment_;
        if(bar_phase_ >= bar_length)
            bar_phase_ -= bar_length;
        if(click_phase_ >= kPhaseOne)
        {
            click_phase_ -= kPhaseOne;
            event.click = true;
            event.downbeat = beat_in_bar_ == 0;
            beat_in_bar_ = static_cast<uint8_t>(beat_in_bar_ + 1u);
            if(beat_in_bar_ >= beat_count_)
                beat_in_bar_ = 0;
        }
        return event;
    }

    bool Active() const { return active_; }

    void Stop() { active_ = false; }

    bool PositionInBar(uint32_t audio_frame, uint64_t &phase) const
    {
        if(audio_frame < first_downbeat_frames_)
            return false;
        const uint64_t bar_length
            = static_cast<uint64_t>(beat_count_) * kPhaseOne;
        phase = MultiplyModulo(audio_frame - first_downbeat_frames_,
                               click_increment_, bar_length);
        return true;
    }

    bool CrossedBarPhase(uint64_t target) const
    {
        const uint64_t bar_length
            = static_cast<uint64_t>(beat_count_) * kPhaseOne;
        target %= bar_length;
        if(bar_phase_ >= previous_bar_phase_)
            return target > previous_bar_phase_ && target <= bar_phase_;
        return target > previous_bar_phase_ || target <= bar_phase_;
    }

  private:
    static uint64_t ModuloAdd(uint64_t left, uint64_t right, uint64_t modulus)
    {
        return left >= modulus - right ? left - (modulus - right)
                                       : left + right;
    }

    static uint64_t MultiplyModulo(uint64_t multiplicand,
                                   uint64_t multiplier,
                                   uint64_t modulus)
    {
        uint64_t result = 0;
        uint64_t addend = multiplier % modulus;
        while(multiplicand != 0)
        {
            if((multiplicand & 1u) != 0)
                result = ModuloAdd(result, addend, modulus);
            addend = ModuloAdd(addend, addend, modulus);
            multiplicand >>= 1;
        }
        return result;
    }

    double bpm_ = 120.0;
    uint64_t clock_increment_ = 0;
    uint64_t click_increment_ = 0;
    uint64_t clock_phase_ = 0;
    uint64_t click_phase_ = 0;
    uint64_t bar_phase_ = 0;
    uint64_t previous_bar_phase_ = 0;
    uint64_t lead_frames_ = 0;
    uint64_t first_downbeat_frames_ = 0;
    uint8_t beat_count_ = 4;
    uint8_t denominator_ = 4;
    uint8_t beat_in_bar_ = 0;
    bool active_ = false;
};

class BackingClick
{
  public:
    static constexpr uint32_t kDurationFrames = 1920;

    void Trigger(bool accent)
    {
        amplitude_ = accent ? .8f : .65f;
        phase_increment_ = (accent ? 1320.f : 880.f) / 48000.f;
        remaining_ = kDurationFrames;
        phase_ = 0.f;
    }

    float NextSample()
    {
        if(remaining_ == 0)
            return 0.f;
        const uint32_t elapsed = kDurationFrames - remaining_;
        const float attack = elapsed < 96 ? static_cast<float>(elapsed) / 96.f
                                          : 1.f;
        const float decay
            = static_cast<float>(remaining_) / kDurationFrames;
        const float sample = sinf(phase_ * 6.28318530718f) * amplitude_
                             * attack * decay * decay;
        phase_ += phase_increment_;
        if(phase_ >= 1.f)
            phase_ -= 1.f;
        --remaining_;
        return sample;
    }

  private:
    float amplitude_ = 0.f;
    float phase_ = 0.f;
    float phase_increment_ = 0.f;
    uint32_t remaining_ = 0;
};

} // namespace chompi