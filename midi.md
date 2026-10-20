# CHOMPI-PIPES — MIDI Implementation

All messages use a fixed MIDI channel: **1**.
Transports: 5-pin DIN (UART) and USB MIDI, both directions.
No other message types (pitch bend, aftertouch, SysEx, clock) and no other CC numbers are processed or sent.

Rotary CC values address the same 33 positions as the encoders:
`position = (value * 32 + 63) / 127`. Outgoing parameter CCs use
`value = (position * 127 + 16) / 32`, so all 33 positions round-trip.
CC values 0, 64 and 127 select positions 0, 16 and 32. Existing automation
keeps the same endpoints and approximate proportions, quantized to this grid.

## MIDI In

| Function | Type / CC | Value span |
|---|---|---|
| Note On | Note On (0x90) | Note 0–127; velocity 1–127 (scaled to 0.0–1.0). Velocity 0 = Note Off |
| Note Off | Note Off (0x80) | Note 0–127; velocity ignored |
| Vibrato depth (Modulation) | CC 1 | 0–32 positions across 0–50 cents peak |
| Pipe 1 level | CC 70 | 0–127 (mapped to 0–32 steps, rounded) |
| Pipe 2 level | CC 71 | 0–127 (mapped to 0–32 steps, rounded) |
| Pipe 3 level | CC 72 | 0–127 (mapped to 0–32 steps, rounded) |
| Pipe 4 level | CC 73 | 0–127 (mapped to 0–32 steps, rounded) |
| Pipe 1 Character | CC 74 | 0–32 positions, clean to fully aged |
| Pipe 2 Character | CC 75 | 0–32 positions, clean to fully aged |
| Pipe 3 Character | CC 76 | 0–32 positions, clean to fully aged |
| Pipe 4 Character | CC 77 | 0–32 positions, clean to fully aged |
| Pipe 1–4 Attack | CC 78–81 | 0–127 (mapped to 0–32 steps, rounded) |
| Pipe 1–4 Release | CC 82–85 | 0–127 (mapped to 0–32 steps, rounded) |
| Pipe 1–4 Mute | CC 86–89 | 0–63 = unmuted, 64–127 = muted |
| Pipe 1–4 Length | CC 90–93 | 0–32 retained positions, snapped to −2/−1/0/+1/+2 octaves at position ranges 0–3, 4–11, 12–19, 20–27, 28–32 |
| Pipe 1–4 Volume mod | CC 94–97 | 0–127 (mapped to 0–32 steps, rounded) |
| Pipe 1–4 Character mod | CC 98–101 | 0–127 (mapped to 0–32 steps, rounded) |
| Pipe 1–4 Length mod | CC 102–105 | 0–127 (mapped to 0–32 steps, rounded) |
| Vibrato note-on delay | CC 106 | 0–32 positions across 0–5 s |
| Rotary amount | CC 107 | 0–127 (mapped to 0–32 steps, rounded) |
| Drive amount | CC 108 | 0–127 (mapped to 0–32 steps, rounded) |
| Drive tone | CC 109 | 0–32 positions across 0–100% |
| Reverb mix | CC 110 | 0–127 (mapped to 0–32 steps, rounded) |
| Reverb decay | CC 111 | 0–32 logarithmic positions across 0.2–20 s |
| Percussion on/off | CC 112 | 0–63 = off, 64–127 = on |
| Delay combined mix/feedback | CC 113 | 0–32 positions across the existing 0–100% curve; position 16 is the 50% pivot |
| Delay time | CC 114 | 0–32 positions across 40–500 ms |
| Drive modulation (global) | CC 115 | 0–127 (mapped to 0–32 steps, rounded) |
| Delay modulation (global) | CC 116 | 0–127 (mapped to 0–32 steps, rounded) |
| Preset slot load | Program Change 1–15 | Loads SD preset slot 1–15 from the RAM cache (click-free, like the local gesture). Empty slots do nothing; other program numbers are ignored |

## MIDI Out

| Function | Type / CC | Value span |
|---|---|---|
| Note On | Note On (0x90) | Note 0–127; velocity fixed at 100 |
| Note Off | Note Off (0x80) | Note 0–127; velocity fixed at 0 |
| Vibrato depth (Modulation) | CC 1 | 0–127 (position 0–32 scaled by the shared outgoing formula) |
| Pipe 1 level | CC 70 | 0–127 (position 0–32 scaled by the shared outgoing formula) |
| Pipe 2 level | CC 71 | 0–127 (position 0–32 scaled by the shared outgoing formula) |
| Pipe 3 level | CC 72 | 0–127 (position 0–32 scaled by the shared outgoing formula) |
| Pipe 4 level | CC 73 | 0–127 (position 0–32 scaled by the shared outgoing formula) |
| Pipe 1 Character | CC 74 | 0–127 (position 0–32 scaled by the shared outgoing formula) |
| Pipe 2 Character | CC 75 | 0–127 (position 0–32 scaled by the shared outgoing formula) |
| Pipe 3 Character | CC 76 | 0–127 (position 0–32 scaled by the shared outgoing formula) |
| Pipe 4 Character | CC 77 | 0–127 (position 0–32 scaled by the shared outgoing formula) |

Notes:

- Incoming messages on channels other than 1 are discarded.
- Every preset-saved rotary parameter has an incoming CC (CC 1 and 70–116). Not preset-saved and therefore not CC-controllable: master volume, fine tune, octave shift, latch state, page selections and wheel mode.
- Incoming CCs set their values silently: they never trigger the white-key LED value bars (a live CC stream would otherwise own the LEDs). The bars follow local controls only.
- CCs 78–116 are receive-only. CC out remains limited to CC 1 and 70–77, sent when vibrato depth, pipe levels or Character change locally and when presets resynchronize them. Notes out are sent for local key presses/releases.
- Character = tonewheel color; position 0 = clean sine and position 32 = fully aged.
- Length CC input first selects a position on the shared grid, then applies the same octave snapping as the local encoder. Anchors 0/32/64/96/127 select -2/-1/0/+1/+2 octaves.
- Playback mode (hold PLAY for 2 s) ignores all MIDI In while the mode is active — notes, CCs and Program Change preset loads are all dropped; MIDI Out produces nothing (no key or encoder events fire).
