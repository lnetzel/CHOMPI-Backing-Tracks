#include "BackingBpmEditor.h"

#include <cassert>
#include <cstdio>

static void ReleaseInputs(chompi::BackingBpmEditorInput &input)
{
    input.shift_down = false;
    for(bool &down : input.selector_down)
        down = false;
    for(bool &down : input.white_down)
        down = false;
}

static void EnterPosition(chompi::BackingBpmEditor &editor,
                          chompi::BackingBpmEditorInput &input,
                          size_t position,
                          uint16_t current)
{
    ReleaseInputs(input);
    editor.Scan(input, current);
    input.shift_down = true;
    input.selector_down[position] = true;
    const auto action = editor.Scan(input, current);
    assert(action.entered && static_cast<size_t>(action.position) == position);
    ReleaseInputs(input);
    editor.Scan(input, current);
}

static uint16_t ChooseDigit(chompi::BackingBpmEditor &editor,
                            chompi::BackingBpmEditorInput &input,
                            uint8_t digit,
                            uint16_t current)
{
    input.white_down[digit == 0 ? 9 : digit - 1] = true;
    const auto action = editor.Scan(input, current);
    assert(action.submit);
    assert(editor.Active() && editor.Saving());
    assert(editor.CompleteSave(true));
    assert(!editor.Active() && !editor.Saving());
    ReleaseInputs(input);
    editor.Scan(input, action.requested_hundredths);
    return action.requested_hundredths;
}

int main()
{
    using namespace chompi;
    for(uint8_t digit = 0; digit <= 9; ++digit)
        assert(BackingBpmWhiteKeyIndex(digit) == (digit == 0 ? 9 : digit - 1));
    assert(BackingBpmWhiteKeyIndex(10) == -1);

    BackingBpmEditor editor;
    BackingBpmEditorInput input = {};
    input.stopped = true;
    input.valid_entry = true;
    input.clean_entry_chord = true;
    editor.Initialize(input);

    input.shift_down = true;
    input.selector_down[0] = true;
    BackingBpmEditorAction action = editor.Scan(input, 12000);
    assert(action.entered && !action.submit);
    assert(action.position == BackingBpmPosition::Hundreds);
    assert(editor.Active() && editor.SelectedDigit(12000) == 1);
    action = editor.Scan(input, 12000);
    assert(!action.exit && editor.Active());
    ReleaseInputs(input);
    editor.Scan(input, 12000);
    assert(editor.Active());

    input.white_down[1] = true;
    action = editor.Scan(input, 12000);
    assert(action.submit && action.requested_hundredths == 22000);
    assert(editor.Saving());
    assert(!editor.CompleteSave(false));
    assert(editor.Active() && !editor.Saving());
    ReleaseInputs(input);
    editor.Scan(input, 12000);
    input.shift_down = true;
    action = editor.Scan(input, 12000);
    assert(action.exit && !editor.Active());

    ReleaseInputs(input);
    editor.Scan(input, 12554);
    input.shift_down = true;
    input.selector_down[4] = true;
    action = editor.Scan(input, 12554);
    assert(action.entered && action.position == BackingBpmPosition::Hundredths);
    assert(editor.SelectedDigit(12554) == 4);
    ReleaseInputs(input);
    editor.Scan(input, 12554);
    input.white_down[3] = true;
    action = editor.Scan(input, 12554);
    assert(action.exit && action.unchanged && !action.submit);
    assert(!editor.Active());

    ReleaseInputs(input);
    editor.Scan(input, 30000);
    input.shift_down = true;
    input.selector_down[4] = true;
    action = editor.Scan(input, 30000);
    assert(action.entered);
    ReleaseInputs(input);
    editor.Scan(input, 30000);
    input.white_down[0] = true;
    action = editor.Scan(input, 30000);
    assert(action.invalid_value && !action.submit && editor.Active());
    ReleaseInputs(input);
    editor.Scan(input, 30000);
    input.white_down[0] = true;
    input.white_down[1] = true;
    action = editor.Scan(input, 30000);
    assert(action.invalid_value && !action.submit && editor.Active());

    ReleaseInputs(input);
    editor.Scan(input, 30000);
    input.shift_down = true;
    input.white_down[9] = true;
    action = editor.Scan(input, 30000);
    assert(action.exit && !action.submit && !editor.Active());

    ReleaseInputs(input);
    editor.Scan(input, 12554);
    input.shift_down = true;
    input.selector_down[3] = true;
    action = editor.Scan(input, 12554);
    assert(action.entered && action.position == BackingBpmPosition::Tenths);
    ReleaseInputs(input);
    editor.Scan(input, 12554);
    input.white_down[0] = true;
    action = editor.Scan(input, 12554);
    assert(action.submit && action.requested_hundredths == 12514);
    ReleaseInputs(input);
    editor.Scan(input, 12554);
    input.shift_down = true;
    action = editor.Scan(input, 12554);
    assert(!action.exit && editor.Saving() && editor.Active());
    assert(editor.CompleteSave(false));
    assert(!editor.Active() && !editor.Saving());

    ReleaseInputs(input);
    editor.Scan(input, 12554);
    input.stopped = false;
    input.shift_down = true;
    input.selector_down[2] = true;
    action = editor.Scan(input, 12554);
    assert(!action.entered && !editor.Active());

    BackingBpmEditor boot_held_editor;
    BackingBpmEditorInput boot_held_input = {};
    boot_held_input.shift_down = true;
    boot_held_input.selector_down[0] = true;
    boot_held_input.stopped = true;
    boot_held_input.valid_entry = true;
    boot_held_input.clean_entry_chord = true;
    action = boot_held_editor.Scan(boot_held_input, 12000);
    assert(!action.entered && !boot_held_editor.Active());
    ReleaseInputs(boot_held_input);
    boot_held_editor.Scan(boot_held_input, 12000);
    boot_held_input.shift_down = true;
    boot_held_input.selector_down[0] = true;
    action = boot_held_editor.Scan(boot_held_input, 12000);
    assert(action.entered && boot_held_editor.Active());

    BackingBpmEditor navigation_editor;
    BackingBpmEditorInput navigation_input = {};
    navigation_input.stopped = true;
    navigation_input.valid_entry = true;
    navigation_input.clean_entry_chord = true;
    navigation_editor.Initialize(navigation_input);
    EnterPosition(navigation_editor, navigation_input, 4, 12554);
    navigation_input.selector_down[3] = true;
    action = navigation_editor.Scan(navigation_input, 12554);
    assert(navigation_editor.Active());
    assert(action.position == BackingBpmPosition::Tenths);
    assert(navigation_editor.SelectedDigit(12554) == 5);
    ReleaseInputs(navigation_input);
    navigation_editor.Scan(navigation_input, 12554);
    navigation_input.selector_down[2] = true;
    action = navigation_editor.Scan(navigation_input, 12554);
    assert(navigation_editor.Active());
    assert(action.position == BackingBpmPosition::Ones);
    ReleaseInputs(navigation_input);
    navigation_editor.Scan(navigation_input, 12554);
    navigation_input.shift_down = true;
    action = navigation_editor.Scan(navigation_input, 12554);
    assert(action.exit && !navigation_editor.Active());

    BackingBpmEditor sequence_editor;
    BackingBpmEditorInput sequence_input = {};
    sequence_input.stopped = true;
    sequence_input.valid_entry = true;
    sequence_input.clean_entry_chord = true;
    sequence_editor.Initialize(sequence_input);
    EnterPosition(sequence_editor, sequence_input, 2, 12000);
    uint16_t edited = ChooseDigit(sequence_editor, sequence_input, 5, 12000);
    assert(edited == 12500);
    EnterPosition(sequence_editor, sequence_input, 3, edited);
    edited = ChooseDigit(sequence_editor, sequence_input, 5, edited);
    assert(edited == 12550);
    EnterPosition(sequence_editor, sequence_input, 4, edited);
    edited = ChooseDigit(sequence_editor, sequence_input, 4, edited);
    assert(edited == 12554);
    EnterPosition(sequence_editor, sequence_input, 2, edited);
    edited = ChooseDigit(sequence_editor, sequence_input, 6, edited);
    assert(edited == 12654);

    EnterPosition(sequence_editor, sequence_input, 1, 12500);
    edited = ChooseDigit(sequence_editor, sequence_input, 3, 12500);
    assert(edited == 13500);
    EnterPosition(sequence_editor, sequence_input, 0, edited);
    edited = ChooseDigit(sequence_editor, sequence_input, 0, edited);
    assert(edited == 3500);
    EnterPosition(sequence_editor, sequence_input, 2, edited);
    edited = ChooseDigit(sequence_editor, sequence_input, 0, edited);
    assert(edited == 3000);

    EnterPosition(sequence_editor, sequence_input, 1, 3500);
    sequence_input.white_down[1] = true;
    action = sequence_editor.Scan(sequence_input, 3500);
    assert(action.invalid_value && !action.submit && sequence_editor.Active());
    assert(!sequence_editor.CompleteSave(false));
    ReleaseInputs(sequence_input);
    sequence_editor.Initialize(sequence_input);

    EnterPosition(sequence_editor, sequence_input, 2, 30000);
    sequence_input.white_down[0] = true;
    action = sequence_editor.Scan(sequence_input, 30000);
    assert(action.invalid_value && !action.submit && sequence_editor.Active());

    std::puts("backing_bpm_editor_test: all assertions passed");
    return 0;
}