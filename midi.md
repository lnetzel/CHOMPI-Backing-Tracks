# MIDI DIN Output

`backing-tracks` sends MIDI System Real-Time messages on the physical DIN output. These messages have no MIDI channel.

| Event | Byte | Timing |
|---|---:|---|
| Start | `FA` | At the configured first-downbeat sample, before the first clock tick |
| Clock | `F8` | 24 pulses per quarter note, derived from the audio sample timeline |
| Stop | `FC` | On pause, explicit Stop, natural end, or transport failure when transmission remains available |
| Continue | `FB` | At the actual quantized resume boundary after a post-downbeat pause |

A silent lead-in emits no Start or Clock. When paused before the first downbeat, playback freezes the remaining lead-in; resuming does not send Continue. After the first downbeat, audio pauses while Clock continues, and Stop is sent. Pressing Play arms resume at the next occurrence of the paused audio position modulo the full bar, including its sub-beat offset. Continue is sent at that boundary.

There is no Song Position Pointer or external-device seek. Continue does not guarantee that every receiver resumes at the same arrangement location. Receivers used on stage must support Start, Stop, and Continue.

Hold SHIFT/CHOMPI and press the first black key (C#) to toggle DIN output.
The C# LED is green when the selected entry's MIDI output preference is on and
red when it is off; this reports the preference, not current wire traffic.
Disabling sends one Stop if the external session has started, then suppresses
Clock, Start, Stop, and Continue. Enabling while stopped or before the first
downbeat waits for the normal first downbeat. Enabling during playback after
the first downbeat waits for the next bar downbeat, sends Start, then resumes
phase-aligned Clock. This restarts the receiver and does not seek it to the
backing track's arrangement position. A disabled MIDI output does not stop WAV
playback, internal timing, click, beat LEDs, or quantized audio resume.

Each set entry may include `"midi_clock": true` or `"midi_clock": false`.
Omission defaults to `true`. The live toggle override belongs to that entry,
survives Stop/Play and pause/resume, and resets when another entry is selected
or the set is reloaded. Overrides are RAM-only and are never written to mixer
settings; reboot reads the JSON default again.

While stopped, hold SHIFT/CHOMPI and press the second black key (D#) to cycle
the selected entry's `advance` mode: `wait` (red), `auto` (blue), then `loop`
(yellow). The selected mode is saved in `set.json` and takes effect on the next
Play. `loop` restarts that same WAV at EOF; `auto` and `wait` do not wrap past
the last entry.

The audio callback writes one byte to the configured USART1 transmit register only when TX-empty is set; it does not call the foreground polling or DMA transmit APIs. A missed transmit slot is latched as an error and is not replayed as a burst. If Start and a phase-aligned Clock coincide, Clock is serialized one sample after Start. DIN start-bit timing and jitter under SD-card and LED load have not yet been measured on hardware; the acceptance limit is 1 ms.
