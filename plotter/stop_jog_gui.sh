#!/usr/bin/env bash
# Stop the PCB Plotter jog GUI.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PID_FILE="$SCRIPT_DIR/jog_gui.pid"

if [ ! -f "$PID_FILE" ]; then
    echo "No PID file found — jog GUI may not be running."
    exit 0
fi

PID=$(cat "$PID_FILE")

if kill -0 "$PID" 2>/dev/null; then
    kill "$PID"
    echo "Stopped jog GUI (PID $PID)."
else
    echo "Process $PID is not running."
fi

rm -f "$PID_FILE"
