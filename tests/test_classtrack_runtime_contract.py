#!/usr/bin/env python3
"""Static runtime contracts backed by failures observed on physical hardware."""

from pathlib import Path
import re
import unittest


ROOT = Path(__file__).resolve().parents[1]


class ClassTrackRuntimeContractTests(unittest.TestCase):
    def test_usb_protocol_task_has_printf_headroom(self) -> None:
        source = (ROOT / "main" / "main.c").read_text(encoding="utf-8")
        match = re.search(r"#define\s+CT_USB_TASK_STACK_BYTES\s+(\d+)", source)
        self.assertIsNotNone(match, "USB task stack budget must be explicit")
        self.assertGreaterEqual(
            int(match.group(1)),
            6144,
            "physical-device HELLO reply overflowed a 4096-byte ct_usb stack",
        )
        self.assertRegex(
            source,
            r'xTaskCreate\(serial_task,\s*"ct_usb",\s*CT_USB_TASK_STACK_BYTES',
        )

    def test_recording_is_not_blocked_by_missing_wall_clock(self) -> None:
        source = (ROOT / "main" / "main.c").read_text(encoding="utf-8")
        start = source.index("static void start_recording(void)")
        end = source.index("static void enter_deep_sleep(void)")
        recording_function = source[start:end]
        self.assertNotIn("recording refused", recording_function)
        self.assertNotRegex(recording_function, r"current_unix_time\(\)\s*==\s*0\s*\)\s*\{[^}]*return")
        self.assertIn("recording without synchronized timestamp", recording_function)

    def test_recording_uses_click_to_start_and_click_to_stop(self) -> None:
        source = (ROOT / "main" / "main.c").read_text(encoding="utf-8")
        self.assertIn("button == BSP_BTN_OK && event == BSP_BTN_CLICK) ct_audio_stop_record()", source)
        self.assertNotIn("event == BSP_BTN_LONG) {\n        start_recording()", source)

    def test_usb_payloads_are_received_completely_before_processing(self) -> None:
        source = (ROOT / "main" / "main.c").read_text(encoding="utf-8")
        self.assertIn("static size_t serial_read_exact", source)
        self.assertIn("received += chunk", source)
        self.assertIn("serial_read_exact(csv, command.length", source)
        self.assertIn("if (line[0] == '\\0') continue", source)
        self.assertIn("!s_usb_busy", source)

    def test_recording_screen_polls_live_audio_progress(self) -> None:
        source = (ROOT / "main" / "main.c").read_text(encoding="utf-8")
        self.assertIn("pdMS_TO_TICKS(250)", source)
        self.assertIn("unsigned seconds = ct_audio_pending_seconds()", source)
        self.assertIn("if (seconds != s_record_seconds)", source)

    def test_china_timezone_and_sync_redraw_are_explicit(self) -> None:
        source = (ROOT / "main" / "main.c").read_text(encoding="utf-8")
        ui = (ROOT / "main" / "ct_ui.c").read_text(encoding="utf-8")
        self.assertIn('setenv("TZ", "CST-8", 1)', source)
        self.assertIn("APP_INPUT_TIME_SYNC", source)
        self.assertIn("%04d年%02d月%02d日", ui)


if __name__ == "__main__":
    unittest.main()
