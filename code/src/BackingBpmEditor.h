#pragma once

#include <stddef.h>
#include <stdint.h>

namespace chompi
{

enum class BackingBpmPosition : uint8_t
{
    Hundreds,
    Tens,
    Ones,
    Tenths,
    Hundredths
};

inline int BackingBpmWhiteKeyIndex(uint8_t digit)
{
    return digit == 0 ? 9 : (digit <= 9 ? digit - 1 : -1);
}

struct BackingBpmEditorInput
{
    bool shift_down;
    bool selector_down[5];
    bool white_down[10];
    bool stopped;
    bool valid_entry;
    bool clean_entry_chord;
};

struct BackingBpmEditorAction
{
    bool entered = false;
    bool exit = false;
    bool submit = false;
    bool unchanged = false;
    bool invalid_value = false;
    BackingBpmPosition position = BackingBpmPosition::Hundreds;
    uint16_t requested_hundredths = 0;
};

class BackingBpmEditor
{
  public:
    static constexpr uint16_t kMinimumHundredths = 3000;
    static constexpr uint16_t kMaximumHundredths = 30000;

    void Initialize(const BackingBpmEditorInput &input)
    {
        CopyInput(input);
        initialized_ = true;
        active_ = false;
        saving_ = false;
        exit_armed_ = false;
        exit_after_save_ = false;
    }

    BackingBpmEditorAction Scan(const BackingBpmEditorInput &input,
                                uint16_t current_hundredths)
    {
        BackingBpmEditorAction action = {};
        if(!initialized_)
        {
            CopyInput(input);
            initialized_ = true;
            return action;
        }
        bool selector_edges[5] = {};
        bool white_edges[10] = {};
        for(size_t i = 0; i < 5; ++i)
            selector_edges[i] = input.selector_down[i] && !previous_selectors_[i];
        for(size_t i = 0; i < 10; ++i)
            white_edges[i] = input.white_down[i] && !previous_white_[i];
        const bool shift_edge = input.shift_down && !previous_shift_;
        CopyInput(input);
        initialized_ = true;

        if(!active_)
        {
            if(!input.stopped || !input.valid_entry || !input.clean_entry_chord
               || !input.shift_down)
                return action;
            int selected = -1;
            for(size_t i = 0; i < 5; ++i)
            {
                if(!input.selector_down[i])
                    continue;
                if(selected >= 0)
                    return action;
                selected = static_cast<int>(i);
            }
            if(selected < 0 || !selector_edges[selected])
                return action;
            position_ = static_cast<BackingBpmPosition>(selected);
            active_ = true;
            exit_armed_ = !input.shift_down;
            action.entered = true;
            action.position = position_;
            return action;
        }

        if(!input.shift_down)
            exit_armed_ = true;
        if(shift_edge && exit_armed_)
        {
            if(saving_)
                exit_after_save_ = true;
            else
            {
                active_ = false;
                action.exit = true;
            }
            action.position = position_;
            return action;
        }

        if(saving_)
        {
            action.position = position_;
            return action;
        }

        size_t digit_key = 0;
        size_t digit_edges = 0;
        for(size_t i = 0; i < 10; ++i)
        {
            if(white_edges[i])
            {
                digit_key = i;
                ++digit_edges;
            }
        }
        action.position = position_;
        if(digit_edges == 0)
            return action;
        if(digit_edges != 1)
        {
            action.invalid_value = true;
            return action;
        }

        static const uint16_t weights[5] = {10000, 1000, 100, 10, 1};
        const size_t position_index = static_cast<size_t>(position_);
        const uint16_t weight = weights[position_index];
        const uint8_t digit
            = digit_key == 9 ? 0 : static_cast<uint8_t>(digit_key + 1);
        const uint8_t saved_digit
            = static_cast<uint8_t>((current_hundredths / weight) % 10u);
        if(digit == saved_digit)
        {
            active_ = false;
            action.exit = true;
            action.unchanged = true;
            return action;
        }

        const int32_t replacement
            = static_cast<int32_t>(current_hundredths)
              + (static_cast<int32_t>(digit) - saved_digit) * weight;
        if(replacement < kMinimumHundredths
           || replacement > kMaximumHundredths)
        {
            action.invalid_value = true;
            return action;
        }
        saving_ = true;
        action.submit = true;
        action.requested_hundredths = static_cast<uint16_t>(replacement);
        return action;
    }

    bool CompleteSave(bool success)
    {
        if(!saving_)
            return false;
        saving_ = false;
        if(success || exit_after_save_)
        {
            active_ = false;
            exit_after_save_ = false;
            return true;
        }
        exit_after_save_ = false;
        return false;
    }

    bool Active() const { return active_; }
    bool Saving() const { return saving_; }
    BackingBpmPosition Position() const { return position_; }

    uint8_t SelectedDigit(uint16_t hundredths) const
    {
        static const uint16_t weights[5] = {10000, 1000, 100, 10, 1};
        return static_cast<uint8_t>(
            (hundredths / weights[static_cast<size_t>(position_)]) % 10u);
    }

  private:
    void CopyInput(const BackingBpmEditorInput &input)
    {
        previous_shift_ = input.shift_down;
        for(size_t i = 0; i < 5; ++i)
            previous_selectors_[i] = input.selector_down[i];
        for(size_t i = 0; i < 10; ++i)
            previous_white_[i] = input.white_down[i];
    }

    bool initialized_ = false;
    bool previous_shift_ = false;
    bool previous_selectors_[5] = {};
    bool previous_white_[10] = {};
    bool active_ = false;
    bool saving_ = false;
    bool exit_armed_ = false;
    bool exit_after_save_ = false;
    BackingBpmPosition position_ = BackingBpmPosition::Hundreds;
};

} // namespace chompi