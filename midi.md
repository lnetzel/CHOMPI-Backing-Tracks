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

The audio callback writes one byte to the configured USART1 transmit register only when TX-empty is set; it does not call the foreground polling or DMA transmit APIs. A missed transmit slot is latched as an error and is not replayed as a burst. DIN start-bit timing and jitter under SD-card and LED load have not yet been measured on hardware; the acceptance limit is 1 ms.
