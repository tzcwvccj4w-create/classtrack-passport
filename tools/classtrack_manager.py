#!/usr/bin/env python3
"""Offline Mac/desktop companion for ClassTrack over USB Serial/JTAG."""

from __future__ import annotations

import argparse
import csv
import json
import pathlib
import struct
import sys
import time
import zlib

TOOLS_DIR = pathlib.Path(__file__).resolve().parent
if str(TOOLS_DIR) not in sys.path:
    sys.path.insert(0, str(TOOLS_DIR))

from classtrack_native_serial import NativeSerial


def validate_csv(path: pathlib.Path) -> bytes:
    raw = path.read_bytes()
    if len(raw) > 4096:
        raise ValueError("CSV must be 4096 bytes or smaller")
    text = raw.decode("utf-8-sig")
    rows = list(csv.DictReader(text.splitlines()))
    if not rows or set(rows[0]) != {"id", "name", "course"}:
        raise ValueError("CSV header must be exactly: id,name,course")
    if len(rows) > 12:
        raise ValueError("ClassTrack supports at most 12 classes")
    ids: set[int] = set()
    for row in rows:
        class_id = int(row["id"])
        if class_id not in range(12) or class_id in ids:
            raise ValueError("class IDs must be unique integers from 0 to 11")
        ids.add(class_id)
        if not row["name"].strip() or len(row["name"].encode()) >= 40:
            raise ValueError(f"invalid class name for id {class_id}")
        if len(row["course"].encode()) >= 40:
            raise ValueError(f"course name too long for id {class_id}")
    return text.encode("utf-8")


class Device:
    def __init__(self, serial_port):
        self.serial = serial_port

    def _protocol_line(self) -> str:
        deadline = time.monotonic() + 4
        while time.monotonic() < deadline:
            line = self.serial.readline().decode("utf-8", "replace").strip()
            if line.startswith("CT "):
                return line[3:]
        raise RuntimeError("device did not respond")

    def command(self, line: str) -> str:
        self.serial.write((line + "\n").encode())
        response = self._protocol_line()
        if not response.startswith("OK"):
            raise RuntimeError(response or "device did not respond")
        return response

    def hello(self) -> dict:
        response = self.command("HELLO")
        return json.loads(response.removeprefix("OK "))

    def sync_time(self, epoch: int | None = None) -> None:
        self.command(f"TIME {epoch or int(time.time())}")

    def import_csv(self, raw: bytes) -> None:
        crc = zlib.crc32(raw) & 0xFFFFFFFF
        self.serial.write(f"IMPORT {len(raw)} {crc:08x}\n".encode() + raw)
        response = self._protocol_line()
        if response != "OK IMPORT":
            raise RuntimeError(response or "CSV import failed")

    def list_records(self) -> list[dict]:
        response = self.command("LIST")
        return json.loads(response.removeprefix("OK "))

    def get_record(self, class_id: int) -> tuple[dict, bytes]:
        self.serial.write(f"GET {class_id}\n".encode())
        header = self._protocol_line()
        if not header.startswith("AUDIO "):
            raise RuntimeError(header or "backup failed")
        _, length, crc_hex, sample_rate, sample_count, unix_time = header.split()
        payload = self.serial.read(int(length))
        if len(payload) != int(length) or (zlib.crc32(payload) & 0xFFFFFFFF) != int(crc_hex, 16):
            raise RuntimeError("backup length/CRC mismatch")
        if self._protocol_line() != "DONE":
            raise RuntimeError("missing backup terminator")
        return {
            "class_id": class_id,
            "sample_rate": int(sample_rate),
            "sample_count": int(sample_count),
            "unix_time": int(unix_time),
            "crc32": crc_hex,
            "codec": "IMA_ADPCM_BLOCK_256",
        }, payload


def open_serial(port: str):
    return NativeSerial(port, 115200, timeout=4)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True, help="ClassTrack USB serial device")
    sub = parser.add_subparsers(dest="action", required=True)
    sub.add_parser("status")
    sub.add_parser("sync-time")
    import_parser = sub.add_parser("import-classes")
    import_parser.add_argument("csv", type=pathlib.Path)
    backup_parser = sub.add_parser("backup")
    backup_parser.add_argument("directory", type=pathlib.Path)
    args = parser.parse_args(argv)

    with open_serial(args.port) as port:
        device = Device(port)
        if args.action == "status":
            print(json.dumps(device.hello(), ensure_ascii=False, indent=2))
        elif args.action == "sync-time":
            device.sync_time()
            print("Time synchronized.")
        elif args.action == "import-classes":
            device.import_csv(validate_csv(args.csv))
            print("Classes imported.")
        elif args.action == "backup":
            args.directory.mkdir(parents=True, exist_ok=True)
            manifest = []
            for record in device.list_records():
                metadata, payload = device.get_record(int(record["class_id"]))
                filename = f"class-{metadata['class_id']:02d}.adpcm"
                (args.directory / filename).write_bytes(payload)
                metadata["file"] = filename
                manifest.append(metadata)
            (args.directory / "manifest.json").write_text(
                json.dumps(manifest, ensure_ascii=False, indent=2), encoding="utf-8"
            )
            print(f"Backed up {len(manifest)} recording(s).")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
