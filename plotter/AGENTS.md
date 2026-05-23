# AGENTS.md

## Project Overview

This is an Arduino sketch project. The entry point is `PCBPlotter.ino`.

## Structure

- `PCBPlotter.ino` — Main sketch with `setup()` and `loop()` functions

## Arduino Sketch Conventions

- `setup()` runs once on power-on or reset. Use it to initialise pins, serial, and peripherals.
- `loop()` runs repeatedly after `setup()` completes. Place the main program logic here.
- Use `Serial.begin(<baud>)` in `setup()` and `Serial.println()` in `loop()` for debug output.

## Building & Uploading

Use the [Arduino IDE](https://www.arduino.cc/en/software) or the [Arduino CLI](https://arduino.github.io/arduino-cli/):

```bash
# Compile
arduino-cli compile --fqbn arduino:avr:uno PCBPlotter.ino

# Upload (replace /dev/ttyUSB0 with your port)
arduino-cli upload -p /dev/ttyUSB0 --fqbn arduino:avr:uno PCBPlotter.ino
```

## Agent Guidelines

- Always preserve the `setup()` and `loop()` entry points.
- Do not introduce platform-specific libraries without noting the required board/FQBN.
- Keep `delay()` calls explicit and documented so timing behaviour is clear.
- Prefer `Serial.println()` for debug output over other I/O methods unless a display is available.
