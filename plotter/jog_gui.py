"""
jog_gui.py — Entry point for the PCB Plotter jog GUI.

Usage:
    python jog_gui.py

Requires:
    pip install pyserial
"""

from plotter_controller import PlotterController
from plotter_model import PlotterModel
from plotter_view import PlotterView


def main() -> None:
    model = PlotterModel()
    view = PlotterView()
    PlotterController(model, view)
    try:
        view.mainloop()
    finally:
        model.disconnect()


if __name__ == "__main__":
    main()
