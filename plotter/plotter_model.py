"""
plotter_model.py — Model layer for the PCB Plotter jog GUI.

Manages the serial connection to the Arduino and tracks the current
machine position. All G-code is sent here; callers receive results
via registered callbacks so the GUI layer stays decoupled.
"""

import threading
import time
from typing import Callable, Optional

import serial
import serial.tools.list_ports

_GCODE_COMMENT_PREFIXES = (";", "(")


class PlotterModel:
    def __init__(self) -> None:
        self._serial: Optional[serial.Serial] = None
        self._lock = threading.Lock()
        self._connected = False
        self._x = 0.0
        self._y = 0.0

        self._on_position_change: Optional[Callable[[float, float], None]] = None
        self._on_status_change: Optional[Callable[[str], None]] = None
        self._on_progress_change: Optional[Callable[[int, int], None]] = None
        self._abort_flag = threading.Event()

    # ------------------------------------------------------------------
    # Callback registration
    # ------------------------------------------------------------------

    def set_position_callback(self, cb: Callable[[float, float], None]) -> None:
        """Called with (x, y) whenever the tracked position changes."""
        self._on_position_change = cb

    def set_status_callback(self, cb: Callable[[str], None]) -> None:
        """Called with a human-readable status string."""
        self._on_status_change = cb

    def set_progress_callback(self, cb: Callable[[int, int], None]) -> None:
        """Called with (lines_done, total_lines) as the file executes."""
        self._on_progress_change = cb

    # ------------------------------------------------------------------
    # Properties
    # ------------------------------------------------------------------

    @property
    def connected(self) -> bool:
        return self._connected

    @property
    def x(self) -> float:
        return self._x

    @property
    def y(self) -> float:
        return self._y

    # ------------------------------------------------------------------
    # Port discovery
    # ------------------------------------------------------------------

    @staticmethod
    def available_ports() -> list[str]:
        """Return all available serial port device names."""
        return [p.device for p in serial.tools.list_ports.comports()]

    # ------------------------------------------------------------------
    # Connection management
    # ------------------------------------------------------------------

    def connect(self, port: str, baud: int = 115200) -> None:
        """Open the serial port and wait for the Arduino to boot."""
        self._serial = serial.Serial(port, baud, timeout=10)
        time.sleep(2)  # wait for Arduino reset after DTR toggle
        self._serial.reset_input_buffer()
        self._connected = True
        self._notify_status(f"Connected to {port} at {baud} baud")

    def disconnect(self) -> None:
        """Close the serial port."""
        if self._serial and self._serial.is_open:
            self._serial.close()
        self._connected = False
        self._notify_status("Disconnected")

    # ------------------------------------------------------------------
    # G-code primitives
    # ------------------------------------------------------------------

    def _send(self, line: str) -> None:
        """Send one G-code line and block until 'ok' is received.
        Must be called with self._lock held."""
        self._serial.write((line.strip() + "\n").encode("ascii"))
        while True:
            raw = self._serial.readline()
            if not raw:
                raise TimeoutError(f"No response to: {line}")
            response = raw.decode("ascii", errors="replace").strip()
            if response.lower() == "ok":
                return
            if response:
                self._notify_status(f"plotter: {response}")

    def move_to(self, x: float, y: float) -> None:
        """Rapid move (pen up) to absolute position (x, y) in mm."""
        with self._lock:
            self._send(f"G0 X{x:.3f} Y{y:.3f}")
        self._x = x
        self._y = y
        self._notify_position()

    def home(self) -> None:
        """Execute G28 — move to (0, 0) with pen up."""
        with self._lock:
            self._send("G28")
        self._x = 0.0
        self._y = 0.0
        self._notify_position()
        self._notify_status("Homed to (0.000, 0.000)")

    def set_origin(self) -> None:
        """Execute G92 — declare current position as (0, 0)."""
        with self._lock:
            self._send("G92")
        self._x = 0.0
        self._y = 0.0
        self._notify_position()
        self._notify_status("Origin set at current position")

    def run_gcode_file(self, filepath: str) -> None:
        """Stream every G-code line in *filepath*, one at a time.
        Blocks until finished or aborted. Call from a background thread."""
        self._abort_flag.clear()

        with open(filepath, "r") as fh:
            raw_lines = fh.readlines()

        # Count only non-blank, non-comment lines for progress reporting
        commands = [
            l.strip() for l in raw_lines
            if l.strip() and not l.strip().startswith(_GCODE_COMMENT_PREFIXES)
        ]
        total = len(commands)
        self._notify_progress(0, total)
        self._notify_status(f"Running {filepath} ({total} commands)…")

        for i, line in enumerate(commands, start=1):
            if self._abort_flag.is_set():
                self._notify_status("Job aborted.")
                self._notify_progress(i - 1, total)
                return
            with self._lock:
                self._send(line)
            self._notify_progress(i, total)

        self._notify_status(f"Job complete — {total} commands sent.")

    def abort(self) -> None:
        """Signal the running G-code job to stop after the current line."""
        self._abort_flag.set()

    @staticmethod
    def parse_toolpath(filepath: str) -> list[tuple[float, float, bool]]:
        """Parse a G-code file and return a list of (x, y, pen_down) points.
        pen_down=True means a G1 (draw) move; False means a G0 (rapid) move."""
        points: list[tuple[float, float, bool]] = []
        x, y = 0.0, 0.0
        absolute = True

        def _param(line: str, ch: str, default: float) -> float:
            idx = line.find(ch)
            if idx == -1:
                return default
            end = idx + 1
            while end < len(line) and (line[end] in "0123456789.+-"):
                end += 1
            try:
                return float(line[idx + 1:end])
            except ValueError:
                return default

        with open(filepath, "r") as fh:
            for raw in fh:
                line = raw.strip().upper()
                # strip inline comments
                semi = line.find(";")
                if semi != -1:
                    line = line[:semi].strip()
                if not line or line.startswith("("):
                    continue

                if "G90" in line:
                    absolute = True
                    continue
                if "G91" in line:
                    absolute = False
                    continue
                if "G92" in line or "G28" in line:
                    x, y = 0.0, 0.0
                    points.append((x, y, False))
                    continue

                g = None
                gi = line.find("G")
                if gi != -1:
                    try:
                        g = int(line[gi + 1:gi + 3].split()[0])
                    except (ValueError, IndexError):
                        pass

                if g in (0, 1):
                    nx = _param(line, "X", x if absolute else 0.0)
                    ny = _param(line, "Y", y if absolute else 0.0)
                    tx = nx if absolute else x + nx
                    ty = ny if absolute else y + ny
                    points.append((tx, ty, g == 1))
                    x, y = tx, ty

        return points

    # ------------------------------------------------------------------
    # Internal helpers
    # ------------------------------------------------------------------

    def _notify_position(self) -> None:
        if self._on_position_change:
            self._on_position_change(self._x, self._y)

    def _notify_status(self, msg: str) -> None:
        if self._on_status_change:
            self._on_status_change(msg)

    def _notify_progress(self, done: int, total: int) -> None:
        if self._on_progress_change:
            self._on_progress_change(done, total)
