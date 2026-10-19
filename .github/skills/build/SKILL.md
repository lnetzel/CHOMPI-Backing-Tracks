---
name: build
description: 'Build the CHOMPI-PIPES firmware with make. Use when: building, compiling, rebuilding, making the firmware or .bin, checking SRAM/SDRAM memory usage, running make in code/src. Always reports SRAM, SRAM_EXEC and SDRAM usage in a table after every build.'
argument-hint: '[clean] [cpu-meter]'
---

# Build CHOMPI-PIPES Firmware

Builds the firmware per the "Building on Windows" section of the repo README and, after **every** build, presents a memory-usage table for the SRAM, SRAM_EXEC and SDRAM regions.

## Prerequisites (from README)

- **MSYS2** with an **UCRT64** shell and GNU Make (`pacman -S --needed make`).
- **Arm GNU Toolchain 10.3-2021.10** (`arm-none-eabi-gcc` 10.3.1) installed at `C:\armgnu\10.3-2021.10`. Exactly this version — the prebuilt libs and the SRAM budget are tuned to it.
- Everything else (libDaisy, DaisySP, coreJSON, prebuilt `.a` files) is vendored in `code/libs/`.

## Procedure

### 1. Run the build

Invoke the MSYS2 UCRT64 bash from the terminal so the right shell/environment is used. From PowerShell:

```powershell
C:\msys64\usr\bin\bash.exe -lc 'export PATH="/c/armgnu/10.3-2021.10/bin:$PATH"; cd /c/Dev/CHOMPI-Pipes/code/src && make -j4 DEBUG=0 OPT=-O3'
```

Variants:

- First build / toolchain sanity check: add `make check-toolchain` before the build (must print GCC 10.3.1).
- Clean rebuild: `make clean` first.
- CPU-meter build (user passes `cpu-meter`): append `PIPES_CPU_METER=1`.

If `C:\msys64\usr\bin\bash.exe` does not exist, locate the MSYS2 install (check `C:\msys64`, or ask the user) and adjust.

### 2. Verify success

Build succeeds when it completes with no errors and produces `code/src/build/CHOMPI_PIPES_1.0.bin` (BOOT_SRAM app type). On failure, show the relevant compiler/linker errors and stop.

### 3. ALWAYS report memory usage (required after every build)

The link step passes `-Wl,--print-memory-usage`, so the build output contains lines like:

```
Memory region         Used Size  Region Size  %age Used
       SRAM_EXEC:      161840 B       232 KB     70.52%
            SRAM:      204452 B       280 KB     71.33%
           SDRAM:      1234567 B        64 MB      1.88%
```

Parse those lines and present **exactly this table** (only these three regions, in this order):

| Region | Used | Size | % Used |
|--------|-----:|-----:|-------:|
| SRAM_EXEC (code) | ... B | 232 KB | ...% |
| SRAM (data/bss)  | ... B | 280 KB | ...% |
| SDRAM            | ... B | 64 MB  | ...% |

Add a one-line note if any region exceeds 90% used (SRAM is the usual bottleneck — "CHOMPI runs out of SRAM, not flash").

**Fallback** if the memory-usage lines are missing from the output: run

```powershell
C:\msys64\usr\bin\bash.exe -lc 'export PATH="/c/armgnu/10.3-2021.10/bin:$PATH"; arm-none-eabi-size -A /c/Dev/CHOMPI-Pipes/code/src/build/CHOMPI_PIPES_1.0.elf'
```

and sum section sizes by load address (per `code/src/chompi_sram.lds`):

| Region | Address range | Size |
|--------|---------------|------|
| SRAM_EXEC | 0x24000000–0x2403A000 | 232 KB |
| SRAM      | 0x2403A000–0x24080000 | 280 KB |
| SDRAM     | 0xC0000000–0xC4000000 | 64 MB |

## Notes

- Output binary: `code/src/build/CHOMPI_PIPES_1.0.bin` — install by copying it to the SD card root (delete other `.bin` files); do not flash the bootloader.
- Do **not** rebuild the vendored libDaisy/DaisySP libraries unless asked; the build links the committed prebuilt `.a` files.
- Native DSP tests are a separate target (`make -C code/tests test`) and are not part of the firmware build.
