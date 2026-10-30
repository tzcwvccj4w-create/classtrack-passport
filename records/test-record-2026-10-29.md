<p align="right"><a href="test-record-2026-10-29.zh_CN.md">简体中文</a> · <strong>English</strong></p>

# ClassTrack pre-package test record

Date: 2026-10-29 (Asia/Shanghai)

- Required repository skill links: PASS.
- Current ClassTrack host gate: PASS.
- Verified firmware/debug archive: PASS.
- Seven-page 240 x 320 browser layout test: PASS.
- Native arm64 Mac manager: PASS for self-test, pseudo-serial protocol, time command, CSV import, signed app inside ZIP/DMG, installer integrity, and observed foreground exit.
- ESP-IDF build: PASS for the retained matching build produced on 2026-10-27; not rebuilt on 2026-10-29.
- Full upstream static gate: NOT RUN in this packaging pass because Apple's system `git` is blocked by an unaccepted Xcode license. No earlier result is represented as a current run.
- Existing device evidence from the command-line manager: exact firmware flash/hash, boot, display/ES8311/ADC/CW2017 initialization, USB status, manual Mac time sync, and six-class CSV import passed. The new graphical app buttons were not re-tested against live hardware in this pass.
- Full capture/playback, power-loss, compaction, backup, sleep-current, wake, duration, drift, and final physical-UI acceptance: NOT RUN.

Firmware SHA-256: `1d1fa86c0b662e15feff2eb8c577652752f53f9f66511dc618e47b194998e0be`.

