"""
plotter_controller.py — Controller layer for the PCB Plotter jog GUI.

Wires the PlotterModel and PlotterView together. All serial I/O is
dispatched to a background thread so the GUI never blocks.
"""

import threading
from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from plotter_model import PlotterModel
    from plotter_view import PlotterView


class PlotterController:
    def __init__(self, model: "PlotterModel", view: "PlotterView") -> None:
        self._model = model
        self._view = view

        # Route model callbacks back to view thread-safely via after()
        model.set_position_callback(self._on_position_update)
        model.set_status_callback(self._on_status_update)
        model.set_progress_callback(self._on_progress_update)

        self._gcode_path: str = ""
        self._toolpath: list = []
        self._job_thread: threading.Thread | None = None

        view.set_controller(self)

        # Populate port list and set initial UI state
        self.on_refresh_ports()
        view.update_connected(False)

    # ------------------------------------------------------------------
    # Model → View callbacks (may arrive from background threads)
    # ------------------------------------------------------------------

    def _on_position_update(self, x: float, y: float) -> None:
        self._view.after(0, self._view.update_position, x, y)

    def _on_status_update(self, msg: str) -> None:
        self._view.after(0, self._view.update_status, msg)

    def _on_progress_update(self, done: int, total: int) -> None:
        self._view.after(0, self._view.update_progress, done, total)
        if self._toolpath:
            self._view.after(0, self._view.update_draw_progress, done, total)

    # ------------------------------------------------------------------
    # Helper: run a serial operation in a daemon thread
    # ------------------------------------------------------------------

    def _run(self, fn, *args) -> None:
        threading.Thread(target=fn, args=args, daemon=True).start()

    # ------------------------------------------------------------------
    # View → Controller button handlers
    # ------------------------------------------------------------------

    def on_refresh_ports(self) -> None:
        from plotter_model import PlotterModel
        ports = PlotterModel.available_ports()
        self._view.update_ports(ports)
        if not ports:
            self._view.update_status("No serial ports found — plug in the Arduino and click ↻")

    def on_connect_toggle(self) -> None:
        if self._model.connected:
            self._run(self._do_disconnect)
        else:
            port = self._view.get_port()
            baud = self._view.get_baud()
            if not port:
                self._view.show_error("No serial port selected.")
                return
            self._run(self._do_connect, port, baud)

    def _do_connect(self, port: str, baud: int) -> None:
        try:
            self._model.connect(port, baud)
            self._view.after(0, self._view.update_connected, True)
        except Exception as exc:
            self._view.after(0, self._view.show_error, str(exc))

    def _do_disconnect(self) -> None:
        try:
            self._model.disconnect()
        finally:
            self._view.after(0, self._view.update_connected, False)

    def on_jog(self, axis: str, direction: int) -> None:
        """Jog the given axis by ±step mm."""
        if not self._model.connected:
            return
        step = self._view.get_step() * direction
        if axis == "x":
            new_x = round(self._model.x + step, 3)
            new_y = self._model.y
        else:
            new_x = self._model.x
            new_y = round(self._model.y + step, 3)
        self._run(self._do_move, new_x, new_y)

    def on_home(self) -> None:
        if not self._model.connected:
            return
        self._run(self._do_home)

    def on_goto(self) -> None:
        if not self._model.connected:
            return
        coords = self._view.get_goto()
        if coords is None:
            self._view.show_error("Invalid coordinates — enter numeric X and Y values.")
            return
        self._run(self._do_move, *coords)

    def on_set_origin(self) -> None:
        if not self._model.connected:
            return
        self._run(self._do_set_origin)

    def on_browse(self) -> None:
        from tkinter import filedialog
        path = filedialog.askopenfilename(
            parent=self._view,
            title="Select G-code file",
            filetypes=[("G-code files", "*.gcode *.gc *.nc *.txt"),
                       ("All files", "*.*")],
        )
        if path:
            self._gcode_path = path
            self._view.update_gcode_path(path)
            self._view.update_progress(0, 0)
            try:
                from plotter_model import PlotterModel
                self._toolpath = PlotterModel.parse_toolpath(path)
                self._view.draw_preview(self._toolpath)
            except Exception as exc:
                self._view.update_status(f"Preview error: {exc}")

    def on_run_file(self) -> None:
        if not self._model.connected:
            self._view.show_error("Connect to the plotter before running a file.")
            return
        if not self._gcode_path:
            self._view.show_error("No G-code file loaded. Click Browse… first.")
            return
        self._view.update_running(True)
        self._job_thread = threading.Thread(
            target=self._do_run_file, daemon=True)
        self._job_thread.start()

    def on_stop_file(self) -> None:
        self._model.abort()

    def _do_run_file(self) -> None:
        try:
            self._model.run_gcode_file(self._gcode_path)
        except Exception as exc:
            self._view.after(0, self._view.show_error, str(exc))
        finally:
            self._view.after(0, self._view.update_running, False)

    # ------------------------------------------------------------------
    # Private serial operations (called in background threads)
    # ------------------------------------------------------------------

    def _do_move(self, x: float, y: float) -> None:
        try:
            self._model.move_to(x, y)
        except Exception as exc:
            self._view.after(0, self._view.show_error, str(exc))

    def _do_home(self) -> None:
        try:
            self._model.home()
        except Exception as exc:
            self._view.after(0, self._view.show_error, str(exc))

    def _do_set_origin(self) -> None:
        try:
            self._model.set_origin()
        except Exception as exc:
            self._view.after(0, self._view.show_error, str(exc))
