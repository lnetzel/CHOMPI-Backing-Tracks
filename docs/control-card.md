# Control Card

## Levels

- Encoders 1-4: track, aux, headphone click, headphone output.
- Encoder 5: previous/next song.
- Encoder 6: main output.
- Each level has 33 positions, 0-32; values clamp at both ends.
- Turning a level encoder shows its level across keys 1-15 for 1.5 seconds; song lights then return. Zero is dark and full level lights all 15 keys.
- Each level encoder's LED brightness follows its level continuously.

## Transport

- Play/Pause: start selected song; pause; arm quantized resume.
- Play/Pause again while resume is pending: cancel and remain paused.
- Loop: Stop, reset song to frame zero, stop click and MIDI clock.
- Hold SHIFT/CHOMPI and press the first black key (C#): toggle MIDI DIN output.
- C# LED: green when the selected entry's MIDI preference is enabled, red when disabled.
- While stopped, hold SHIFT/CHOMPI and press the second black key (D#) to cycle advance mode.
- D# LED: red for wait, blue for auto, yellow for loop. The mode is saved to `set.json`.
- While fully stopped, hold SHIFT/CHOMPI and press lower F# for hundreds, lower G# for tens, lower A# for ones, upper C# for tenths, or upper D# for hundredths. Release the chord to enter the editor. While active, press any of the five selectors directly to view or edit another position; no additional SHIFT press is needed.
- The selected position and saved digit light white. White keys 1-9 enter digits 1-9; white key 10 enters 0. A changed digit saves immediately and exits after success; choosing the existing digit exits without rewriting. Press SHIFT/CHOMPI to exit without changing BPM.
- BPM editing locks song/page selection, encoders, transport, MIDI, and advance controls. The page switch follows its physical position when the editor exits.
- BPM edits are limited to 30.00-300.00. Out-of-range choices are rejected. A known-safe save failure keeps the editor open for retry or SHIFT exit; an uncertain card commit lights the red status and requires reload/recovery.
- BPM changes affect click, beat feedback, MIDI clock, and quantized timing on the next playback start. They do not change WAV playback speed or pitch.
- Toggle while stopped: choose song page 1 or 2.
- White keys while stopped: select one of 15 entries on the current page.
- Play/Pause LED: blinking green while playing; solid green while paused or waiting for resume.
- Loop LED: solid orange while stopped; dark otherwise. Both transport LEDs are dark during loading or errors.

## Timing

- WAV begins at frame zero. Click and MIDI Start wait for the configured first downbeat.
- After the first downbeat, pause freezes WAV position but click and MIDI Clock continue.
- Resume waits for the same musical position in a later bar.
- `auto` starts the next song; `wait` selects it and waits for Play; `loop` replays the current song. Auto and wait stop at the last entry.
- The soft metronome tone is higher on the first beat of each bar.
- Encoder 5's left LED flashes on bar downbeats; its right LED flashes on other beats. These flashes remain active when click audio is muted and continue during pause after the first downbeat.
- MIDI output can be disabled without stopping playback, click, or internal timing. Re-enable during playback starts the receiver at the next bar downbeat.

## Errors

A red status light means set/card, audio, MIDI, or playback failure. Stop before retrying. Aux monitoring remains active. Back up the set and WAV files independently of the firmware's mixer-level settings.

The status light turns amber for clipping and missing/unsaved/failed level settings; these settings warnings do not invalidate the song set. Level bars do not cover this light.

If any WAV fails startup validation, its song slot blinks red and other song lights go dark. The whole set waits for that file to be fixed. A file-open failure on Play also blinks the affected slot red. Select the other page while stopped if necessary. WAVs must have consistent RIFF/chunk sizes as well as supported audio format.

After SD-card removal or replacement, press Play while stopped to reload the set and levels. This reload does not start playback; press Play again to start the selected song.

Existing BPM values with extra decimal places are read at the nearest hundredth, with decimal half-up rounding; for example, 133.456 is played and displayed as 133.46 without rewriting the card. A changed tempo is saved with two decimal places, such as `125.50` or `30.00`.
