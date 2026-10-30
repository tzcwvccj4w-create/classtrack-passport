#!/usr/bin/env python3
"""Small dependency-free serial transport for ClassTrack on macOS/POSIX."""

from __future__ import annotations

import glob
import os
import select
import termios
import time


class SerialError(RuntimeError):
    """A user-facing serial connection failure."""


def list_candidate_ports() -> list[str]:
    """Return likely USB serial devices without opening or resetting them."""
    patterns = (
        "/dev/cu.usbmodem*",
        "/dev/cu.usbserial*",
    )
    return sorted({port for pattern in patterns for port in glob.glob(pattern)})


class NativeSerial:
    """Subset of pyserial used by the ClassTrack protocol client."""

    def __init__(self, port: str, baudrate: int = 115200, timeout: float = 4.0):
        if baudrate != 115200:
            raise ValueError("ClassTrack requires 115200 baud")
        self.port = port
        self.timeout = timeout
        self._buffer = bytearray()
        self._fd = -1
        try:
            self._fd = os.open(port, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
            self._configure()
        except OSError as exc:
            self.close()
            raise SerialError(f"无法打开设备端口 {port}: {exc.strerror or exc}") from exc

    def _configure(self) -> None:
        attributes = termios.tcgetattr(self._fd)
        attributes[0] = 0
        attributes[1] = 0
        attributes[2] = termios.CS8 | termios.CLOCAL | termios.CREAD
        attributes[3] = 0
        attributes[4] = termios.B115200
        attributes[5] = termios.B115200
        attributes[6][termios.VMIN] = 0
        attributes[6][termios.VTIME] = 0
        termios.tcsetattr(self._fd, termios.TCSANOW, attributes)
        termios.tcflush(self._fd, termios.TCIOFLUSH)

    def __enter__(self) -> "NativeSerial":
        return self

    def __exit__(self, exc_type, exc, traceback) -> None:
        self.close()

    def close(self) -> None:
        if self._fd >= 0:
            try:
                os.close(self._fd)
            finally:
                self._fd = -1

    def _require_open(self) -> None:
        if self._fd < 0:
            raise SerialError("设备连接已经关闭")

    def write(self, data: bytes) -> int:
        self._require_open()
        view = memoryview(data)
        deadline = time.monotonic() + self.timeout
        written = 0
        while view:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise SerialError("向设备发送数据超时")
            _, writable, _ = select.select([], [self._fd], [], remaining)
            if not writable:
                continue
            try:
                count = os.write(self._fd, view)
            except BlockingIOError:
                continue
            written += count
            view = view[count:]
        return written

    def _fill(self, deadline: float) -> bool:
        self._require_open()
        while time.monotonic() < deadline:
            readable, _, _ = select.select([self._fd], [], [], max(0.0, deadline - time.monotonic()))
            if not readable:
                return False
            try:
                chunk = os.read(self._fd, 4096)
            except BlockingIOError:
                continue
            if chunk:
                self._buffer.extend(chunk)
                return True
        return False

    def readline(self) -> bytes:
        deadline = time.monotonic() + self.timeout
        while True:
            newline = self._buffer.find(b"\n")
            if newline >= 0:
                line = bytes(self._buffer[: newline + 1])
                del self._buffer[: newline + 1]
                return line
            if not self._fill(deadline):
                line = bytes(self._buffer)
                self._buffer.clear()
                return line

    def read(self, size: int) -> bytes:
        if size < 0:
            raise ValueError("size must be non-negative")
        deadline = time.monotonic() + self.timeout
        while len(self._buffer) < size and self._fill(deadline):
            pass
        result = bytes(self._buffer[:size])
        del self._buffer[:size]
        return result

