#!/usr/bin/env bash
# Start the PCB Plotter jog GUI using the project virtual environment.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

if [ ! -f ".venv/bin/python" ]; then
    echo "Virtual environment not found. Run:"
    echo "  python3 -m venv .venv && .venv/bin/pip install pyserial"
    exit 1
fi

echo "Starting jog GUI…"
nohup .venv/bin/python jog_gui.py > jog_gui.log 2>&1 &
echo $! > jog_gui.pid
echo "Started (PID $(cat jog_gui.pid)). Log: $SCRIPT_DIR/jog_gui.log"
