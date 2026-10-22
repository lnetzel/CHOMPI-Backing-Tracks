# backing-tracks

`backing-tracks` is an independent live-performance WAV player and mixer for CHOMPI-compatible hardware. It is not an organ or playable instrument. Its sources are SD-card WAV playback, the external stereo input, and a headphone-only metronome. It is an independent project, not an official CHOMPI Club release.

The repository retains history from CHOMPI-Pipes, starting at `1e2bc676184d5ddb5330bc844c5471b1e372bbe8`. The original CHOMPI-Pipes checkout is the hardware and build reference and is not modified by this project.

**Status:** experimental. The firmware builds and host tests pass, but no on-device audio-routing, SD-stall, MIDI-jitter, receiver, or soak test has been completed. Do not treat a successful build as stage-readiness evidence.

## Controls

- Encoders 1-4, left to right: track, aux input, click, headphone level.
- Encoder 5: previous/next song; selection is clamped to the set.
- Encoder 6: main-output level.
- Play/Pause key: start, pause, or arm quantized resume.
- Loop key: Stop and reset the selected song.
- Hold SHIFT/CHOMPI and press the first black key (C#) to toggle MIDI DIN
  output. The C# key LED is green when enabled and red when disabled.
- While stopped, hold SHIFT/CHOMPI and press the second black key (D#) to cycle
  the selected song's advance mode: wait (red), auto (blue), loop (yellow).
  The selected mode is saved to `set.json` and applies the next time you play.
- The toggle selects page 1 (songs 1-15) or page 2 (songs 16-30) while stopped. White keys select a song on the active page. Empty keys do nothing.
- Song selection and page changes are ignored during playback, pause, loading, and pending resume. Encoder pushes have no assigned action.

Track, aux, click, headphone, and main levels use 33 positions (0-32). First-boot levels are track 16, aux 16, click 0, headphones 16, and main 16. Levels are saved under `BACKING-TRACKS/` in two readable, redundant checksummed JSON slots when transport is stopped. The click starts muted.

Turning a level encoder displays a left-to-right level bar on keys 1-15 for 1.5 seconds, then restores song-selection lighting. Zero is dark; full level fills all 15 keys. Each level encoder's panel LED continuously follows its level in brightness.

The metronome uses a soft, decaying tone with a higher-pitched first beat of each bar. The two LEDs above encoder 5 flash with the beat: left for the bar downbeat, right for other beats, including when click audio is muted. Click and beat flashes continue during pause after the first downbeat. Play/Pause blinks green while playing and stays green while paused or waiting for quantized resume. Loop stays orange while stopped; otherwise it is dark. Both transport lights are dark during loading and errors. The separate status LED shows red for playback/card/set errors and amber for clipping or missing/unsaved/failed level settings. A WAV that fails preflight blinks red in its song slot and clears the other song lights; fix that file before the set can play. A song that fails to open at playback also blinks red in its slot. Select the other page while stopped if the failing song is on it.

## SD Card

Prepare audio as 48 kHz, signed 16-bit PCM WAV, mono or stereo. Mono is duplicated to both sides. No resampling, compressed audio, RF64, tempo maps, recording, effects, or gapless transition is supported.

Create this layout at the card root:

```text
BACKING-TRACKS/
  set.json
  LEVEL0.json      firmware-created, readable settings slot
  LEVEL1.json      firmware-created, readable settings slot
  TRACKS/
    01-intro.wav
    02-song.wav
```

Song paths in `BACKING-TRACKS/set.json` are relative to `BACKING-TRACKS/`.
The firmware updates only the selected entry's `advance` value when changed
with SHIFT+D# while stopped; it verifies a temporary file before replacing the
set and keeps a backup during replacement. MIDI clock overrides remain
RAM-only. Example:

```json
{
  "version": 1,
  "songs": [
    {
      "file": "TRACKS/01-intro.wav",
      "bpm": 120,
      "meter": [4, 4],
      "first_downbeat_ms": 0,
      "advance": "auto",
      "midi_clock": true
    },
    {
      "file": "TRACKS/02-song.wav",
      "bpm": 98.5,
      "meter": [6, 8],
      "first_downbeat_ms": 350,
      "advance": "wait",
      "midi_clock": false
    },
    {
      "file": "TRACKS/01-intro.wav",
      "bpm": 120,
      "meter": [4, 4],
      "first_downbeat_ms": 0,
      "advance": "loop",
      "midi_clock": true
    }
  ]
}
```

The set supports 1-30 ordered entries, 127-byte relative paths, and a 16 KiB file limit. BPM is 30-300 and always counts quarter notes. In /8 meters the click counts eighth notes. `first_downbeat_ms` gates click and MIDI Start but WAV playback begins at frame zero. `advance` is `auto`, `wait`, or `loop`: auto starts the next entry, wait selects the next entry and waits for Play, and loop replays the same entry. Auto and wait do not wrap after the last entry. Optional `midi_clock` must be a JSON boolean and defaults to `true`. Repeated file paths are allowed. The parser rejects traversal, absolute paths, duplicate/unknown fields, invalid ranges, unsupported WAV formats, and truncated files. See [the full specification](docs/specification.md) and [example set](docs/examples/set.json).

No automatic playback occurs at boot. The first song is selected and waits for Play. Aux monitoring remains active if the card or set is missing, during transport stops/pauses, and on file errors. After card removal or replacement, press Play while stopped to reload the set and settings; this does not start playback. Press Play again to start the selected song.

## MIDI

The physical DIN output sends MIDI Start at the first downbeat, 24-PPQN Clock while the timeline runs (including after pause), Stop on pause/stop/end, and Continue at a quantized resume boundary. Disabling output sends one Stop for an established session and suppresses later messages without affecting audio or click. Enabling during playback restarts the receiver at the next bar downbeat. A live override lasts for the selected set entry and is not saved. MIDI input, USB MIDI, and Song Position Pointer are not implemented. See [midi.md](midi.md) for receiver and pause behavior.

## Build and Tests

On Windows, install MSYS2 with GNU Make and Arm GNU Toolchain 10.3-2021.10 (`arm-none-eabi-gcc` 10.3.1) at `C:\armgnu\10.3-2021.10`. Use an MSYS2 shell:

```sh
export PATH="/c/armgnu/10.3-2021.10/bin:$PATH"
cd /path/to/backing-tracks/code/src
make check-toolchain
make -j4 DEBUG=0 OPT=-O3
```

The output is `code/src/build/BACKING_TRACKS_1.0.bin` (BOOT_SRAM app). Preserve the CHOMPI bootloader; do not flash it as part of a firmware update. The build uses the committed CHOMPI-adapted libDaisy and vendored dependencies. Every build must report SRAM, SRAM_EXEC, and SDRAM usage; current values are recorded with validation notes in the project history, not treated as permanent budgets.

Run the backing-tracks host tests with a host C/C++ compiler:

```sh
make -C code/tests test
```

The tests cover stereo routing and click isolation, bounded set/WAV parsing, sample-derived timing, transport ring/state behavior, and settings integrity. Physical jack mapping, analog crosstalk, DIN timing/jitter, real receiver behavior, SD latency, and full-set soak remain hardware acceptance gates. See the [control card](docs/control-card.md) and [stage checklist](docs/stage-checklist.md).

## License and Attribution

The root project is MIT-licensed. Vendored dependencies retain their own license files and source notices; see [THIRD_PARTY.md](THIRD_PARTY.md). CHOMPI hardware compatibility does not imply endorsement or an official CHOMPI Club release.
