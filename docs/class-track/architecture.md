<p align="right"><a href="architecture.zh_CN.md">简体中文</a> · <strong>English</strong></p>

# ClassTrack V1.1 architecture

## Fixed product behavior

- Fully offline; no Wi-Fi, Bluetooth, cloud account, or model runtime.
- Up to 12 stable class IDs (`0..11`), imported as UTF-8 CSV.
- One visible recording per class, 16 kHz mono IMA ADPCM, normally 15 seconds and hard-limited to 20 seconds.
- A new recording remains provisional through review. Confirming save atomically makes it visible; discard, reset, or power loss leaves the previous committed recording selected.
- The header shows the full date and time in China Standard Time. Recording remains available after a cold boot with an invalid clock; those records show an unsynchronized timestamp, while USB synchronization redraws immediately and subsequent records store wall-clock time. The recording screen updates elapsed seconds live.

## Flash layout and transaction

| Range | Size | Purpose |
| --- | ---: | --- |
| NVS + PHY | 64 KiB | Class CSV, last-sync marker, ESP-IDF data |
| Factory app | 2.94 MiB | ClassTrack plus the official FoloToy BSP |
| `classdata` | 4.94 MiB | Two equal transactional audio banks |
| End reserve | 64 KiB | Kept outside the declared partitions |

Each audio bank has one 4 KiB header and fourteen fixed 168 KiB slots. Twelve slots cover the maximum class count; two spare slots absorb replacement/aborted transactions. A slot contains a checksummed header, streaming ADPCM payload, and a fixed-position checksummed trailer. The four-byte commit marker is written last. Boot scanning ignores any slot without a valid header, trailer, commit marker, and payload CRC.

When a bank has no free slot, the latest committed recording for every class is copied to the erased inactive bank. The new bank header is committed only after all copies validate. A reset before that point keeps the old bank active. The old bank is not erased until it becomes the target of a later compaction.

At 16 kHz, one independent 256-sample ADPCM block uses at most 132 bytes. Twenty seconds occupies 165,000 bytes. Encoding and playback use one 512-byte PCM block plus one 132-byte encoded block, rather than a 640 KiB PCM recording buffer.

## USB serial protocol

The Mac manager and device use an ASCII control channel with CRC-protected binary payloads. Device responses start with `CT ` so the manager can ignore boot logs.

```text
HELLO
TIME <unix-seconds>
IMPORT <byte-count> <crc32-hex>\n<UTF-8 CSV bytes>
LIST
GET <class-id>
```

`GET` returns an `AUDIO` header, exactly the declared ADPCM bytes, and `CT DONE`. The backup manifest records codec, sample rate, sample count, timestamp, payload CRC, and file name. Import loops until the declared payload length is received, then validates its CRC, schema, IDs, UTF-8 lengths, duplicates, and actual LVGL font coverage before replacing the NVS class blob. USB transfers inhibit automatic deep sleep.

The native [Mac manager](mac-manager.md) combines explicit time sync and class import in one foreground-only app. It installs no login item or background service and terminates when its window closes.

## Power sequence

After 30 seconds without input, and never during recording/playback/Flash work, the application checks that the ADC key is released. It then hands GPIO0 back to digital input, arms low-level key wake, suspends CW2017 before ES8311, stops/releases I2S, releases shared I2C, waits for the current LVGL flush, puts ST7789P3/backlight pins into their terminal sleep state, and enters deep sleep. A wake restarts the application. The held wake key must still be acceptance-tested on hardware.

## 240 x 320 UI and fonts

The browser prototype and firmware both reserve the top 44 pixels for the two-line date/time status and the bottom 29-30 pixels for fixed key hints. The review page places its four actions above a separate two-line note region; neither can enter the footer. Browser automation measures all seven pages at exactly 240 x 320 and checks safe-area bounds, measured overflow, and element intersections.

Firmware uses a custom 16 px subset generated from Source Han Sans SC and explicitly assigns it to every label. CSV import rejects names containing glyphs absent from that exact active font. This prevents silently accepting a class name that would render as boxes, but final pixel appearance still requires the physical display test.

CW2017 startup requires three consecutive stable SOC samples. A transient 0% is rejected while cell voltage remains above 3.6 V, and large runtime jumps require three matching readings before display, preventing an occasional 90%-to-0% flash.

## Required device acceptance

1. Verify all fixed Chinese strings and imported sample names on ST7789P3, including rounded corners, long names, selected states, and the review note/footer gap.
2. Measure UP/DOWN/OK ADC voltages and confirm one OK click starts recording and a second OK click stops it.
3. Record, preview, discard, confirm, reboot during capture, and interrupt power before commit; confirm the previous recording survives every uncommitted case.
4. Listen for correct sample rate, non-zero microphone data, intelligible ADPCM playback, and Flash-save crackles.
5. Fill enough slots to trigger bank compaction, reset at each compaction phase, and verify the last committed record per class.
6. Exercise time sync, CSV import, backup, cold-boot invalid-time behavior, and a restore tool before claiming backup/restore completeness.
7. Measure active, screen-off, and deep-sleep current for 24 hours, 72 hours, and one week; verify key wake and time drift.
