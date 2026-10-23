# Per-track BPM readout and five-position editor

## Problem and goal

Read and edit the selected set entry's `bpm` in `BACKING-TRACKS/set.json`
directly on CHOMPI, without a computer. Interpret the request's "BMP" as BPM.
Support three integer digits and up to two decimal places, such as 125.54.
Accept higher-precision BPM from the card and round to two decimal places for
playback and editing: `133.456` becomes an effective tempo of `133.46` for both
the metronome and MIDI clock, without invalidating the set.
Keep each entry independent, including entries that reference the same WAV.

This is a plan only. Do not change firmware until approved.

## Requested controls

| Entry chord | Latched digit mode | Example at 125.54 BPM |
|---|---|---|
| SHIFT/CHOMPI + lower F# | Hundreds | 1 |
| SHIFT/CHOMPI + lower G# | Tens | 2 |
| SHIFT/CHOMPI + lower A# | Ones | 5 |
| SHIFT/CHOMPI + upper C# | Tenths | 5 |
| SHIFT/CHOMPI + upper D# | Hundredths | 4 |

White keys 1-9 enter digits 1-9; white key 10 enters 0. Keys are numbered
physically from the left, independently of the selected song page.

Only enter a digit mode while playback is fully stopped, never while playing
or paused (including pending resume, loading, or ready states). A valid selected
set entry is required.

SHIFT plus one of the five selectors latches its digit mode. Release the chord;
the mode remains active. Highlight that black key and the white key matching
the saved digit, both with white LEDs. Read 120 as 120.00: the five modes show
white keys 1, 2, 10, 10, and 10. Read 75 as 075.00: they show keys 10 (zero),
7, 5, 10, and 10. Missing decimal places always read as zero.
For higher-precision card values, "saved digit" in this plan means the digit of
the effective rounded BPM: `133.456` reads as 133.46, not 133.45. Loading or
inspecting it does not change the original JSON token on the card.

All other normal controls are locked while the mode is active. Press white
key 1-10 to replace that digit and immediately save the resulting BPM to the
card, without any extra Save or Play press. If no value key is pressed, nothing
is overwritten. A new SHIFT press exits without changing the saved value.

To enter 125 from 120: enter hundreds mode with SHIFT + lower F# and choose
white key 1; enter tens mode with SHIFT + lower G# and choose white key 2;
enter ones mode with SHIFT + lower A# and choose white key 5. Each accepted
change saves independently; there is no multi-digit draft or final confirmation.

To extend 125 to 125.54, enter tenths mode with SHIFT + upper C# and choose
white key 5, immediately saving 125.50. Then enter hundredths mode with SHIFT
+ upper D# and choose white key 4, immediately saving 125.54. Each successful
value choice exits automatically. Every edit preserves the other four digits,
including decimals when an integer digit is changed.

## Recommended approach

### Mode lifecycle and remaining recommendations

The latched mode, immediate saving, SHIFT exit, white selector/value highlights,
control lock, stopped-only entry, automatic exit after successful value entry,
five-position mapping, preservation of other digits, 30.00-300.00 range, and
acceptance/rounding of higher-precision card values are confirmed requirements.
The following details are recommended where the request leaves them open:

- A successful value choice exits the mode after the save completes. To edit
  another digit, enter its mode with a new chord. No extra button press is
  needed to save or exit after success.
- Choosing the existing digit exits without rewriting an unchanged BPM, avoiding
  unnecessary SD writes. A read-only inspection never rewrites the file.
- Edit effective BPM values 030.00-300.00 inclusive with two decimal places.
  Leading zero is a UI convention: save 030.00 as JSON `30.00`, never invalid
  JSON `030.00`. Reject out-of-range replacements, keep the saved BPM and
  mode unchanged, and briefly flash the selector red. Do not clamp or wrap.
- Because every accepted digit is saved immediately, intermediate BPM values
  must also be valid. For example, change 125 to 030 through 135 -> 035 -> 030,
  rather than trying to save invalid 025. No hidden draft or deferred save.
- Show the selected black key and saved digit in white, use amber on the selector
  while the write is pending, and briefly acknowledge success in green. On
  failure, keep the mode open with the original saved digit and storage fault
  indication so the user can retry or press SHIFT to leave.
- Accept existing integer and fractional BPM, including more than two decimal
  places and trailing zeros. Round higher precision to the nearest hundredth
  on load, with exact halfway cases rounded up for positive BPM. Do not rewrite
  the card on load or inspection. A real digit change starts from the rounded
  value and saves the new two-decimal BPM; choosing an unchanged digit leaves
  the original higher-precision token untouched.

### Controls and feedback ownership

`PollControls()` currently handles stopped white-key song selection before
modifier shortcuts and also applies level encoder changes first. Route editor
entry and active-mode dispatch before all normal control mutations. In a mode,
ignore level/song encoders, song/page selection, Play, Loop, MIDI toggle,
advance edits, and other normal shortcuts. Keys 11-15 and other black keys are
inert. Keep audio and aux monitoring running at their existing levels; this is
an input lock, not a suspended callback. Preserve safety/power handling.

Sample key states once per scan. Enter on a fresh selector press while SHIFT
and exactly one selector are held, with no conflicting shortcut. Once latched,
neither SHIFT nor the selector must remain held. Require a fresh white-key edge
after entry; already-held keys cannot enter a value. Reject simultaneous digit
edges rather than choosing an arbitrary winner. The entry SHIFT press must not
also exit: arm exit only after SHIFT has been released, then consume its next
press as exit. If exit and value arrive together, exit wins with no write.

Update all key histories and drain/discard encoder increments while locked.
Suppress held inputs until released on exit so Play, white keys, and accumulated
encoder movement cannot leak through. Initialize histories before audio starts
to suppress boot-held keys. Do not enter a mode after stopping merely because
its keys were already held during playback or pause; require a fresh chord.

The page switch is a maintained-position control, not an accumulated key event.
Freeze the displayed page while editing. On every editor exit, apply its current
physical position to the displayed page without changing the selected entry or
playing anything. Moving it while locked must never redirect a pending BPM
write. Apply held-key suppression before accepting song keys on the new page.

Verify lower F#/G#/A# and upper C#/D# switch and LED mappings on hardware; do not infer them
solely from enum order. Reuse the existing SHIFT mapping. Ambiguous chords
containing lower C#/D# and a BPM selector must not toggle MIDI or advance mode.
Upper C#/D# are decimal selectors, distinct from those lower shortcut keys.
Outside editor mode, unambiguous existing shortcuts remain unchanged.

Throughout the latched mode, blank the white-key overlay and light only the
matching saved digit plus the active one of the five black-key selectors. Give this readout priority
over song lights and temporary level bars; fatal/file-error feedback remains
highest priority. Leave unrelated status LEDs unchanged. Restore normal overlays
on exit, including after success. Never show an unsaved value as committed.

Keep editor decisions in a small hardware-independent state machine shared by
the callback and host tests. Publish a coherent feedback snapshot to the LED
renderer. Do not parse JSON, access FatFs, or allocate memory in the callback.

### JSON rewrite and persistence

Reuse `bpm`; no new field or set version is needed. Preserve required-field,
JSON-number, and finite/range validation. Validate the original numeric value
against 30-300 inclusive before rounding: 29.999 and 300.001 remain out of
range, rather than being admitted through rounding. Extra decimal places alone
never invalidate an otherwise supported number.

Round once on load to the nearest hundredth using decimal half-up semantics:
`133.456` -> `133.46`, `133.454` -> `133.45`, `133.455` -> `133.46`, and
`125.5400` -> `125.54`. Handle carries such as `133.995` -> `134.00` and
`299.999` -> `300.00`. Do not truncate or depend on binary float tie behavior.
Retain JSON exponent notation with the identical numeric rounding rule:
`1.33456e2` and `133456e-3` both become `133.46`.

Bound token/exponent processing and check overflow before arithmetic; reject
malformed, non-finite, out-of-range, or unsupported-length tokens, not values
merely because they have extra precision. Process mantissa digits within the
existing token bound without requiring the whole mantissa to fit an integer.
Reject excessive exponents safely rather than overflowing or looping in
proportion to their numeric magnitude. Changed values always save in ordinary
two-decimal notation, not exponent notation. Loading a higher-precision value
must not trigger a storage warning, a card write, or a playback error.

Use exact rounded integer hundredths (3000-30000) for the parsed tempo, editor
snapshots, digit replacement, validation, and save requests. The five position
weights are 10000, 1000, 100, 10, and 1 in these units. Convert to the existing
floating-point timing representation only at the runtime boundary; do not
derive editable digits by truncating a binary float multiplied by 100. Keep the
parsed rounded value available to the editor, in the song model or an existing
coherent publication, without reparsing JSON inside the callback.
Initialize metronome, MIDI clock, beat feedback, bar phase, and quantized resume
from this same effective tempo. No timing path may use the unrounded token.

Serialize changed BPM with exactly two decimal digits using integer formatting,
such as `125.54`, `125.50`, or `30.00`; do not rewrite an unchanged token merely
to normalize its formatting. Never emit a quoted number or a leading zero.

Add a bounded BPM-token rewrite beside `RewriteBackingAdvance()` in
`BackingSet.h`, using coreJSON to find the selected array entry's numeric
token. Unlike the four-character advance value, BPM tokens vary in length:
`30`, `120`, `120.5`, `125.54`, and exponent notation require different space. Accept
buffer capacity and update length explicitly; check capacity before moving the
suffix, preserve termination, and use overlap-safe movement. Preserve every
byte outside that one token, including whitespace, other entries, and fields.
Validate the candidate with the production set parser before persistence.
Reject missing/duplicate fields, invalid entry indices, and invalid values
without changing the input buffer or its length.

Use the existing foreground `SaveSetJson()` temporary-file, sync, readback,
backup, and rename path. Serialize BPM and advance writes; neither may lose
the other's change. Capture entry identity, set generation, request revision, and
requested BPM in a bounded request/acknowledgement handoff. Ignore stale
requests after reload and stale acknowledgements after selection changes.

A valid changed white-key press queues the write immediately; do not wait for
SHIFT release, mode exit, timeout, or another confirmation. Run the foreground
service promptly with no intentional debounce-save delay. Keep controls locked
and serialize any preceding advance/settings transaction before writing.
During the bounded pending write, consume extra digit presses; SHIFT may request
exit, but cannot undo an already-submitted save. Complete/acknowledge that write
before releasing the input lock. A reload or card-loss event invalidates the
editor session safely; never apply its result to another entry or set.

### Save deadline and recovery

Set an engineering acceptance limit of five seconds from accepting a changed
digit through success/failure acknowledgement, including preceding queued
storage work and cleanup. Start the request promptly; this deadline is a fault
limit, not a normal save delay. Use a monotonic deadline and finite driver waits
that fit the remaining budget. Do not retry automatically or extend the
deadline after a timeout. The callback and aux monitoring must remain active.

Before wiring persistence, verify the actual FatFs/SD driver timeout and abort
behavior. A foreground elapsed-time check after a blocking call is not enough.
All in-flight operations must return or be safely aborted/quiesced within the
budget before acknowledging failure or allowing another storage operation.
If the current driver cannot meet this requirement, treat it as an implementation
blocker and seek approval for any driver changes outside this feature's scope;
do not claim the editor is bounded or unlock around a still-running write.

On timeout, latch the storage error and return failure, never success. Restore
the original in-memory JSON and runtime BPM. If the destination is known to
remain unchanged and the card/set is valid, return to the selected digit mode
so SHIFT can exit or a fresh digit press can retry. Honor an already requested
SHIFT exit after cleanup. If a rename/rollback failure leaves the on-card commit
uncertain, invalidate the set, close the editor, and require the existing safe
reload/recovery flow before playback or further edits. Do not assume that RAM
rollback proves the disk was rolled back. Recovery must preserve a usable
destination or backup where possible; never delete the only recoverable copy.

Publish the new `BackingSong::bpm` only after persistence succeeds. On failure,
restore the exact original JSON token, document length, and terminator, not a
reformatted approximation of a float. Keep the committed runtime BPM unchanged,
retain the selected digit mode for retry/exit if the card/set is still valid,
and latch the existing storage error. A read-only card must not produce false
success. Card loss closes the editor and preserves the existing invalid-set
error/reload behavior, with no automatic retry against replacement media.

Saved tempo takes effect on the next playback initialization through the
existing timing engine. It changes click, beat, MIDI clock, and quantized timing
metadata, not WAV playback speed or pitch, meter, or first-downbeat offset.

## Affected files/components

- [backing_tracks_main.cpp](../code/src/backing_tracks_main.cpp): control
  precedence, latched mode/input lock, request/acknowledgement service,
  runtime publication, and LED overlay integration.
- [BackingSet.h](../code/src/BackingSet.h): bounded variable-length BPM rewrite
  and candidate validation; accept extra precision and round decimal/exponent
  values to the shared effective hundredths tempo on load.
- [BackingFeedback.h](../code/src/BackingFeedback.h): digit-to-key feedback and
  overlay decisions where consistent with existing helpers.
- [BackingTypes.h](../code/src/BackingTypes.h) and
  [BackingTiming.h](../code/src/BackingTiming.h): retain exact hundredths for
  editing and reuse existing floating-point timing initialization; no timing
  engine rewrite expected.
- [hardware.h](../code/src/hardware.h) and
  [temp_led_stuff.h](../code/src/temp_led_stuff.h): mapping references; no driver
  changes expected.
- A small new hardware-independent editor header under `code/src/` and focused
  host test under `code/tests/`, only to share actual production editor logic.
- [backing_format_test.cpp](../code/tests/backing_format_test.cpp),
  [backing_transport_test.cpp](../code/tests/backing_transport_test.cpp),
  [backing_timing_test.cpp](../code/tests/backing_timing_test.cpp), and
  [test Makefile](../code/tests/Makefile): persistence, interaction, and timing
  regression coverage.
- [control-card.md](../docs/control-card.md),
  [specification.md](../docs/specification.md), and
  [stage-checklist.md](../docs/stage-checklist.md): performer contract and
  hardware acceptance checks.

## Step-by-step tasks, in order

1. Retain the confirmed five-position interaction contract; review only the
  remaining unchanged-value, invalid-input feedback, and save-pending details.
2. Verify all five physical selector and white-key/LED mappings, including the
  distinction between lower shortcuts and upper decimal selectors.
3. Implement and host-test exact decimal half-up rounding to hundredths,
  digit preservation, bounded rewriting, rollback, and capacity handling.
4. Implement and host-test latched mode transitions, full input lock, digit
    mapping, SHIFT exit arming, edge detection, and ambiguity rejection.
5. Verify driver timeout/abort behavior against the five-second end-to-end save
  limit, then wire foreground persistence, serialized requests, stale-message
  protection, rollback, acknowledgement, and playback gating. Stop for scope
  approval if meeting the limit requires unrelated driver changes.
6. Integrate selector/value highlighting and save/error feedback without hiding
   fault indications or changing audio/MIDI behavior.
7. Update the directly affected documentation with 125, 030, and 125.54 examples,
   immediate per-digit saving, SHIFT exit, control lock, range constraints,
  higher-precision rounding and exponent handling, automatic exit, page-switch behavior,
  save failure/recovery, and no-time-stretch warning.
8. Run focused host tests, then `make -C code/tests test`. Build firmware using
   the build skill and report SRAM, SRAM_EXEC, and SDRAM usage.
9. Complete hardware acceptance before declaring the feature implemented.

## Risks, open questions, memory/CPU/timing considerations

- Per-digit immediate writes require valid intermediate values in the current
  30.00-300.00 range. Reject invalid replacements rather than persisting a set that
  cannot load. Expanding that range would require a separate decision.
- Higher-precision sets remain loadable, but timing now uses the documented
  nearest-hundredth tempo. The original card token may differ from the displayed
  effective value until explicitly edited; inspection never normalizes it.
- Decimal rounding must not inherit binary float representation errors.
  At 300.00, either nonzero fractional digit is out of range; reduce the integer
  part first. At the lower limit, preserve 30.00 as the minimum.
- A BPM metadata edit can desynchronize click/MIDI from a recorded WAV if the
  entered BPM does not match the recording. It is not time stretching.
- Existing backup/rename persistence is not a blanket guarantee against power
  loss on FAT/SD. Exercise actual recovery states and document limits.
- The five-second fault deadline depends on verified low-level timeouts and
  safe cleanup, not simply an application timer. An uncertain disk commit must
  block playback until reload establishes the actual saved value.
- Variable-length rollback, concurrent advance edits, and stale entry identity
  are the main correctness risks. Repeated WAV paths must remain independent.
- Budget fixed-size editor state, snapshots, and one pending request. Prefer
  existing document/readback storage and bounded token backups over another
  whole-set buffer. No heap, new audio buffers, or unbounded callback work.
- JSON movement, validation, and SD writes run in the foreground while stopped;
  aux monitoring and callback deadlines still must remain unaffected. Do not
  disable interrupts around file operations or full-document copying.

## Testing and validation

### Focused host checks

- Read 120 as keys 1/2/10/10/10 and 75 as 10/7/5/10/10; highlight the selected
  black key and matching white key in white in all five modes. Release all
  entry keys and verify the mode stays latched. Press SHIFT again to exit and
  assert zero JSON changes or writes after read-only inspection.
- Enter 125 with the requested selectors and values; assert each changed digit
  triggers persistence immediately, without Play, SHIFT release, or timeout.
  Verify successful changes exit automatically only after persistence succeeds,
  and unchanged-digit choices exit without writing.
- Reach 030, 099, 100, 299, and 300 via valid per-digit transitions. Reject
  attempted 000, 025, 029, 301, and 999 without changing saved data or leaving
  the mode. Cover the valid 125 -> 135 -> 035 -> 030 route without clamping.
- Enter 125.54 from 125 via upper C# + key 5 (125.50), then upper D# + key 4
  (125.54). Verify immediate writes, automatic exit, and reload after each step.
- Change each of the five digits independently and assert the other four are
  preserved. Test decimal zero via key 10, integer edits preserving `.54`, and
  hundredths edits preserving tenths. Reject 300.01 and 300.10.
- Parse integer, one-decimal, two-decimal, and higher-precision numbers,
  including 30.01, 125.54, 125.5400, 133.456, 299.99, and 300.00. Assert
  133.456 -> 133.46, 133.454 -> 133.45, 133.455 -> 133.46,
  133.995 -> 134.00, and 299.999 -> 300.00. Reject malformed/non-finite
  tokens and original values outside the range, including 29.999 and 300.001.
  Test equivalent exponent forms and long supported mantissas, halfway cases,
  carry propagation, exponent overflow, and very large exponent magnitudes;
  assert deterministic rounding, bounded processing, and no arithmetic overflow.
- Load a set containing 133.456 and verify successful validation, white-key
  readout of 133.46, and no warning or card mutation. Choosing an unchanged
  digit or pressing SHIFT to exit preserves the exact original token. A real
  digit edit preserves the other rounded digits and writes two-decimal BPM.
- Inspect fractional values without mutation; test unchanged-digit no-op,
  SHIFT exit, and exact failed-save rollback of decimal/exponent tokens.
- Verify one event per press, held/boot-held suppression, fresh-chord entry,
  SHIFT exit arming, exit/value collision priority, invalid multi-key chords,
  keys 11-15, and no action from inputs still held when the mode exits.
- Assert all normal controls are inert while latched/saving: encoders, song/page
  selection, Play, Loop, MIDI toggle, advance mode, and reload gestures. Check
  that suppressed encoder movement does not accumulate for later application.
- Move the page switch while locked, including during a save. On success,
  SHIFT exit, or failure-related exit, assert that the page follows its current
  position, selection is unchanged, the write targets the original entry, and
  held white keys cannot select a track on the newly displayed page.
- Reject mode entry while playing, paused, pending resume, loading, ready, or
  in error, and without a valid set/entry. Verify normal controls outside mode.
- Rewrite shorter/equal/longer tokens at exact capacity; assert full-document
  byte preservation outside the target token, valid termination, unchanged
  input on rejection, repeated-file independence, and parser compatibility.
- Inject write/sync/readback/rename failure and card removal; verify exact
  rollback, no false acknowledgement, safe retry/reload, write serialization,
  and stale request/acknowledgement rejection.
- With a fake monotonic clock and storage backend, exercise delay/timeouts at
  each stage and preceding queued work. Assert acknowledgement within five
  seconds, no automatic retries, no unlocking with in-flight I/O, SHIFT exit
  after cleanup, and invalidation/reload when the disk commit is uncertain.
  Host fakes alone do not establish that the actual SD driver meets the limit.
- Verify overlay priority, selector/value/save/error feedback, restored lights,
  and preserved fatal errors. Test the production helpers, not a test-only copy.
- After saving 125.54, verify next-start click/beat/MIDI timing uses 125.54 BPM while
  WAV frame progression, meter, first-downbeat offset, and prior playback
  behavior remain unchanged.
- Load 133.456 without editing; compare metronome, MIDI clock, beat/bar phase,
  and quantized resume against a set explicitly containing 133.46. They must
  use the same effective tempo, not 133.456 or a truncated 133.45.

### Build and hardware acceptance

- Run the existing host suite and firmware build with all three memory regions
  reported. No firmware build is necessary merely to approve this plan.
- Verify physical latched readout and the 125/030/125.54 sequences on both song pages;
  reboot and inspect `set.json` after each changed value to confirm immediate
  persistence and entry isolation. Verify SHIFT-only exit never writes.
- Verify no editor entry during playback or pause; while stopped and latched,
  verify the full input lock and uninterrupted aux monitoring.
- Exercise write-protected media, card removal, interrupted save/recovery, and
  rapid input around value entry/exit. Measure callback timing under save/LED load.
- Measure request-to-acknowledgement under queued storage load, slow/failing
  cards, and removal. Verify the five-second limit, no stuck input lock, safe
  recovery after rename failure, and working SHIFT exit after known failures.
  Verify page-switch reconciliation and exponent-form sets on actual hardware.
- Load 133.456 on the card and verify normal playback, metronome/MIDI timing
  at 133.46, rounded readout, and an unchanged source token after inspection
  and reboot. Verify an explicit changed digit saves the resulting rounded BPM.