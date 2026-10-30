<p align="right"><a href="validation-2026-10-27.zh_CN.md">Simplified Chinese</a> · <strong>English</strong></p>

# ClassTrack V1.1 validation report

> Historical note: this is the retained validation snapshot from 2026-10-27. Use the more conservative
> [`test-record-2026-10-29.md`](test-record-2026-10-29.md) as the current packaging status; it does not mark unfinished device work as passed.

Date: 2026-10-27  
Target: FoloToy AI Passport (ESP32-C3, 8 MB Flash, no PSRAM, ST7789P3, ES8311, and three GPIO0 ADC buttons)

## Summary

| Category | Result | Evidence |
| --- | --- | --- |
| 240 x 320 prototype | PASS | All seven pages were measured at the real resolution with no safe-area violation, vertical overflow, or element overlap. |
| ClassTrack host tests | PASS | ADPCM boundaries and round trips, interrupted replacement, bank compaction, CSV/protocol, layout rectangles, and Mac manager validation passed. |
| FoloToy static/host gate | PASS | The retained run passed the repository text, BSP, button wake, deep sleep, audio, display, battery, and firmware-archive suites. |
| ESP-IDF firmware build | PASS | ESP-IDF 5.5.3 for ESP32-C3; the 776,304-byte app left about 75% of the factory partition free. |
| Merged image verification | PASS | `FoloToy-AI-Passport-full.bin` was 841,840 bytes and verified from address `0x0`. |
| USB device and flash | PASS | `/dev/cu.usbmodem1101`; ESP32-C3 rev1.1; 8 MB Flash; device-side hash verification passed after writing. |
| Boot initialization | PASS | The 240 x 320 display, ES8311, three ADC buttons, and CW2017 initialized; battery stabilized at 92% / 4097 mV. |
| Mac management channel | PASS | `HELLO`, China Standard Time synchronization, and six-row UTF-8 CSV import received successful device responses. |
| Physical recording counter | USER CHECK | The implementation polls every 250 ms and redraws each second; the real display still needed visual confirmation for the date and 00:01 / 00:02 transitions. |

## Implemented scope

- Separated the discard action, simulation note, and fixed footer on the review-before-saving screen.
- Checked safe areas, Chinese glyphs, long text, and overflow; added the project CJK font subset.
- Bundled six class/course rows from the supplied schedule.
- Changed recording to press OK once to start and once to stop, with a 20-second limit and an uncalibrated-time state.
- Added live elapsed-second redraws, the full date/time header in China Standard Time, and immediate redraw after USB sync.
- Added stable CW2017 sampling and false-zero filtering.
- Implemented 16 kHz mono streaming IMA ADPCM without a full 20-second PCM buffer.
- Implemented two-bank Flash transactions that retain the old committed recording until final commit.
- Added recent updates, timestamps, USB time sync, CSV import, and recording backup.
- Added the 30-second idle deep-sleep path and GPIO0 wake guard contract.

## Firmware identity

- Merged image SHA-256: `1d1fa86c0b662e15feff2eb8c577652752f53f9f66511dc618e47b194998e0be`
- App image SHA-256: `c7ce23d6756ef87753609119890057e788ddafe5a1bbae7f3ffe01d7bc465aa8`
- ELF SHA-256: `4b4a229fd870e0f8c999fdd849b12711ccc5832de1c0f860a359c3780cecb0a8`

## Remaining device validation

1. Final date typography, long class names, and footer spacing on the physical ST7789P3.
2. Physical 00:01 / 00:02 counter transitions and reliable second-press stop behavior.
3. ES8311 microphone input, intelligible ADPCM playback, volume, and Flash-write noise.
4. Power loss during recording, commit, and bank compaction.
5. End-to-end USB time sync, CSV import, and backup.
6. Deep-sleep current, button wake, long-duration operation, and clock drift.

The work was completed in an independent directory and did not modify the family-education repository.
