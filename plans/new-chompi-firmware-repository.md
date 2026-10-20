# Starting a new CHOMPI firmware repository

## Problem and recommended approach

Create a different firmware for the same CHOMPI hardware while reusing selected
parts of CHOMPI-Pipes. Use an independent GitHub repository seeded from this
repository, retaining its Git history. Keep the proven hardware/build foundation
initially; choose instrument features only after defining the new specification.

This is a setup plan, not authorization to create a repository, change remotes,
push commits, or modify firmware in the existing checkout.

## Repository and specification decisions

- Keep CHOMPI-Pipes unchanged as a reference and independently maintained product.
- Put the new specification in the new repository, for example
  `docs/specification.md`. Do not make CHOMPI-Pipes' product documentation describe
  a different instrument.
- A renamed GitHub fork is suitable when an upstream relationship and contributions
  back are desirable. Renaming does not remove the fork relationship.
- For the chosen independent approach, create an empty GitHub repository and use a
  separate normal clone of CHOMPI-Pipes as the local starting point. Retain Git
  metadata rather than copying files into a freshly initialized repository.
- In that new clone only, rename the existing remote to `upstream` and add the new
  repository as `origin`. Push the chosen starting branch to the new repository.
  Do not mirror-push all branches and tags by default.
- Record the exact CHOMPI-Pipes starting commit in the new project's documentation.
  Keeping it in Git ancestry supports later comparison and selective cherry-picks.

## Todos

### 1. Defining the new instrument

Write the specification in the new repository before replacing behavior. Include:

- Instrument purpose, sound generation, polyphony and signal flow.
- Encoder pages, key actions, SHIFT gestures, toggle modes and LED feedback.
- MIDI input/output, channel behavior and CC mappings.
- Whether presets, SD recording, playback and each effect are wanted.
- Persistence format, boot defaults and whether compatibility with PIPES presets
  is required. Do not assume compatibility or overwrite existing preset files.
- Acceptance criteria, performance/memory constraints and hardware checks.

The instrument design and feature selection remain open; none are assumed here.
The specification can be drafted separately before the new repository exists.

### 2. Seeding the independent repository

After the name, location and GitHub visibility are chosen:

- Create an empty destination repository.
- Clone this project into a different local folder, preserving history.
- Configure `upstream` and `origin` in the new clone only.
- Publish the intended starting branch and record its source commit.
- Keep the current checkout and its remotes untouched.

### 3. Establishing a verified hardware baseline

Before removing instrument code, build the unchanged starting firmware using the
existing documented workflow and pinned Arm GCC 10.3.1 toolchain, run the existing
host tests, and establish a hardware smoke-test baseline.

Preserve initially:

- `code/src/hardware.h`, `encoder.h`, `encoder.cpp` and `temp_led_stuff.h`.
- `code/src/Makefile`, `chompi_sram.lds`, and bootloader compatibility.
- The vendored dependencies in `code/libs/`, including the CHOMPI-adapted libDaisy.
- Known-working audio initialization, control polling and execution-context rules.

`chompi_main.cpp` contains application integration as well as reusable startup and
hardware handling; it is not a drop-in generic framework. Preserve the known-good
execution pattern while replacing product-specific logic.

Report SRAM, SRAM_EXEC and SDRAM usage when building. Hardware validation is a
separate requirement from a successful compile.

### 4. Adapting the firmware to the specification

Once the specification and baseline are available:

- Classify existing components as keep, adapt or remove.
- Treat OrganEngine, registrations, organ presets, controls and associated MIDI
  behavior as product-specific, not mandatory platform code.
- Evaluate effects, SD player/recorder, preset storage and MIDI handling as reuse
  candidates; inspect their contracts and coupling before carrying them over.
- Rename the build target/output and product-facing documentation for the new
  instrument. Update source comments that refer to old specifications.
- Implement one vertical slice first: boot, playable input, intended audio output
  and basic controls. Add selected effects, MIDI and persistence incrementally.
- Retain applicable tests, replace product-specific assertions as behavior changes,
  and add tests for new requirements.
- Keep the new firmware runnable at each milestone. Avoid extracting a shared
  framework or separate common library before a concrete need emerges.

### 5. Preparing documentation and distribution

- Rewrite README and control/MIDI documentation to match the new firmware.
- Retain applicable license texts, source copyright notices and third-party
  attribution. The root project is MIT licensed, while bundled components retain
  their respective licenses.
- Review `THIRD_PARTY.md` for inherited paths and descriptions that do not match
  this smaller repository; preserve relevant attribution while making descriptions
  accurate for the actual distributed contents.
- Give the new firmware its own name and describe CHOMPI hardware compatibility
  without suggesting an official CHOMPI Club release.
- Verify build, relevant tests, memory budgets, MIDI/SD behavior as selected, and
  controls/audio on the device before publishing a release.

## Dependencies

- Seeding the new repository and drafting the specification can proceed independently.
- Baseline validation requires the new clone.
- Instrument adaptation requires both the specification and verified baseline.
- Final documentation and release verification require the implemented behavior.

## Considerations

- Retaining history is useful provenance, but does not automatically synchronize
  future changes. Review and selectively import fixes from upstream when needed.
- Do not replace the vendored libDaisy with stock upstream as a cleanup step.
- Keep this project as a reference rather than asking an implementation session to
  turn its existing checkout into the new product.
- A useful implementation request in the new workspace is: "Implement this
  repository's specification using the retained CHOMPI hardware/build foundation.
  Reuse existing components where appropriate, preserve the platform setup, and
  replace only instrument-specific behavior."
