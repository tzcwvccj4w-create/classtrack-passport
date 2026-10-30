<p align="right"><a href="README.zh_CN.md">简体中文</a> · <strong>English</strong></p>

# ClassTrack for FoloToy AI Passport

<p align="center">
  <a href="https://classtrack-passport.liaofely.chatgpt.site"><strong>Product site and complete guide</strong></a> ·
  <a href="https://github.com/tzcwvccj4w-create/classtrack-passport">Source</a> ·
  <a href="https://classtrack-passport.liaofely.chatgpt.site/downloads/ClassTrack-Manager-macOS.dmg">Mac manager</a>
</p>

ClassTrack ("Where did the class get to?") is an offline teaching-progress recorder for the ESP32-C3 FoloToy AI Passport. It keeps one latest recording per class, records a timestamp, supports review before replacement, and exchanges time, class CSV data, and backups with a Mac over USB serial.

This independent project is based on the official FoloToy `ai-passport` BSP snapshot downloaded on 2026-10-26. It does not modify or depend on the user's family-education repository.

> ClassTrack is a community project for FoloToy AI Passport, not an official FoloToy product.

## Use it

1. Use Up and Down to choose a class, then press OK.
2. Press OK to start recording and press it again to stop. Each note is limited to 20 seconds.
3. Review the note, then save, re-record, or discard it.
4. Open the [Mac manager](https://classtrack-passport.liaofely.chatgpt.site/downloads/ClassTrack-Manager-macOS.dmg) only when you need to sync time or upload the class list. Closing its window exits the app; it does not remain in the background.

Class lists are UTF-8 CSV files with the exact `id,name,course` header:

```csv
id,name,course
0,class-1,Vocational English
1,class-2,Chinese Listening
```

For the verified build identity and completion status, start with
[`records/test-record-2026-10-29.md`](records/test-record-2026-10-29.md). The Mac app is documented in
[`docs/class-track/mac-manager.md`](docs/class-track/mac-manager.md).

## Current first-stage increment

- A corrected 240 x 320 browser prototype with seven screenshot-tested pages.
- An ESP-IDF 5.5.3 application shell using the official display, button, ES8311, battery, shared-I2C, and deep-sleep BSP contracts.
- Streaming IMA ADPCM blocks at 16 kHz without a 20-second PCM allocation.
- Two-bank append-only Flash storage: a new recording is invisible until its final commit marker is valid; interrupted recording or save leaves the old recording selected.
- Up to 12 classes, 20 seconds each, with two spare transaction slots per bank.
- USB serial protocol, a foreground-only native Mac app for time sync and CSV import, and a command-line ADPCM backup tool.
- Host tests for codec bounds, interrupted replacement, bank compaction, layout rectangles, CSV validation, and the manager protocol.

## Validate

```text
./tools/validate_classtrack.sh --host
./tools/validate.sh --firmware
```

Firmware validation requires an activated ESP-IDF v5.5.3 environment. Building never flashes a connected device.

See `docs/class-track/architecture.md` for the storage transaction, memory budget, protocol, sleep sequence, and remaining device acceptance work.

## License and project status

ClassTrack is available under the [MIT License](LICENSE). The current firmware has been built, flashed, and observed through basic boot and USB checks. End-to-end audio quality, staged power-loss behavior, deep-sleep current, and long-duration operation still require complete device acceptance. See the [2026-10-29 test record](records/test-record-2026-10-29.md) for the exact boundary.
