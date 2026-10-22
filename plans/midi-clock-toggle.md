# Quick MIDI clock toggle and per-song defaults

## Problem and goal

Allow a performer to enable or disable MIDI DIN clock quickly without stopping
the backing track, changing the headphone click, or disturbing musical timing.
Allow each set entry to specify its initial MIDI output state.

This is a plan only. Do not change firmware until approved.

## Confirmed behavior

- Hold SHIFT/CHOMPI and press the first black key, C#, to toggle MIDI output.
  No long-press delay or encoder gesture is required.
- C# is always green when enabled and red when disabled. Leave existing beat,
  level, transport, song-selection, and error indications unchanged.
- OFF suppresses Clock (`F8`), Start (`FA`), Stop (`FC`), and Continue (`FB`),
  after sending one Stop if an external MIDI session has started.
- ON during playback waits for the next bar downbeat, then sends Start and
  resumes clock. This restarts a receiver; it does not seek to the backing
  track's arrangement position.
- Internal timing, WAV playback, aux monitoring, headphone click, beat LEDs,
  and quantized audio resume continue independently of MIDI output.
- An optional per-entry JSON boolean, `midi_clock`, supplies the initial state.
  Omission means `true`, preserving behavior for existing sets.
- A live override belongs to the selected set entry. Keep it through Stop/Play
  and pause/resume. Reset it from JSON when a different entry is selected or
  loaded, including automatic advancement, or when the set is reloaded.
- Overrides are RAM-only. Never rewrite the set or add this flag to saved mixer
  levels. After reboot, read the first entry's JSON default again; there is no
  persistent global override.

## Recommended approach

### Set format and initialization

Extend version 1 with an optional `midi_clock` field on each song:

```json
{
  "file": "TRACKS/01-intro.wav",
  "bpm": 120,
  "meter": [4, 4],
  "first_downbeat_ms": 0,
  "advance": "auto",
  "midi_clock": false
}
```

Initialize the parser's per-song default to true explicitly; zero-initialized
storage must not accidentally turn older sets off. Accept only JSON booleans.
Reject duplicate `midi_clock`, null, numeric values, strings, and all other
invalid values through the existing invalid-set/error indication. Preserve
strict rejection of other unknown fields and all existing required fields.
Version 1 remains backward compatible for new firmware reading older sets;
older firmware rejects sets containing the new field.

Apply defaults when the selected entry identity changes, not every time its
WAV is opened. Entries referencing the same WAV still have independent defaults.
Selecting the same entry again or changing pages without changing selection
must not discard an override. Set reload deliberately discards it.

### Shortcut and LED

Verify the physical switch and LED mapping for SHIFT/CHOMPI and the lowest C#
before wiring constants. The existing hardware enum exposes KEY_16 through
KEY_26, but the current application does not identify these two physical keys.
Do not infer their mapping solely from numeric order.

Use the existing debounced control scan in the audio callback. Toggle on C#'s
press edge only when SHIFT/CHOMPI is already held. Holding both keys must not
repeat; pressing SHIFT while C# is already held must not toggle. Track key
history even when the modifier is absent, and initialize it before audio starts
so a held boot key does not trigger a toggle.

Allow the shortcut while stopped, loading, playing, paused, pending resume,
or in an error state, provided a valid entry is selected. During loading it
updates that entry's pending preference without initiating MIDI early. With
no valid entry, leave it inert and retain the existing error indication.
Preserve the existing power/shipping/bootloader gestures.

Render C# independently of the white-key level/song overlays, using a published
snapshot of the effective enabled preference. Green means enabled or armed,
not proof that MIDI bytes are currently being sent. Silent lead-in, stopped
transport, and next-downbeat waiting can therefore all show green. Do not add
blinking or change the song encoder's beat LEDs.

### MIDI output state and timing

Keep the shared sample-derived timing engine running normally. Gate MIDI
emission, not timing advancement or click generation. Distinguish the enabled
preference, an armed next-downbeat restart, and whether an external MIDI session
has actually started.

Extract only the small MIDI emission/transition policy needed for host testing,
with a fake byte sink. Reuse it for all MIDI send sites rather than introducing
independent conditions for each transport path.

| Situation | Planned MIDI behavior |
|---|---|
| Enabled song starts normally | Preserve first-downbeat Start followed by 24-PPQN clock. No output during lead-in. |
| Disabled song starts | No MIDI bytes; internal timing and click work normally. |
| Toggle OFF after MIDI Start | Send one Stop, clear external-session and restart state, then silence output. Do not stop audio or timing. |
| Toggle OFF before MIDI Start | Silence output and cancel any pending restart; no unnecessary Stop. |
| Toggle ON while stopped or in lead-in | Arm normal first-downbeat Start; do not send immediately. |
| Toggle ON while playing after the first downbeat | Emit nothing until the next bar downbeat; send Start before clock, without resetting internal phase. |
| Toggle ON while paused or pending resume | Arm restart, but do not start the external device while WAV playback is paused. After actual audio resume, start on the next bar downbeat at or after that boundary. |
| Pause with an established MIDI session | Preserve existing Stop-on-pause and continuing clock while enabled. |
| Resume with an established MIDI session | Preserve Continue at the actual quantized audio-resume boundary. |
| Resume before a newly armed MIDI session has started | Do not send orphan Continue; retain the armed downbeat Start instead. |
| Stop, EOF, or failure before an armed restart | Cancel the armed restart. No late Start may escape after transport ends. |
| Stop, EOF, or failure after MIDI Start | Preserve existing Stop/error handling when transmission is available, unless already disabled and stopped. |
| Next entry or successful set reload | Clear previous MIDI session/restart state and apply the new entry's JSON default. |

Process a toggle once per audio block before sample events. An OFF request
observed on a boundary wins over a Start or clock due in that block. A newly
armed ON can use a downbeat reached after the request is processed.

Audit every current send path: first downbeat, clock ticks, pause, quantized
resume, explicit Stop, EOF, underrun, and foreground-triggered transport error.
Intentional suppression is not a transmit failure. Real failed transmissions,
including the one-shot Stop, must retain explicit MIDI error reporting and
the existing fail-closed behavior where applicable.

Start and a phase-aligned clock can fall on the same sample at re-enable.
Keep Start before the first transmitted clock without busy-waiting in the ISR,
resetting the timebase, bursting missed ticks, or treating that planned pair as
an unexpected TX-empty failure. Use only bounded serialization if needed;
verify the resulting DIN timing against the existing 1 ms jitter limit.

Keep transition ownership in the audio callback. Foreground selection, reload,
and auto-advance must publish entry-default changes safely so a new selection
cannot briefly inherit the previous entry's override or pending Start. Use the
existing atomic/publication patterns; do not add SD work to the callback.

## Affected files/components

- [BackingTypes.h](../code/src/BackingTypes.h): per-song MIDI default.
- [BackingSet.h](../code/src/BackingSet.h): optional boolean parsing/defaults.
- [backing_tracks_main.cpp](../code/src/backing_tracks_main.cpp): controls, entry
  initialization, all MIDI sends, audio/foreground handoff, C# LED.
- [BackingTiming.h](../code/src/BackingTiming.h): reuse existing downbeat events;
  preserve phase and internal timing when MIDI is disabled.
- New `code/src/BackingMidi.h`: small, hardware-independent MIDI policy shared
  by the application and tests, only if needed to expose the actual send logic.
- [BackingFeedback.h](../code/src/BackingFeedback.h): testable C# feedback if
  consistent with existing helpers.
- [hardware.h](../code/src/hardware.h) and
  [temp_led_stuff.h](../code/src/temp_led_stuff.h): mapping references; no driver
  changes expected.
- [backing_format_test.cpp](../code/tests/backing_format_test.cpp),
  [backing_timing_test.cpp](../code/tests/backing_timing_test.cpp),
  [backing_transport_test.cpp](../code/tests/backing_transport_test.cpp), and
  [test Makefile](../code/tests/Makefile): parser, timing, UI, and MIDI policy
  coverage. Add a focused MIDI test target if using the extracted policy.
- [README.md](../README.md), [midi.md](../midi.md),
  [specification.md](../docs/specification.md),
  [control-card.md](../docs/control-card.md),
  [stage-checklist.md](../docs/stage-checklist.md), and
  [example set](../docs/examples/set.json): configuration and performer guidance.
- Mixer-level persistence and vendored libraries require no changes.

## Step-by-step tasks, in order

1. Verify physical modifier/C# switch mapping and C# RGB LED on target hardware.
2. Add the optional song field, explicit true default, strict validation, and
   parser tests. Add examples showing enabled and disabled entries.
3. Implement and host-test the bounded MIDI policy, including next-downbeat
   restart, all transport transitions, and byte ordering/failure handling.
4. Wire entry-default publication for boot, selection, reload, and both advance
   modes. Preserve overrides on same-entry Stop/Play and WAV reopen.
5. Wire modifier-plus-key edge detection, boot-held protection, and continuous
   red/green C# feedback without changing other controls or LEDs.
6. Update the directly affected specification, MIDI guide, control card, README,
   examples, and hardware checklist to describe the new contract.
7. Run focused host tests, then the existing host suite. Build with the project
   build workflow and report SRAM, SRAM_EXEC, and SDRAM usage.
8. Verify physical DIN bytes, timing, shortcut, LED mapping, receiver behavior,
   and unaffected audio under SD/LED load before marking the feature complete.

## Risks, open questions, memory/CPU/timing considerations

- Physical key/LED mapping remains a hardware verification prerequisite.
- The paused-state restart rule above is a proposed edge-case detail for plan
  approval: defer Start until audio has resumed, rather than starting a receiver
  against frozen WAV playback.
- A receiver restarted at a later bar begins its own arrangement from the start;
  Song Position Pointer and external seeking remain out of scope.
- C# green represents enabled/armed preference, not active wire traffic or
  receiver synchronization. Red is a setting, not a fault indicator.
- MIDI gating must never freeze bar phase, click, or quantized resume.
- UART runs at 31.25 kbaud; one real-time byte takes approximately 320 us.
  Planned Start/clock serialization must remain bounded and preserve the
  existing error policy for genuinely unavailable transmission.
- Budget one boolean per entry plus a small bounded runtime state and control
  history; confirm actual struct padding and linker usage rather than assuming
  the exact byte increase. No new audio buffers, heap allocation, SD writes,
  polling loops, or unbounded callback work.

## Testing and validation

### Host tests

- Parse omitted/true/false fields, mixed defaults, repeated WAV entries, duplicate
  fields, wrong JSON types, and malformed input. Preserve all older set tests,
  required fields, unknown-field rejection, and set size/count limits.
- Assert exactly one toggle per modified C# press; no toggle on bare C#, modifier
  alone, modifier pressed after C#, held keys, or held boot keys.
- Compare internal timing, click, consumed audio position, and quantized resume
  against an always-enabled reference while toggling MIDI repeatedly.
- Use the production MIDI policy with a capturing fake sink. Assert exact byte
  sequences and sample positions for every transition in the table, including
  no bytes while OFF after the optional one-shot Stop.
- At 120 BPM/4/4, verify 24 clocks per quarter note and 96 per full running bar
  after restart. Also test fractional BPM, /8 meters, lead-in, cancellation,
  requests immediately around a downbeat, and Start-before-clock ordering.
- Cover re-enable during pause/pending resume, pause before a pending Start,
  resume coincident with a downbeat, Stop/EOF/error before restart, and injected
  failed Start/Clock/Stop/Continue writes. Never catch up with a stale burst.
- Verify override retention through same-entry Stop/Play and reopening, reset
  on different-entry selection including identical WAV paths, auto/wait advance,
  set reload and reboot. Page-only changes do not reset it.
- Verify red/green preference rendering, including armed ON, and preservation
  of white-key overlays, beat flashes, and latched error feedback.

### Build and hardware acceptance

- Run the existing host suite and firmware build; record all three memory
  regions. A compile alone is not evidence of correct MIDI behavior.
- Confirm the physical SHIFT/CHOMPI+C# gesture and continuous C# red/green state
  across boot, selection, playback, pause, and level overlays.
- Capture DIN start bits under sustained SD reads and LED updates. Confirm the
  1 ms maximum timing deviation, correct byte order, one Stop on disable when
  needed, no OFF traffic, and next-bar Start with phase-correct clock on enable.
- Test the intended receiver's restart behavior explicitly; do not claim
  arrangement synchronization.
- Record main/headphone outputs across toggles. Confirm uninterrupted WAV/aux,
  unchanged click phase/routing, correct quantized resume, and no underruns.
- Run enabled/disabled entries through auto and wait transitions and a long
  soak. Confirm no pending Start leaks across transport or entry changes.
