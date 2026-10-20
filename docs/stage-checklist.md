# Stage Checklist

## Before the set

- Prepare mono/stereo 48 kHz, signed 16-bit PCM WAV files; keep external backups.
- Validate every entry in `BACKING-TRACKS/set.json`, including BPM, meter, first-downbeat offset, and advance mode.
- Test the exact SD card and all tracks on the target CHOMPI. Preflight the full set while stopped.
- Confirm the physical aux input pair, left/right orientation, and whether any mic path is present.
- Record headphone and main outputs separately. Confirm click reaches headphones and is absent from the digital/main route; measure analog crosstalk against a click-muted baseline.
- Test the intended MIDI receiver's Start, Stop, Continue, and quantized-resume behavior.
- Measure DIN start-bit timing under heavy SD reads and LED servicing; clock lateness/jitter must remain within 1 ms.
- Run the full set and a long soak; investigate every underrun, clipping indication, storage error, or watchdog reset.

## Before power-up

- Confirm the intended `BACKING-TRACKS/set.json`, WAV paths, settings backup, audio cabling, and MIDI DIN cable.
- Confirm no other firmware `.bin` is on the update card when installing firmware; do not flash the bootloader.
- After boot, verify the selected first song and required output levels before pressing Play.

A firmware build and host-test pass do not replace these hardware checks.
