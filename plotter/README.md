# PCB Plotter — README

A 2-axis PCB circuit plotter built on an Arduino Nano. It draws PCB traces on a single-layer copper board using a PCB marker pen on an XY stage driven by two 28BYJ-48 stepper motors. G-code is streamed from a host PC over USB serial.

---

## Hardware

| Component | Details |
|---|---|
| Controller | Arduino Nano |
| X-axis stepper | 28BYJ-48 via ULN2003 — pins 8, 10, 9, 11 (moves pen left/right) |
| Y-axis stepper | 28BYJ-48 via ULN2003 — pins 2, 4, 3, 5 (moves PCB board forward/back) |
| Pen-lift servo | MG90S 180° micro servo — pin 7 |
| Leadscrew pitch | 10 mm per revolution |
| Steps per mm | 409.6 (4096 half-steps / 10 mm) |

---

## Libraries Required

Install via the Arduino IDE Library Manager or Arduino CLI:

- [AccelStepper](https://www.airspayce.com/mikem/arduino/AccelStepper/) — stepper motor control with acceleration
- **Servo** — built into the Arduino IDE / AVR core

---

## Building & Uploading

```bash
# Compile
arduino-cli compile --fqbn arduino:avr:nano PCBPlotter.ino

# Upload (replace /dev/cu.usbserial-XXXX with your port)
arduino-cli upload -p /dev/cu.usbserial-XXXX --fqbn arduino:avr:nano PCBPlotter.ino
```

---

## Streaming G-code from a PC

Install the Python dependency once:

```bash
pip install pyserial
```

Then stream a G-code file:

```bash
python send_gcode.py circuit.gcode
```

Override the serial port or baud rate if needed:

```bash
python send_gcode.py circuit.gcode --port /dev/cu.usbserial-1420 --baud 115200
```

The script auto-detects the Arduino's port by looking for common USB-serial chips (CH340, CP210x, FTDI). The baud rate defaults to **115200** to match the firmware.

### Protocol

The plotter uses a simple line-by-line streaming protocol:

1. Host sends one G-code line terminated with `\n`
2. Plotter executes the command
3. Plotter replies with `ok`
4. Host sends the next line

Do **not** send the next line before receiving `ok` — the plotter has no input buffer for queued commands.

---

## Supported G-code Commands

### G0 — Rapid Move (pen up)

```
G0 X<mm> Y<mm>
```

Lifts the pen, then moves to the specified XY position at maximum speed. Use for repositioning between drawn segments.

**Example:**
```
G0 X10.0 Y5.0
```

---

### G1 — Linear Draw Move (pen down)

```
G1 X<mm> Y<mm>
```

Lowers the pen, then moves to the specified XY position, drawing a line. Both axes are synchronised so they arrive at the target simultaneously regardless of the distance each axis travels.

**Example:**
```
G1 X25.0 Y40.0
```

---

### G28 — Move to Home

```
G28
```

Lifts the pen, then moves both axes back to the origin **(X0, Y0)** and resets the internal position counter. Use this at the start or end of a job to return to the home corner.

**Example:**
```
G28
```

---

### G92 — Set Current Position as Origin

```
G92
```

Declares the current physical position as **(X0, Y0)** without moving the motors. Useful for setting a new drawing origin mid-job, or for zeroing the machine after manually jogging the stage to a start position.

**Example:**
```
G92
```

---

### G90 — Absolute Positioning (default)

```
G90
```

All X and Y coordinates are interpreted as absolute positions from the home/origin point (0, 0). This is the default mode on startup.

---

### G91 — Relative Positioning

```
G91
```

All X and Y coordinates are interpreted as offsets from the current position.

**Example** — draw a 10 × 10 mm square relative to wherever the pen currently is:
```
G91
G1 X10.0 Y0.0
G1 X0.0  Y10.0
G1 X-10.0 Y0.0
G1 X0.0  Y-10.0
G90
```

---

### M2 / M30 — End of Program

```
M2
```
or
```
M30
```

Lifts the pen and signals end of program. The plotter prints `Program end` on the serial connection. Always end your G-code file with one of these commands.

---

### Comments

Lines beginning with `;` or `(` are treated as comments and ignored. Inline comments after a `;` on a command line are also stripped before parsing.

```
; This is a full-line comment
G1 X10.0 Y5.0 ; this inline comment is stripped
```

---

## Coordinate System

- Origin **(0, 0)** is the home position — bottom-left of the drawing area as seen from above
- **+X** moves the pen to the right
- **+Y** moves the pen away from home (the board moves toward the motor)
- All coordinates are in **millimetres**

> **Note:** The Y-axis stepper physically moves the PCB board, not the pen. The firmware inverts the Y motor direction automatically so that G-code coordinates always describe the position of the pen *on the board*.

---

## Tuning

The following constants at the top of `PCBPlotter.ino` can be adjusted to match your physical build:

| Constant | Default | Description |
|---|---|---|
| `MAX_SPEED_MM_S` | `2.0` | Maximum axis speed in mm/s |
| `ACCEL_MM_S2` | `1.0` | Axis acceleration in mm/s² |
| `PEN_DOWN_ANGLE` | `90` | Servo angle when pen touches the board |
| `PEN_UP_ANGLE` | `60` | Servo angle when pen is lifted clear |
| `Y_INVERT` | `true` | Set to `false` if Y-axis draws in the wrong direction |

---

## Example G-code File

A minimal rectangle:

```gcode
G90
G0 X0 Y0
G1 X20 Y0
G1 X20 Y15
G1 X0 Y15
G1 X0 Y0
G0 X0 Y0
M2
```

A sample cat drawing (50 × 50 mm) is included in `circuit.gcode`.
