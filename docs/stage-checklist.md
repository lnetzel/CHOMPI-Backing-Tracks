# Stage Checklist

## Before the set

- Prepare mono/stereo 48 kHz, signed 16-bit PCM WAV files; keep external backups.
- Validate every entry in `BACKING-TRACKS/set.json`, including BPM, meter, first-downbeat offset, and advance mode.
- Verify SHIFT+D# cycles wait/auto/loop with red/blue/yellow feedback while stopped, persists only the selected entry's `advance` value, and loops the full entry at EOF.
- Verify the five BPM selector and LED mappings physically: lower F#/G#/A# and upper C#/D# must select hundreds/tens/ones/tenths/hundredths without colliding with lower C#/D# shortcuts.
- Verify readout for 120.00 and 075.00, then immediate 120 -> 125 -> 125.50 -> 125.54 edits. Confirm unchanged-digit inspection and SHIFT exit do not write, and each changed digit saves before automatic exit.
- Verify 030.00 and 300.00 boundaries, rejection of out-of-range intermediate values, preservation of the other four digits, white-key 10 as zero, and no WAV time-stretching.
- Load 133.456 and equivalent exponent forms. Confirm display and timing use 133.46 while inspection/reboot leave the original JSON token untouched; a changed digit writes two-decimal JSON.
- While latched or saving, verify all encoders, song keys, Play, Loop, MIDI, and advance controls are inert while aux monitoring continues. Move the page switch during editing and verify it reconciles only on exit without changing selection or write target.
- Measure digit-press-to-acknowledgement under queued settings/advance work, slow/write-protected media, card removal, and rename failures. Require completion within five seconds, no automatic retries, exact RAM rollback, safe backup recovery, and no unlocking with I/O in flight.
- Test the exact SD card and all tracks on the target CHOMPI. Preflight the full set while stopped.
- Confirm the physical aux input pair, left/right orientation, and whether any mic path is present.
- Record headphone and main outputs separately. Confirm click reaches headphones and is absent from the digital/main route; measure analog crosstalk against a click-muted baseline.
- Test the intended MIDI receiver's Start, Stop, Continue, and quantized-resume behavior.
- Verify on the target unit that SHIFT/CHOMPI plus the first black key (C#) toggles MIDI output and that C# shows green/red preference through boot, selection, playback, pause, and level overlays.
- Test enabled/disabled `midi_clock` entries through auto/wait advances, reload, and reboot; confirm live overrides are RAM-only and entry-specific.
- Measure DIN start-bit timing under heavy SD reads and LED servicing; clock lateness/jitter must remain within 1 ms.
- Run the full set and a long soak; investigate every underrun, clipping indication, storage error, or watchdog reset.

## Before power-up

- Confirm the intended `BACKING-TRACKS/set.json`, WAV paths, settings backup, audio cabling, and MIDI DIN cable.
- Confirm no other firmware `.bin` is on the update card when installing firmware; do not flash the bootloader.
- After boot, verify the selected first song and required output levels before pressing Play.

A firmware build and host-test pass do not replace these hardware checks.
