# CHOMPI stage backing-track firmware

## Problem and approach

Create a dedicated live-performance backing-track player for CHOMPI hardware in
a separate, independently maintained repository. Use CHOMPI-Pipes as the source
and hardware/build reference, retaining its history and applicable attribution.
Leave the existing checkout, its untracked plans, remotes, and firmware unchanged.

Implement a small stereo mixer, reliable SD transport, sample-derived musical
timing, headphone-only metronome, MIDI DIN clock output, and an SD-configured set.
Prove physical audio routing and MIDI timing first; do not assume a successful
compile proves stage readiness.

This artifact is a plan only. Project name, destination folder, GitHub visibility,
repository creation, and publishing are not yet authorized or decided.

## Confirmed scope

- Up to 30 ordered entries in one active set, presented as two pages of 15 songs.
  Entries may reference the same WAV more than once with different metadata.
- Prepared mono or stereo, 48 kHz, signed 16-bit PCM WAV files. No resampling,
  compressed audio, RF64, tempo maps, recording, organ, effects, or gapless promise.
- Per-entry constant BPM, meter, first-downbeat offset, and `auto` or `wait` ending.
  Use BPM 30-300; meters 2/4 through 12/4 and 3/8 through 12/8. BPM always counts
  quarter notes; in /8 meters the click counts eighth notes, with a bar accent.
- Track, line-in, click, headphone-output, and main-output levels are independently
  adjustable and retained on SD. Save while stopped, never while playing, paused,
  or waiting for quantized resume. Unsaved changes may be lost on sudden power loss.
- No automatic playback after boot. Always select the first entry and wait for Play.
- Line-in remains audible through transport stops, pauses, and SD errors.
- MIDI output is the physical DIN port. USB MIDI, MIDI input control, tap tempo,
  count-in, loop playback, selectable multiple sets, and on-device metadata editing
  are outside v1.

## Evidence from the reference checkout

- `code/src/hardware.h` configures synchronized SAI1/SAI2 stereo codecs at 48 kHz,
  with 24-frame audio blocks. Preserve this initialization and vendored libDaisy.
- `code/src/OrganEngine.h` writes headphones to output channels 0/1 and main to
  2/3. The existing application does not establish the line-in input mapping:
  identify it on hardware, including stereo orientation and unwanted mic paths.
- `code/src/SdPlayer.h` provides staged foreground FatFS reads and ISR ring-buffer
  consumption, but is coupled to recorder filenames, canonical 44-byte WAV headers,
  newest-first selection, wrapping, skipping errors, and read-ahead EOF transitions.
  Reuse its proven buffering pattern, not those product policies.
- `code/src/SdRecorder.h` supplies reusable pure ring-buffer groundwork. Extract only
  the required pieces in the new repository; do not keep the recorder dependency.
- `code/src/PresetStorage.h` demonstrates DMA-accessible global file/buffer storage
  and foreground SD servicing. Its organ schema and replacement-write algorithm
  are not a persistence contract for this project.
- `code/src/MidiHandler.h` depends on OrganEngine and starts queued DMA messages
  from the main loop. That path cannot guarantee clock timing during FatFS stalls.
  Its comments also warn against starting HAL UART DMA from an ISR.
- `code/src/Makefile`, `chompi_sram.lds`, the CHOMPI bootloader, native-test layout,
  encoder helpers, and LED helpers are suitable baseline components.
- `plans/new-chompi-firmware-repository.md` is the existing repository-seeding
  reference. Do not rewrite it or other files in CHOMPI-Pipes for this product.

## Audio signal flow

For each stereo channel, with smoothed gains:

    base_mix = track * track_gain + line_in * line_gain
    main_out = protect(base_mix * main_gain)
    phones   = protect((base_mix + click * click_gain) * phones_gain)

Duplicate mono WAV samples into L/R; preserve stereo WAV and line-in orientation.
The click has no path into the main bus, including during gain changes, transitions,
errors, and startup. Use a deterministic short synthesized click, accented on each
bar downbeat, with no SD reads for click generation.

Use unity-or-lower source/output gains, documented summing headroom, output safety
clamping, and clip feedback; do not normalize tracks dynamically. Smooth gain
changes and use short transport fades without inserting/deleting timeline frames.
Choose conservative documented first-boot defaults, with click initially muted
until the operator sets its level. Missing saved settings must be indicated.

## Transport and timing contract

- **Play from stopped:** validate/open and prebuffer the selected WAV; start audio
  and its timeline together only when ready. Stop resets that entry to frame zero.
- **Pause:** freeze the consumed WAV frame position, not the read-ahead file cursor.
  After the first downbeat has occurred, the click and 24-PPQN MIDI clock continue.
  Send MIDI Stop (`FC`) while leaving clock ticks (`F8`) running.
- **Resume:** Play arms a pending state. Resume the frozen WAV at the next occurrence
  of its saved musical position modulo the entire bar, including sub-beat position.
  Keep the running click/clock phase unchanged; send MIDI Continue (`FB`) at the
  actual resume boundary. This may wait almost a full bar. Another Play cancels
  pending resume back to paused; Stop cancels it and resets the song.
- **Silent lead-in:** `first_downbeat_ms >= 0` marks the first bar downbeat in the
  WAV. WAV audio begins at frame zero, but click and MIDI clock remain off until
  this offset. Send MIDI Start (`FA`) there, before the first clock tick.
  Pausing before that boundary simply freezes/resumes the remaining lead-in,
  without phase matching or MIDI Continue. Offset zero is the normal case.
- **Stop:** stop click/clock, send MIDI Stop if the MIDI transport had started,
  reset current playback, and leave line-in monitoring active.
- **Natural EOF:** act on the final consumed sample, not when foreground read-ahead
  reaches EOF. Drain the ring before finishing. `wait` stops timing, selects the
  next entry, and waits for Play; `auto` loads/prebuffers/starts the next entry.
  Each new song starts a fresh timing origin, including its own lead-in.
  A transition gap is allowed; click/clock are stopped during it. The last entry
  always stops without wrapping and remains selected.
- **Failures:** fail closed on the affected entry; never silently skip a song or
  start a different one. Stop track/click/clock and indicate the error, preserving
  line-in. Recovery/reload is explicit while stopped.

Use one audio-sample-derived fractional/fixed-point timebase for consumed position,
bar phase, metronome, and 24-PPQN clock. Avoid rounded integer beat lengths and
main-loop millisecond scheduling. MIDI receivers must support Start/Stop/Continue;
phase-compatible continuation is not a guarantee of every receiver's musical
arrangement position. Song Position Pointer and external-device seeking are out
of scope.

## Controls and feedback

- Top encoders 1-4: track, line-in, click, headphone level, respectively.
- Existing volume encoder: main-output level.
- Remaining encoder: select previous/next entry, clamped at the set boundaries.
- Play/Pause key: play, pause, or arm/cancel quantized resume.
- Loop key: Stop; no loop function.
- Fifteen white keys select an entry in the current page. The toggle chooses
  entries 1-15 or 16-30. Empty keys remain dark/inert.
- Song/page selection is accepted only while stopped, not paused, playing, or
  pending resume. Changing the toggle while running takes effect only once stopped;
  never interrupt playback or silently queue a different song.
- Encoder pushes do not introduce hidden parameter pages in v1.
- LEDs distinguish selected/available entries, playing, paused, pending resume,
  stopped/loading, beat/downbeat, clipping, SD/config/audio errors, and pending or
  failed persistence. Level bars are temporary and must not hide latched errors.
- Preserve the hardware's power/shipping/bootloader gestures and reserve their
  button combinations rather than reassigning them.

## SD configuration and persistence

Use a project-specific directory, represented here as `BACKING/`, never `PIPES/`.
Its final name follows the new product naming decision.

Example active set, edited on a computer:

```json
{
  "version": 1,
  "songs": [
    {
      "file": "TRACKS/01-intro.wav",
      "bpm": 120,
      "meter": [4, 4],
      "first_downbeat_ms": 0,
      "advance": "auto"
    },
    {
      "file": "TRACKS/02-song.wav",
      "bpm": 98.5,
      "meter": [6, 8],
      "first_downbeat_ms": 350,
      "advance": "wait"
    }
  ]
}
```

- `BACKING/set.json` is read-only to the firmware. It preserves tempo/order across
  power cycles; runtime volume saves never rewrite the user's set metadata.
- Validate version, required fields, finite values, ranges, entry count, string and
  config size bounds, meter, advance enum, and relative paths. Reject traversal,
  absolute paths, duplicate schema fields, empty sets, and unknown schema fields.
  Allow repeated song entries. Use coreJSON and bounded application-owned storage.
- Document concrete limits: 30 entries, 127-byte relative WAV paths, 16 KiB set file;
  no runtime heap use in audio/timing paths. Reject overflow rather than truncating.
- Implement a bounded RIFF chunk walker for PCM WAV, accepting ordinary extra
  chunks, padding, and extended PCM `fmt ` chunks. Validate file/chunk bounds,
  sample rate, channels, block alignment, byte rate, whole frames, nonempty data,
  and downbeat offset inside the audio duration. Reject unsupported formats and
  truncated reads explicitly. Preflight entries while stopped before stage use.
- Store five level steps separately in two versioned, generation-numbered,
  checksummed settings slots. Write only the inactive slot, sync/close and verify
  before accepting it; choose the newest valid slot at boot. Never delete the only
  valid copy first. This improves recovery but is not a claim of power-fail atomic
  FAT updates. Keep external backups of the set and tracks.
- Coalesce changes in RAM, save automatically once stopped, and indicate pending
  save until durable verification completes. If settings are absent/corrupt, use
  explicit first-boot defaults with visible status; report write/recovery failures.
- No SD card or invalid set disables backing playback, not line monitoring.
  Card replacement requires a stopped-state explicit reload; no hot autoplay.

## Implementation todos

### 1. Seeding and documenting the independent project

After destination details and authorization are supplied, create a separate normal
clone, retain ancestry, configure upstream/origin only there, and record the source
commit. Do not mirror-push branches/tags or publish without permission. Write the
confirmed specification and SD/control contract in the new project's documentation.

### 2. Establishing platform and routing baseline

Build the unchanged starting point with the pinned Arm GCC 10.3.1 workflow and run
the inherited host tests. Preserve bootloader, linker, watchdog, power handling,
codec synchronization, and control polling. On hardware identify line-in channels,
confirm independent output pairs, stereo orientation, and headphone-click isolation.
Do not proceed on guessed physical routing.

### 3. Implementing bounded set and WAV parsing

Add host-testable set/schema and RIFF parsers, validated entry models, and example
SD content. Keep FatFS operations foreground-only with globally allocated,
DMA-accessible FIL and bounce storage, never stack objects in DTCMRAM.

### 4. Implementing buffered transport and stereo mixing

Replace recorder-specific selection/looping with explicit transport states, a
caller-owned SDRAM SPSC ring, prebuffering, consumed-frame accounting, true EOF
draining, and visible underrun handling. Add stereo line input, independent gains,
headphone/main buses, fades, output protection, and line-in continuity on errors.

### 5. Implementing musical timing and MIDI output

Add a pure fractional timing core, accented click, offset handling, bar-phase
resume, and explicit MIDI transport events. Implement a bounded interrupt-safe
nonblocking UART output adapter scheduled from the audio/timer timebase, independent
of SD/main-loop latency. Inspect the vendored UART implementation before selecting
the exact low-level interface; do not start its existing HAL DMA path from the
audio ISR or silently fall back to foreground transmission. Keep changes local
unless a narrowly scoped new-repository driver change is actually required.
Report late/failed sends and stop timing on unrecoverable queue/TX failure; never
burst stale clock ticks to catch up. Prove the MIDI timing gate before UI polish.

### 6. Implementing controls, set progression, and saved levels

Wire the confirmed control mapping, two-page selection, state-specific gestures,
auto/wait progression, latched error feedback, and redundant settings storage.
Serialize SD ownership and prohibit settings writes during active transport.
Preserve reserved hardware gestures and make all recovery actions explicit.

### 7. Validating and preparing distribution

Replace organ-specific documentation, target/output naming, and obsolete tests
only in the new project. Retain relevant tests, licenses and third-party notices.
Supply WAV preparation instructions, annotated set/settings examples, a control
card, MIDI pause/Continue caveats, backup/recovery instructions, and a stage checklist.
Run focused host tests, complete relevant regression tests, firmware build, and
physical acceptance tests before claiming a release is ready.

## Dependencies

- Todo 2 requires todo 1.
- Todo 3 requires todo 1.
- Todo 4 requires todos 2 and 3.
- Todo 5 requires todos 2 and 4.
- Todo 6 requires todos 3, 4, and 5.
- Todo 7 requires all prior implementation todos.

## Verification and acceptance gates

- Parser tests: 1/15/16/30 entries; reject 0/31; malformed/oversize JSON, duplicate
  fields, unsafe/long paths, invalid numeric values/meters; RIFF unknown chunks,
  odd padding, mono/stereo, extended fmt, unsupported formats and truncation.
- Transport tests: prebuffering, pause without frame loss, Stop/restart, cancel
  pending resume, EOF after read-ahead, page boundaries, no wrap, auto/wait/final
  endings, missing next file, repeated paths, card removal and buffer underrun.
- Timing tests: fractional BPM, both meter denominators, bar/downbeat accents,
  nonzero offsets, pause before/at/after downbeat, repeated pauses, sub-beat saved
  positions, pending resume cancellation, and song changes. Compare long simulated
  runs to ideal fractional timing with at most one sample of algorithmic error.
  Do not interpret MCU oscillator accuracy as scheduling error.
- MIDI tests: 24 ticks per quarter independent of meter; FA/FC/FB ordering,
  paused F8 continuation, no clock in silent lead-in or after Stop/EOF, no stale
  tick bursts, explicit TX failures, and phase-matched actual resume.
- Mixer tests: channel-isolated stereo input, mono duplication, gain smoothing,
  zero/muted sources, clipping protection, independent output levels, and exact
  digital zero at both main outputs for click-only input across every state.
- Persistence tests: save/reboot all five levels, no active-transport writes,
  generation selection, corrupt/missing slots, failed/partial writes and syncs,
  newest-slot recovery, rapid edits during saves, and unchanged set/track files.
- Hardware tests: record actual headphone/main jacks to verify routing and click
  isolation; measure residual analog crosstalk against a click-muted baseline
  rather than claiming mathematically zero analog leakage.
- Measure DIN start-bit timing under heavy SD reads and normal LED/power servicing:
  clock lateness/jitter must stay within 1 ms of the sample-derived schedule with
  no dropped/duplicated ticks. If not, revise the sender before feature sign-off.
- Test real receiver Start/Stop/Continue behavior, quantized resume, different
  sample-prepared tracks, all 30 entries, and a full-set soak without underruns.
- Firmware builds must report SRAM, SRAM_EXEC and SDRAM usage in a table. Preserve
  linker limits and document measured headroom and peak audio callback CPU use;
  no release if watchdog, buffer, or timing stress tests fail.

## Open execution prerequisites

Only repository identity/location/visibility and creation/publishing authorization
remain for setup. Physical input mapping, analog crosstalk, SD latency and UART
timing are engineering validation gates, not established facts. No repository
files have been changed and no build/hardware validation is claimed by this plan.
