# backing-tracks firmware specification

## Product

`backing-tracks` is an independent CHOMPI-compatible live-performance WAV player and mixer. It is not an organ, synthesizer, or playable instrument. The audio sources are prepared SD-card WAV files, the CHOMPI auxiliary input, and a metronome routed to headphones only.

The repository is seeded from CHOMPI-Pipes commit `1e2bc676184d5ddb5330bc844c5471b1e372bbe8`. CHOMPI-Pipes remains the hardware/build reference and is not modified by this project. This is an independent project and does not imply an official CHOMPI Club release.

## Audio

- Play prepared mono or stereo 48 kHz, signed 16-bit PCM WAV files. Mono is duplicated to left/right; stereo channels and aux input orientation are preserved.
- Mix track and aux independently. Apply the main-output level only to their sum.
- Add a deterministic metronome click only to the headphone bus. Apply an independent headphone-output level to that bus.
- Smooth all level changes and transport fades. Clamp output samples to [-1, 1]. Do not apply automatic normalization or effects.
- Keep aux input audible during stop, pause, loading, missing-card, and file-error states.
- No organ generation, recording, resampling, compressed audio, RF64, tempo maps, looping, gapless transition promise, or sample-rate conversion.

The digital main-output pair must be exactly zero for click-only input. Physical analog crosstalk must be measured against a click-muted baseline before claiming isolation.

## Set and files

One active set contains 1-30 ordered song entries and is presented as two pages of 15. Entries may reference one WAV more than once with different metadata. The firmware never autoplays: entry 1 is selected after boot and waits for Play.

Store the read-only set at `BACKING-TRACKS/set.json` and WAV files under `BACKING-TRACKS/TRACKS/`. A set entry has exactly these fields: `file`, `bpm`, `meter`, `first_downbeat_ms`, and `advance`. Version 1 uses top-level `version: 1` and `songs` array. Reject invalid JSON, duplicate or unknown fields, empty sets, more than 30 entries, paths longer than 127 bytes, absolute paths, traversal, and set files larger than 16 KiB. Song paths are relative to `BACKING-TRACKS/`, for example `TRACKS/song.wav`.

Each song uses constant BPM 30-300, a meter of 2/4 through 12/4 or 3/8 through 12/8, a nonnegative first-downbeat offset in milliseconds, and `advance` set to `auto` or `wait`. BPM always counts quarter notes. In /8 meters, the audible click counts eighth notes and accents the bar downbeat.

Accept ordinary RIFF/WAVE chunk ordering, padding, unknown chunks, and extended PCM `fmt ` chunks. Require PCM signed 16-bit, 48 kHz, one or two channels, consistent byte rate and block alignment, complete nonempty frames, a bounded file/chunk layout, and a first downbeat inside the audio duration. Reject truncated or unsupported files explicitly. Preflight files while stopped.

## Transport and timing

- Play from stopped opens, validates, and prebuffers the selected WAV. Audio and its timeline start together when ready.
- Pause freezes the consumed audio frame, not the read-ahead cursor. Before the first downbeat, freeze the remaining lead-in. After it, click and 24-PPQN MIDI clock continue while MIDI Stop is sent.
- Play while paused arms resume at the next occurrence of the saved musical position modulo the full bar, including sub-beat position. Keep click/clock phase running; send Continue at the actual resume boundary. Play again cancels the pending resume. Stop cancels it and resets the entry.
- The first-downbeat offset gates click and MIDI Start; WAV playback begins at frame zero. Start is sent before the first clock tick. Stop halts timing and resets the entry.
- Determine EOF from the final consumed sample after ring drain. `wait` stops timing, selects the next entry, and waits for Play. `auto` preloads and starts the next entry with a new timing origin. The final entry never wraps. Transition gaps are allowed and timing is stopped during them.
- Errors fail closed for that entry. Never skip a song silently. Stop track/click/clock, preserve aux, and require explicit recovery while stopped.
- Derive audio position, bars, click, and 24-PPQN MIDI clock from one fractional sample timebase. Do not use rounded integer beat lengths or main-loop millisecond scheduling. Do not burst stale MIDI clock ticks to catch up.

MIDI output is the physical DIN port. Receivers must support Start, Stop, and Continue. Continue phase does not promise arrangement-position seeking on every receiver. USB MIDI, MIDI input control, Song Position Pointer, tap tempo, and count-in are out of scope.

## Controls

- Encoders 1-4: track, aux, click, and headphone levels.
- Encoder 5: previous/next entry, clamped to the active 15-entry page and set bounds.
- Encoder 6: main-output level.
- Play/Pause key: play, pause, or arm/cancel quantized resume.
- Loop key: Stop; it has no loop function.
- Fifteen white keys select entries on the current page. The hardware toggle selects page 1 or 2; empty keys are inert. Selection and page changes take effect only while stopped.
- Encoder pushes do not change pages or reveal hidden controls. Preserve power, shipping, and bootloader gestures.
- LEDs distinguish available/selected entries, playback states, beat/downbeat, clipping, SD/config/audio errors, and pending/failed settings writes. Temporary level bars must not hide latched errors.

## Settings and boot

Keep five level steps in two human-readable JSON settings slots, `BACKING-TRACKS/LEVEL0.json` and `BACKING-TRACKS/LEVEL1.json`. Each file contains `version`, `generation`, named `levels` (`track`, `line_in`, `click`, `headphones`, and `main`), and an eight-digit hexadecimal `checksum`. The checksum is CRC-32 over the version byte, the generation as four little-endian bytes, and the five level bytes in the order listed. Limit each settings file to 256 bytes. Write the inactive slot only, then sync, close, and verify it before accepting the save. Select the newest valid slot on boot. The files are readable for inspection; manual edits must have a matching checksum to be accepted. This is recovery-oriented, not a guarantee of power-fail-atomic FAT updates. Keep external backups of the set and audio files.

Coalesce level edits and save automatically only while stopped. Never write settings while playing, paused, or waiting for quantized resume. Missing/corrupt settings use documented first-boot levels and visible status. The click level boots muted. Missing SD/configuration disables WAV playback, not aux monitoring. Card replacement requires an explicit stopped-state reload; never hot-autoplay.

No heap allocation is permitted in audio or timing paths. FatFS operations run in the foreground only, using globally allocated DMA-accessible file and bounce buffers. Audio callback work is bounded and allocation-free.

## Acceptance gates

Host tests cover bounded set and RIFF parsing, all transport transitions, fractional timing, MIDI event ordering/failures, stereo mix routing, exact digital click isolation, settings recovery, and file/error handling. Firmware builds report SRAM, SRAM_EXEC, and SDRAM usage. Hardware tests verify real line-in channel mapping and orientation, both output pairs, headphone click isolation, card-stall playback, MIDI DIN jitter under SD/LED load (within 1 ms), receiver transport behavior, and a full-set soak without underruns. A successful compile alone is not stage-readiness evidence.
