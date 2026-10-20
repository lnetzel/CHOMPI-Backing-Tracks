# Encoder consistency: 32 ticks for every rotary parameter

## Status and goal

Plan only. Do not implement firmware, MIDI, preset, or test changes as part of
creating this document.

Make every active encoder-controlled parameter use **32 accepted encoder ticks
from its minimum control position to its maximum**, for both normal turns and
SHIFT + turns. MIDI must address the same positions, not merely rescale a
different local encoder range.

Terminology: 32 ticks means positions **0 through 32 inclusive**, i.e. 33
positions separated by 32 intervals. It does not mean 32 positions, 32 physical
quadrature edges, or one full mechanical revolution. One accepted increment
advances one position; no acceleration, wrapping, or parameter-specific coarse
or fine multiplier.

Confirmed pipe-length decision: spread the existing five octave settings over
the 32-tick control span. Keep octave snapping; do not introduce intermediate
pitches. Some ticks therefore advance the control position without changing the
sounding octave.

## Current implementation and scope

The source confirms the inconsistency described in the MIDI documentation.
Local routing is in [chompi_main.cpp](../code/src/chompi_main.cpp), engine ranges
and conversions in [OrganEngine.h](../code/src/OrganEngine.h), and MIDI conversion
and dispatch in [MidiCcMap.h](../code/src/MidiCcMap.h) and
[MidiHandler.h](../code/src/MidiHandler.h).

| Control/context | Current full-span ticks | Planned behavior |
|---|---:|---|
| Pipe encoders, Normal page 1: level | 32 | Keep 0..32; preserve squared gain curve |
| Pipe encoders, Normal page 2: Character | 64 | 0..32; preserve clean-to-fully-aged range |
| Pipe encoders, Normal page 3: length | 4 | 0..32 positions, snapped to five octaves |
| SHIFT + pipe turn, all three Normal pages | Same as corresponding page | Advance source position, then broadcast that position to all four pipes |
| Modulation Mode: attack and release | 32 each | Keep 0..32 and logarithmic 5 ms..10 s range |
| Modulation Mode: volume, Character, length modulation | 32 each | Keep 0..32 and existing modulation depths |
| Modulation Mode: global drive and delay modulation | 32 each | Keep 0..32, leftmost pipe encoder only |
| SHIFT + pipe turn in Modulation Mode | Same as destination | Keep per-pipe absolute broadcast; globals remain one shared value |
| Wheel, rotary mode, with or without SHIFT | 32 | Keep 0..32 and existing effect curve |
| Wheel, vibrato mode | 50 | 0..32 spanning 0..50 cents peak |
| SHIFT + wheel, vibrato mode: note-on delay | 50 | 0..32 spanning 0..5 s |
| Volume encoder page 1: master | 100 | 0..32 spanning gain 0..1 |
| SHIFT + volume page 1: fine tune | 100 | 0..32 spanning -50..+50 cents; center at 16 |
| Volume page 2: drive | 32 | Keep 0..32 and existing drive curve |
| SHIFT + volume page 2: tone | 100 | 0..32 spanning existing 0..100% tone curve |
| Volume page 3: combined delay mix/feedback | 100 | 0..32 spanning the entire existing combined control |
| SHIFT + volume page 3: delay time | 92 | 0..32 spanning 40..500 ms |
| Volume page 4: reverb mix | 32 | Keep 0..32, exact dry and wet endpoints |
| SHIFT + volume page 4: reverb decay | 100 | 0..32, logarithmic 0.2..20 s |
| Playback mode: unshifted volume turn, on any page | 100 | Same 0..32 master state as outside playback |

Playback continues to ignore SHIFT + volume turns and all other encoder turns;
the change must not activate controls that are currently intentionally inert.
Normal and Modulation Mode share the same wheel and volume parameter states.
Encoder presses/page cycling, mutes, percussion, preset selection, keyboard
octave gestures, latch, and destination selection are not rotary value spans:
preserve their existing discrete behavior, not 33-state toggles.

The current [README](../README.md) has stale MIDI availability statements and
omits the implemented delay-modulation destination. For this work, use code as
the authority and correct directly related control/MIDI descriptions.

## Target control model

### One authoritative position per parameter

1. Add a small hardware-independent mapping helper, proposed as
   `code/src/ParameterSteps.h`, defining the shared maximum of 32, clamping,
   MIDI conversions, and parameter-to-physical-value conversions.
2. Reuse [ModDest.h](../code/src/ModDest.h)'s clamping/broadcast patterns rather
   than introducing another input-routing framework. Reuse
   [EnvelopeMap.h](../code/src/EnvelopeMap.h) for attack/release.
3. Keep canonical integer positions in the engine, including master, fine
   tuning, and pipe length. Local turns, MIDI input, defaults, init patches,
   and preset application must all update these same positions through setters.
4. Advance with `clamp(current_position + delta, 0, 32)`. Preserve signed and
   multi-tick deltas. At a bound, discarded overshoot must not create a reversal
   dead zone. Do not derive the next position from rounded DSP units.
5. Use explicit step APIs for controls currently expressed as cents, percent,
   or milliseconds. Keep physical units at the DSP boundary; audit every caller
   instead of changing the meaning of an existing physical-unit API silently.
6. Do not keep independent UI/MIDI position caches that can drift from the
   engine. Derived physical targets and smoothed DSP values are not additional
   authoritative parameter states.

The hardware decoder already emits signed increments, which
[hardware.h](../code/src/hardware.h) accumulates. Preserve
[encoder.cpp](../code/src/encoder.cpp)'s quadrature decoding and debounce, the
application's 8 ms chatter filter, and press/tracker synchronization unless
hardware testing demonstrates a separate defect. Changing raw pulse decoding
would affect every control and is not the solution to unequal parameter spans.

### Physical mappings

Let `s` be the clamped position 0..32 and `u = s / 32.0`.

| Parameter | Mapping from position |
|---|---|
| Pipe level | Existing gain curve `u * u` |
| Character | Existing normalized Character amount `u`; existing 0..64 DSP step API can receive `2 * s` |
| Attack/release | Existing envelope mapping at step `s` |
| Modulation amounts, rotary, drive, reverb mix | Existing normalized curves at `u` |
| Master | Gain `u`, retaining the existing smoothing and recording/playback bus placement |
| Fine tune | `100 * (s - 16) / 32.0` cents; if retaining integer cents, round magnitude to nearest and restore sign so opposite positions are symmetric |
| Vibrato depth | `50 * u` cents, rounded to nearest integer for the existing DSP API |
| Vibrato note-on delay | `5000 * u` ms; pass milliseconds directly, not a rounded legacy step multiplied by 100 |
| Drive tone | `100 * u` percent, rounded to nearest integer for the existing DSP API; retain logarithmic 500 Hz..10 kHz per-stage cutoff |
| Reverb decay | `0.2 * pow(100, u)` seconds |
| Delay combined control | Pass `100 * u` into the existing combined mix/feedback curve |
| Delay time | `40 + 460 * u` ms, rounded to nearest integer if retaining the current millisecond API |

Retain all endpoints, bypass behavior, smoothing, tail preservation, free-running
LFOs/drift, and held-note update behavior. In particular, do not replace the
combined delay control with a plain wet/dry control: step 16 must remain its
50% pivot and step 32 must retain the current maximum feedback, including the
existing bounded self-oscillation range.

Integer physical targets still give 33 distinct values for the continuous
controls above. Rounding must happen only after deriving the target from `s`;
never repeatedly add rounded per-click physical increments. Keep units and
control positions distinct in names, types, constants, and comments.

### Pipe length: 32 positions, five sounding values

- Store a separate authoritative length position `s` for each pipe, initially 16.
- Derive the octave as `((s + 4) / 8) - 2` using integer division. This gives
  nearest-anchor snapping, with midpoint ties toward the higher octave:

  | Positions | Sounding octave offset |
  |---|---:|
  | 0..3 | -2 |
  | 4..11 | -1 |
  | 12..19 | 0 |
  | 20..27 | +1 |
  | 28..32 | +2 |

- The five exact anchors are 0, 8, 16, 24, 32. MIDI input first chooses a
  position on the shared 0..32 scale, then derives the octave by the same rule.
- Do not reconstruct the position from the octave after every tick: that would
  discard accumulated progress and prevent traversal of the snapping plateaus.
- SHIFT + turn broadcasts the resulting **position**, not just the octave, so
  subsequent turns on any pipe behave identically.
- Page changes, mode changes, encoder presses, and preset round trips retain
  the position even when the sounding octave has not changed.
- Thirty-two ticks traverse the entire control coordinate. The +2 octave sound
  begins at position 28, so audible endpoint arrival is not itself proof of
  completing the 32-position span.

## MIDI consistency

- Keep channel 1, CC assignments, DIN/USB transports, note handling, Program
  Change slots, playback gating, and the existing transmit/receive-only split.
- For every rotary parameter with a CC, use
  `position = (value * 32 + 63) / 127`. In particular, remove input scaling to
  50, 64, 100, or direct milliseconds.
- For existing outgoing parameter CCs, use
  `value = (position * 127 + 16) / 32`, including vibrato depth and Character.
  Update dirty/coalesced values and preset-load resynchronization to hold steps,
  not legacy cents or Character units.
- All 33 positions must round-trip exactly through MIDI out and in. CC 0, 64,
  and 127 select positions 0, 16, and 32. All 128 inputs must be monotonic,
  bounded, and reach the full position set.
- Length CCs 90..93 now preserve all 33 control positions, with the five sounding
  octaves derived above. The documented anchors 0/32/64/96/127 still select the
  same five octaves, but some between-anchor snapping thresholds intentionally
  change because quantization now uses the common position first.
- Mute and percussion CCs remain binary with the existing 64 threshold.
  Do not invent CCs for master/fine tune or add new MIDI outputs.
- Document that older automation still uses the same endpoints and approximate
  proportions but is quantized onto the new grid.

## Defaults and preset compatibility

[OrganPreset.h](../code/src/OrganPreset.h) currently stores mixed engine units
under version 1; parsing treats the version as informational.
Simply lowering range constants would clamp or misinterpret old sounds.

1. Introduce a version-2 preset representation with canonical 0..32 positions
   for every saved rotary parameter. Keep toggles binary. Preserve the length
   position, not just its derived octave. Master and fine tune remain unsaved.
2. Use explicit step field names for changed units, such as `character_step`,
   `vibrato_depth_step`, `vibrato_delay_step`, `drive_tone_step`,
   `reverb_decay_step`, `delay_mix_step`, `delay_time_step`, and `length_step`.
   Existing fields already measured in 0..32 steps can retain their names.
   Do not maintain competing authoritative physical-value fields.
3. Dispatch parsing by schema version before interpreting values. Treat missing
   version as legacy version 1. Clamp legacy values in their original ranges,
   then convert to the nearest position:
   - Character: old step / 64 mapped to 0..32.
   - Vibrato depth and vibrato delay: old value / 50 mapped to 0..32.
   - Tone, reverb-decay coordinate, combined delay control: old value / 100
     mapped to 0..32. Decay conversion is in its logarithmic coordinate.
   - Delay time: `(old_ms - 40) / 460` mapped to 0..32.
   - Length: `(old_octave + 2) * 8`, preserving the old octave exactly.
   - Already-32-step fields and toggles: unchanged.
4. For nonnegative legacy coordinates, use nearest rounding with ties upward.
   Fill missing fields with unit-correct defaults for that schema, and preserve
   the current unknown-field tolerance within a supported schema.
5. Unsupported future versions and invalid JSON must not become successful
   default patches. Leave the destination unchanged on failure, propagate the
   failure through [PresetStorage.h](../code/src/PresetStorage.h), and surface
   it through the existing application diagnostic/status path; add a narrow
   status signal if the existing path cannot distinguish an incompatible slot.
6. Import old files in RAM without dirtying or rewriting their SD slots.
   Serialize version 2 only on an explicit save/copy that writes a slot.
   Keep migration and serialization host-testable.
7. Update capture/apply, RAM cache, init patch, SD reload, preset copying, and
   outgoing CC resync together. Recheck JSON buffer and query-path sizes.

Most old interior values cannot be represented exactly at lower resolution.
This plan proposes nearest-grid migration, not bit-identical legacy sound
retention. Endpoints, exact bypasses, 50% positions, and octave choices survive.
The normalized rounding error is at most 1/64 of full scale before physical-unit
rounding; logarithmic controls must be assessed in their logarithmic coordinate.

Keep defaults that already lie on the grid: level 16, neutral fine tune and
length 16, tone 32, disabled effects/modulation 0, minimum envelopes/decay 0.
Quantize the current master boot gain with the same rule if necessary.
The 250 ms delay default is off-grid: proposed new default is position 15,
255.625 ms, or **256 ms** through the integer API. This is an intentional,
documented proposal for the eventual implementation review, not an unnoticed
default change. Do not add an irregular special-case grid just to retain 250 ms.

Back up SD presets before rollout. Older firmware does not enforce schema
versions and cannot safely consume the new files: document downgrade as
requiring restoration of the version-1 backup.

## LED and UX updates

- Derive continuous value-bar fill from position:
  `(position * 15 + 16) / 32`. Remove control-resolution assumptions such as
  divisors 50/64/100 from UI code, not legitimate physical/DSP constants.
- Keep fine tune's center-green/directional-red display, using position 16 as
  exact neutral. Preserve colors, brightness floors, mute overrides, 500 ms
  display ownership, and modulation destination indicators.
- Keep the five existing yellow length groups showing the sounding octave;
  document that ticks within a group still advance its retained position.
- Preserve local-only value bars: incoming MIDI changes state/steady knob
  feedback without claiming the temporary white-key bars.
- Audit steady knob LEDs, wheel mode feedback, preset-load resync, and playback
  volume feedback as well as temporary bars. Do not confuse 15 display keys
  with the 33 underlying positions.

## Implementation sequence (future work only)

1. Add shared step mappings and exhaustive host tests; retain existing helpers
   where possible rather than adding a generic parameter framework.
2. Add canonical engine step setters/getters and adapt DSP targets. Audit all
   existing physical-unit setters/callers and defaults.
3. Add version-aware preset migration, serialization, capture/apply, and
   diagnostics. Test old/new fixtures before changing local input routing.
4. Wire every row of the control inventory, including SHIFT broadcasts and
   playback master volume. Preserve disabled contexts and press behavior.
5. Wire MIDI input/output and dirty-state/resync paths to the same step APIs.
6. Update LED normalization and directly related comments/documentation in
   [README.md](../README.md) and [midi.md](../midi.md). Describe physical ranges
   separately from 32 control ticks, including pipe-length plateaus and preset
   compatibility/default changes.
7. Run focused tests, integration checks, a firmware build, and hardware/MIDI
   acceptance tests. Ship only after the criteria below are satisfied.

## Verification and acceptance criteria

### Automated host tests

- Add a focused mapping/control test target to
  [code/tests/Makefile](../code/tests/Makefile). Exercise the same pure mapping
  and turn/broadcast logic used by production code, not copied test formulas.
- For every active control/context, verify 32 successive +1 ticks take position
  0 to 32 and 32 successive -1 ticks return it; test each intermediate position,
  neutral, bounds, repeated overshoot, immediate reversal, and multi-tick deltas.
- Verify every continuous physical mapping is monotonic with 33 distinct
  targets and exact intended endpoints. Check fine-tune symmetry/zero, Character
  DSP endpoints, delay pivot/maximum feedback, log envelope/decay curves,
  vibrato delay endpoints, and proposed delay default.
- For length, test all 33 positions, snapping boundaries, all anchors, plateau
  retention across page/press/mode changes, immediate direction reversal,
  broadcast of different source positions within the same octave, and preset
  capture/apply preserving those positions.
- Exhaust all 128 MIDI inputs per mapped parameter and all 33 outgoing
  round trips; verify CC layout, toggle thresholds, and length's updated
  between-anchor behavior.
- Extend [midi_cc_map_test.cpp](../code/tests/midi_cc_map_test.cpp),
  [mod_dest_test.cpp](../code/tests/mod_dest_test.cpp), and
  [organ_preset_test.cpp](../code/tests/organ_preset_test.cpp) as appropriate.
  Include version-1/missing-version migration fixtures, all changed fields,
  missing values, clamping, invalid/unsupported versions, unchanged destination
  on failure, version-2 round trips, and serialization/query buffer bounds.
- Use engine integration coverage, following
  [percussion_test.cpp](../code/tests/percussion_test.cpp), to verify real
  setters, init/defaults, preset application, and MIDI-facing values agree.
  Extract only the small hardware-independent routing seams needed to test
  normal/SHIFT/playback dispatch; do not redesign the whole polling loop.
- Run focused targets first, then the existing native suite once integrated;
  retain DSP regression tests for exact bypasses, envelopes, modulation,
  distortion, vibrato, and delay/reverb behavior.

### Firmware and device checks

- Build using the repository firmware build workflow; check warnings, linker
  limits, and report SRAM, SRAM_EXEC, and SDRAM usage. Do not add allocation,
  SD I/O, or MIDI transmission to the real-time parameter-update path.
- On hardware, traverse every inventory row at normal playing speed and count
  accepted ticks. Check all four physical pipe encoders, both hardware modes,
  every page, normal/SHIFT turns, wheel assignments, and playback master.
- Verify page/press transitions never reset parameter positions or cause a
  first-turn jump; verify SHIFT broadcasts remain source-relative absolute
  assignments even when pipes start at different values.
- Check DIN and USB separately with CC sweeps, local outgoing CC capture,
  preset-load resync, no incoming echo/value bars, and playback MIDI gating.
- Listen while holding/latched notes and while tails ring: retain click-free
  updates, bypass silence, phase continuity, delay glides, bounded feedback,
  vibrato triggering/ramp, and free-running modulation.
- Save/reload every changed parameter, especially length plateau positions;
  import a backed-up old preset bank without modifying the source files.

Completion means one consistent 0..32 control domain across all active normal
and SHIFT rotary controls, matching MIDI and persistence, with intentional
quantization changes documented and tested. Renaming MIDI ranges alone, scaling
only encoder deltas, or changing the low-level encoder decoder is insufficient.
