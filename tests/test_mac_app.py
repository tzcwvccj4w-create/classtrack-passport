import os
import pathlib
import pty
import select
import sys
import tempfile
import threading
import time
import zlib


ROOT = pathlib.Path(__file__).resolve().parents[1]
TOOLS = ROOT / "tools"
sys.path.insert(0, str(TOOLS))

import classtrack_manager as manager


class DeviceSimulator:
    def __init__(self):
        self.master, slave = pty.openpty()
        self.port = os.ttyname(slave)
        os.close(slave)
        self.commands = []
        self.imported = b""
        self.stop = threading.Event()
        self.thread = threading.Thread(target=self.run, daemon=True)
        self.thread.start()

    def close(self):
        self.stop.set()
        self.thread.join(timeout=1)
        os.close(self.master)

    def run(self):
        buffer = bytearray()
        expected = None
        crc = None
        while not self.stop.is_set():
            readable, _, _ = select.select([self.master], [], [], 0.1)
            if not readable:
                continue
            try:
                chunk = os.read(self.master, 4096)
            except OSError:
                continue
            if not chunk:
                continue
            buffer.extend(chunk)
            if expected is not None and len(buffer) >= expected:
                self.imported = bytes(buffer[:expected])
                del buffer[:expected]
                assert zlib.crc32(self.imported) & 0xFFFFFFFF == crc
                os.write(self.master, b"CT OK IMPORT\n")
                expected = None
            while expected is None and b"\n" in buffer:
                line, _, rest = buffer.partition(b"\n")
                buffer[:] = rest
                command = line.decode()
                self.commands.append(command)
                if command == "HELLO":
                    os.write(self.master, b"boot log\nCT OK {\"app\":\"ClassTrack\",\"protocol\":1}\n")
                elif command.startswith("TIME "):
                    os.write(self.master, b"CT OK TIME\n")
                elif command.startswith("IMPORT "):
                    _, length, crc_hex = command.split()
                    expected = int(length)
                    crc = int(crc_hex, 16)
            if expected is not None and len(buffer) >= expected:
                self.imported = bytes(buffer[:expected])
                del buffer[:expected]
                assert zlib.crc32(self.imported) & 0xFFFFFFFF == crc
                os.write(self.master, b"CT OK IMPORT\n")
                expected = None


def main():
    simulator = DeviceSimulator()
    try:
        with manager.open_serial(simulator.port) as port:
            device = manager.Device(port)
            assert device.hello()["app"] == "ClassTrack"
            device.sync_time(1793293200)

        with tempfile.TemporaryDirectory() as directory:
            path = pathlib.Path(directory) / "classes.csv"
            path.write_text("id,name,course\n0,26机电2班,高职英语\n", encoding="utf-8")
            raw = manager.validate_csv(path)
            with manager.open_serial(simulator.port) as port:
                manager.Device(port).import_csv(raw)
            deadline = time.monotonic() + 1
            while not simulator.imported and time.monotonic() < deadline:
                time.sleep(0.01)
            assert simulator.imported == raw

        assert "HELLO" in simulator.commands
        assert "TIME 1793293200" in simulator.commands
        assert any(command.startswith("IMPORT ") for command in simulator.commands)
        source = (TOOLS / "mac_manager" / "ClassTrackManager.m").read_text(encoding="utf-8")
        plist = (TOOLS / "mac_manager" / "Info.plist").read_text(encoding="utf-8")
        build = (TOOLS / "mac_manager" / "build_app.sh").read_text(encoding="utf-8")
        assert "applicationShouldTerminateAfterLastWindowClosed" in source
        assert "return YES" in source
        assert "LaunchAgent" not in source
        assert "LSBackgroundOnly" not in plist and "LSUIElement" not in plist
        assert "ClassTrackManager.m" in build and "-framework Cocoa" in build
        assert "app.py" not in build and "python3" not in build
        print("PASS: foreground native Mac app, serial protocol, time sync, and class import")
    finally:
        simulator.close()


if __name__ == "__main__":
    main()

