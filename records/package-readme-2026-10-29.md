<p align="right"><a href="package-readme-2026-10-29.zh_CN.md">简体中文</a> · <strong>English</strong></p>

# ClassTrack V1.1 complete project package

Package date: 2026-10-29  
Target: FoloToy AI Passport, ESP32-C3, 8 MB Flash, no PSRAM  
Framework: ESP-IDF 5.5.3

This is the latest ClassTrack project snapshot. It is independent and does not modify the user's family-education GitHub repository.

The package contains all application and BSP source, font/assets, pinned managed-component source, the seven-screen prototype and screenshots, the native foreground-only Mac app and installer, tests, documentation, and the exact verified firmware/debug bundle. See the Chinese version for the detailed inventory and acceptance status.

## Status

Implemented and host-verified: 240 x 320 layout, Chinese glyph coverage, streaming ADPCM, per-class replacement transactions, time/date UI, live recording counter logic, CSV validation, USB protocol, deep-sleep contract, and CW2017 false-zero filtering.

Device-observed: the exact merged image was flashed and hash-verified; display, ES8311, ADC buttons, and CW2017 initialized; USB status, manual Mac time sync, and six-class CSV import succeeded through the command-line manager; battery reported a stable 92% at 4097 mV.

Still unverified on hardware: the new graphical Mac app's time-sync and class-upload buttons against an awake device, the live counter's physical display, end-to-end capture/playback quality, power-loss phases, bank compaction, final display typography, USB backup, deep-sleep current/wake behavior, long-duration drift, and long-term battery stability. A restore command is not implemented. By design there is no persistent Mac auto-sync agent; the Mac app runs only when opened.

Build SHA-256: `1d1fa86c0b662e15feff2eb8c577652752f53f9f66511dc618e47b194998e0be`.

See [`test-record-2026-10-29.md`](test-record-2026-10-29.md) and [`docs/class-track/mac-manager.md`](../docs/class-track/mac-manager.md).

