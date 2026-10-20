# Authorship and Third-Party Notices

`backing-tracks` is an independent firmware project seeded from CHOMPI-Pipes commit `1e2bc676184d5ddb5330bc844c5471b1e372bbe8`. The project retains the source repository's MIT license and relevant copyright and license notices. CHOMPI hardware compatibility does not mean this is an official CHOMPI Club release.

## Vendored components

| Component | Location | Notice |
|---|---|---|
| CHOMPI-adapted libDaisy | `code/libs/libDaisy/` | Electrosmith, MIT; retain its bundled license and source notices. Do not replace this CHOMPI adaptation with stock libDaisy as a cleanup step. |
| coreJSON | `code/libs/coreJSON/` | FreeRTOS/Amazon, MIT; retain its bundled license and source notices. |
| CMSIS, STM32 HAL, USB middleware, FatFs | `code/libs/libDaisy/Drivers/` and middleware directories | Retain per-file ST/ARM notices and bundled license texts. |
| CHOMPI bootloader binary | `bin/` | Electrosmith Daisy Bootloader-derived CHOMPI distribution; retain the included attribution and license material. The firmware build does not replace or modify the bootloader. |
| Encoder support | `code/src/encoder.*` | Hardware support inherited from the CHOMPI-Pipes source; the encoder decoding is influenced by Mutable Instruments encoder classes. Retain existing source notices. |

The vendored libraries retain their original contents, license files, and copyright headers. Only the application target in `code/src/Makefile` selects `backing_tracks_main.cpp`; inherited organ code is not linked into the `backing-tracks` firmware.
