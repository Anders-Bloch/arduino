"""
send_gcode.py — Stream a G-code file to the PCB plotter (PCBPlotter.ino) over serial.

Usage:
    python send_gcode.py <gcode_file> [--port PORT] [--baud BAUD]

Examples:
    python send_gcode.py circuit.gcode
    python send_gcode.py circuit.gcode --port /dev/cu.usbserial-1420 --baud 115200

The script sends one line at a time and waits for 'ok' before sending the next,
matching the streaming protocol implemented in PCBPlotter.ino.

Requirements:
    pip install pyserial
"""

import argparse
import sys
import time
import serial


def find_arduino_port() -> str:
    """Return the first serial port that looks like an Arduino Nano."""
    import serial.tools.list_ports
    candidates = serial.tools.list_ports.comports()
    for port in candidates:
        desc = (port.description or "").lower()
        hwid = (port.hwid or "").lower()
        if any(k in desc or k in hwid for k in ("arduino", "ch340", "cp210", "ftdi", "usbserial")):
            return port.device
    # Fall back to the first available port
    if candidates:
        return candidates[0].device
    raise RuntimeError("No serial port found. Use --port to specify one.")


def stream_gcode(filepath: str, port: str, baud: int) -> None:
    print(f"Opening {port} at {baud} baud …")
    with serial.Serial(port, baud, timeout=30) as ser:
        # Give the Arduino time to reset after the serial connection opens
        time.sleep(2)

        # Flush any startup message (e.g. "PCB plotter ready …")
        ser.reset_input_buffer()

        with open(filepath, "r") as f:
            lines = f.readlines()

        total = len(lines)
        sent  = 0

        for raw_line in lines:
            line = raw_line.strip()

            # Skip blank lines and comment-only lines locally
            if not line or line.startswith(";") or line.startswith("("):
                continue

            sent += 1
            print(f"[{sent}/{total}] Sending: {line}")
            ser.write((line + "\n").encode("ascii"))

            # Wait for 'ok' acknowledgement from the plotter
            while True:
                response = ser.readline().decode("ascii", errors="replace").strip()
                if response:
                    print(f"         → {response}")
                if response.lower() == "ok":
                    break
                if "program end" in response.lower():
                    print("Plotter signalled end of program.")
                    return

    print("Done — all lines sent.")


def main() -> None:
    parser = argparse.ArgumentParser(description="Stream G-code to the PCB plotter over serial.")
    parser.add_argument("file",           help="Path to the G-code file")
    parser.add_argument("--port", "-p",   help="Serial port (auto-detected if omitted)")
    parser.add_argument("--baud", "-b",   type=int, default=115200, help="Baud rate (default: 115200)")
    args = parser.parse_args()

    port = args.port or find_arduino_port()

    try:
        stream_gcode(args.file, port, args.baud)
    except FileNotFoundError:
        print(f"Error: G-code file '{args.file}' not found.", file=sys.stderr)
        sys.exit(1)
    except serial.SerialException as exc:
        print(f"Serial error: {exc}", file=sys.stderr)
        sys.exit(1)
    except KeyboardInterrupt:
        print("\nInterrupted by user.")
        sys.exit(0)


if __name__ == "__main__":
    main()
