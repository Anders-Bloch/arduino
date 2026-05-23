"""
plotter_view.py — View layer for the PCB Plotter jog GUI.

Builds the tkinter UI and exposes public update methods that the
controller calls. No business logic or serial I/O lives here.

Layout
------
┌─────────────────────────────────────────────────────┐
│  Connection  [port ▼] [baud] [Connect] [↻ Refresh]  │
├──────────────┬──────────────────┬────────────────────┤
│  Position    │     Jog pad      │   Move To / Actions│
│  X: 000.000  │   [Y+]           │  X: [____]         │
│  Y: 000.000  │[X-][⌂][X+]       │  Y: [____] [Go]    │
│              │   [Y-]  step[__] │  [Set Origin (G92)]│
├──────────────┴──────────────────┴────────────────────┤
│  Status bar                                          │
└─────────────────────────────────────────────────────┘
"""

import tkinter as tk
from tkinter import filedialog, messagebox, ttk


class PlotterView(tk.Tk):
    def __init__(self) -> None:
        super().__init__()
        self.title("PCB Plotter — Jog Control")
        self.resizable(True, True)
        self.minsize(640, 800)
        self.columnconfigure(0, weight=1)
        self.rowconfigure(3, weight=1)  # preview row expands
        self._controller = None
        self._build_ui()

    # ------------------------------------------------------------------
    # Controller wiring
    # ------------------------------------------------------------------

    def set_controller(self, controller) -> None:
        self._controller = controller
        self._bind_callbacks()

    # ------------------------------------------------------------------
    # UI construction
    # ------------------------------------------------------------------

    def _build_ui(self) -> None:
        outer_pad = {"padx": 8, "pady": 4}
        self.columnconfigure(0, weight=1)
        self.columnconfigure(1, weight=1)
        self.columnconfigure(2, weight=1)

        # ── Connection frame ─────────────────────────────────────────
        conn = ttk.LabelFrame(self, text="Connection")
        conn.grid(row=0, column=0, columnspan=3, sticky="ew", **outer_pad)

        ttk.Label(conn, text="Port:").grid(row=0, column=0, padx=4, pady=4)
        self.port_var = tk.StringVar()
        self.port_combo = ttk.Combobox(conn, textvariable=self.port_var,
                                       width=22, state="readonly")
        self.port_combo.grid(row=0, column=1, padx=4, pady=4)

        ttk.Label(conn, text="Baud:").grid(row=0, column=2, padx=4, pady=4)
        self.baud_var = tk.StringVar(value="115200")
        ttk.Entry(conn, textvariable=self.baud_var, width=8).grid(
            row=0, column=3, padx=4, pady=4)

        self.connect_btn = ttk.Button(conn, text="Connect", width=10)
        self.connect_btn.grid(row=0, column=4, padx=4, pady=4)

        self.refresh_btn = ttk.Button(conn, text="↻ Refresh", width=10)
        self.refresh_btn.grid(row=0, column=5, padx=4, pady=4)

        # ── Position display ─────────────────────────────────────────
        pos = ttk.LabelFrame(self, text="Position")
        pos.grid(row=1, column=0, sticky="nsew", **outer_pad)

        for row_i, label, attr in ((0, "X:", "x_var"), (1, "Y:", "y_var")):
            ttk.Label(pos, text=label, font=("", 12, "bold")).grid(
                row=row_i, column=0, sticky="w", padx=8, pady=6)
            var = tk.StringVar(value="0.000")
            setattr(self, attr, var)
            ttk.Label(pos, textvariable=var,
                      font=("Courier", 16, "bold"), width=8,
                      anchor="e").grid(row=row_i, column=1, padx=4, pady=6)
            ttk.Label(pos, text="mm").grid(row=row_i, column=2,
                                           sticky="w", padx=2)

        # ── Jog pad ──────────────────────────────────────────────────
        jog = ttk.LabelFrame(self, text="Jog")
        jog.grid(row=1, column=1, sticky="nsew", **outer_pad)

        b = {"width": 5, "padding": 8}
        self.y_plus_btn  = ttk.Button(jog, text="Y +", **b)
        self.x_minus_btn = ttk.Button(jog, text="X −", **b)
        self.home_btn    = ttk.Button(jog, text=" ⌂ ", **b)
        self.x_plus_btn  = ttk.Button(jog, text="X +", **b)
        self.y_minus_btn = ttk.Button(jog, text="Y −", **b)

        self.y_plus_btn .grid(row=0, column=1, padx=4, pady=4)
        self.x_minus_btn.grid(row=1, column=0, padx=4, pady=4)
        self.home_btn   .grid(row=1, column=1, padx=4, pady=4)
        self.x_plus_btn .grid(row=1, column=2, padx=4, pady=4)
        self.y_minus_btn.grid(row=2, column=1, padx=4, pady=4)

        ttk.Label(jog, text="Step (mm):").grid(
            row=3, column=0, columnspan=2, sticky="e", pady=6)
        self.step_var = tk.StringVar(value="1.0")
        ttk.Combobox(jog, textvariable=self.step_var,
                     values=["0.1", "0.5", "1.0", "5.0", "10.0"],
                     width=6, state="readonly").grid(
            row=3, column=2, pady=6, padx=4)

        # ── Move to / actions ────────────────────────────────────────
        act = ttk.LabelFrame(self, text="Move To")
        act.grid(row=1, column=2, sticky="nsew", **outer_pad)

        ttk.Label(act, text="X (mm):").grid(row=0, column=0,
                                             sticky="w", padx=8, pady=4)
        self.goto_x_var = tk.StringVar(value="0.0")
        ttk.Entry(act, textvariable=self.goto_x_var, width=9).grid(
            row=0, column=1, padx=4, pady=4)

        ttk.Label(act, text="Y (mm):").grid(row=1, column=0,
                                             sticky="w", padx=8, pady=4)
        self.goto_y_var = tk.StringVar(value="0.0")
        ttk.Entry(act, textvariable=self.goto_y_var, width=9).grid(
            row=1, column=1, padx=4, pady=4)

        self.goto_btn = ttk.Button(act, text="Go", width=10)
        self.goto_btn.grid(row=2, column=0, columnspan=2, pady=6)

        ttk.Separator(act, orient="horizontal").grid(
            row=3, column=0, columnspan=2, sticky="ew", padx=4, pady=4)

        self.set_origin_btn = ttk.Button(act, text="Set Origin (G92)", width=18)
        self.set_origin_btn.grid(row=4, column=0, columnspan=2, pady=4)

        # ── G-code file ──────────────────────────────────────────────
        gcode = ttk.LabelFrame(self, text="G-code File")
        gcode.grid(row=2, column=0, columnspan=3, sticky="ew", **outer_pad)
        gcode.columnconfigure(1, weight=1)

        self.gcode_path_var = tk.StringVar(value="No file selected")
        ttk.Label(gcode, textvariable=self.gcode_path_var,
                  anchor="w", width=52, relief="sunken").grid(
            row=0, column=0, columnspan=2, padx=4, pady=4, sticky="ew")

        self.browse_btn = ttk.Button(gcode, text="Browse…", width=10)
        self.browse_btn.grid(row=0, column=2, padx=4, pady=4)

        self.progress_var = tk.IntVar(value=0)
        self.progress_bar = ttk.Progressbar(
            gcode, variable=self.progress_var,
            maximum=100, length=320, mode="determinate")
        self.progress_bar.grid(row=1, column=0, columnspan=2,
                               padx=4, pady=4, sticky="ew")
        self.progress_label_var = tk.StringVar(value="")
        ttk.Label(gcode, textvariable=self.progress_label_var, width=14,
                  anchor="e").grid(row=1, column=2, padx=4)

        self.run_btn  = ttk.Button(gcode, text="▶  Run",  width=12, state="disabled")
        self.stop_btn = ttk.Button(gcode, text="■  Stop", width=12, state="disabled")
        self.run_btn .grid(row=2, column=1, sticky="e", padx=4, pady=4)
        self.stop_btn.grid(row=2, column=2, padx=4, pady=4)

        # ── G-code preview canvas ──────────────────────────────────
        prev = ttk.LabelFrame(self, text="Preview")
        prev.grid(row=3, column=0, columnspan=3, sticky="nsew", **outer_pad)
        prev.columnconfigure(0, weight=1)
        prev.rowconfigure(0, weight=1)

        self._canvas_size = 420
        self.preview_canvas = tk.Canvas(
            prev, width=self._canvas_size, height=self._canvas_size,
            background="#1a1a1a", cursor="crosshair")
        self.preview_canvas.grid(row=0, column=0, padx=4, pady=4, sticky="nsew")
        self.preview_canvas.create_text(
            self._canvas_size // 2, self._canvas_size // 2,
            text="No file loaded", fill="#555555",
            font=("Helvetica", 12), tags="placeholder")

        # ── Status bar ───────────────────────────────────────────────
        self.status_var = tk.StringVar(value="Not connected")
        ttk.Label(self, textvariable=self.status_var,
                  relief="sunken", anchor="w").grid(
            row=4, column=0, columnspan=3, sticky="ew", padx=4, pady=(0, 4))

    def _bind_callbacks(self) -> None:
        c = self._controller
        self.connect_btn    .config(command=c.on_connect_toggle)
        self.refresh_btn    .config(command=c.on_refresh_ports)
        self.x_plus_btn     .config(command=lambda: c.on_jog("x", +1))
        self.x_minus_btn    .config(command=lambda: c.on_jog("x", -1))
        self.y_plus_btn     .config(command=lambda: c.on_jog("y", +1))
        self.y_minus_btn    .config(command=lambda: c.on_jog("y", -1))
        self.home_btn       .config(command=c.on_home)
        self.goto_btn       .config(command=c.on_goto)
        self.set_origin_btn .config(command=c.on_set_origin)
        self.browse_btn     .config(command=c.on_browse)
        self.run_btn        .config(command=c.on_run_file)
        self.stop_btn       .config(command=c.on_stop_file)

    # ------------------------------------------------------------------
    # Public update methods (called by the controller)
    # ------------------------------------------------------------------

    def update_position(self, x: float, y: float) -> None:
        self.x_var.set(f"{x:.3f}")
        self.y_var.set(f"{y:.3f}")

    def update_status(self, msg: str) -> None:
        self.status_var.set(msg)

    def update_ports(self, ports: list[str]) -> None:
        self.port_combo["values"] = ports
        if ports and not self.port_var.get():
            self.port_var.set(ports[0])

    def update_connected(self, connected: bool) -> None:
        self.connect_btn.config(text="Disconnect" if connected else "Connect")
        state = "normal" if connected else "disabled"
        for btn in (self.x_plus_btn, self.x_minus_btn,
                    self.y_plus_btn, self.y_minus_btn,
                    self.home_btn, self.goto_btn, self.set_origin_btn):
            btn.config(state=state)
        # Re-evaluate run button: needs both a connection and a loaded file
        if not connected:
            self.run_btn.config(state="disabled")
            self.stop_btn.config(state="disabled")
        elif self.gcode_path_var.get() not in ("", "No file selected"):
            self.run_btn.config(state="normal")

    def update_gcode_path(self, path: str) -> None:
        self.gcode_path_var.set(path)
        if self._controller and self._controller._model.connected:
            self.run_btn.config(state="normal")

    def update_progress(self, done: int, total: int) -> None:
        pct = int(done / total * 100) if total else 0
        self.progress_var.set(pct)
        self.progress_label_var.set(f"{done} / {total}")

    def update_running(self, running: bool) -> None:
        """Toggle Run/Stop buttons and disable jog controls while running."""
        self.run_btn .config(state="disabled" if running else "normal")
        self.stop_btn.config(state="normal"  if running else "disabled")
        jog_state = "disabled" if running else "normal"
        for btn in (self.x_plus_btn, self.x_minus_btn,
                    self.y_plus_btn, self.y_minus_btn,
                    self.home_btn, self.goto_btn, self.set_origin_btn):
            btn.config(state=jog_state)

    # ------------------------------------------------------------------
    # Value accessors (called by the controller)
    # ------------------------------------------------------------------

    def get_port(self) -> str:
        return self.port_var.get()

    def get_baud(self) -> int:
        try:
            return int(self.baud_var.get())
        except ValueError:
            return 115200

    def get_step(self) -> float:
        try:
            return float(self.step_var.get())
        except ValueError:
            return 1.0

    def get_goto(self) -> tuple[float, float] | None:
        try:
            return float(self.goto_x_var.get()), float(self.goto_y_var.get())
        except ValueError:
            return None

    def show_error(self, msg: str) -> None:
        messagebox.showerror("Error", msg, parent=self)

    def draw_preview(self, points: list[tuple[float, float, bool]]) -> None:
        """Render full toolpath on the preview canvas (all segments as 'pending')."""
        self._toolpath_cache = points
        self._render_toolpath(points, split_at=0)

    def update_draw_progress(self, done: int, total: int) -> None:
        """Re-render the canvas with segments split into done (bright) vs remaining (dim)."""
        if not hasattr(self, "_toolpath_cache") or not self._toolpath_cache:
            return
        pts = self._toolpath_cache
        # Map command progress fraction onto toolpath point index
        split_at = int(done / total * len(pts)) if total else 0
        self._render_toolpath(pts, split_at)

    def _render_toolpath(
        self,
        points: list[tuple[float, float, bool]],
        split_at: int,
    ) -> None:
        """Draw toolpath on canvas.
        Segments 1..split_at are 'done' (bright); split_at+1..end are 'remaining' (dim)."""
        c = self.preview_canvas
        c.delete("all")

        if not points:
            c.create_text(self._canvas_size // 2, self._canvas_size // 2,
                          text="No moves found", fill="#555555",
                          font=("Helvetica", 12))
            return

        xs = [p[0] for p in points]
        ys = [p[1] for p in points]
        min_x, max_x = min(xs), max(xs)
        min_y, max_y = min(ys), max(ys)
        span_x = max_x - min_x or 1.0
        span_y = max_y - min_y or 1.0
        pad = 20
        scale = (self._canvas_size - 2 * pad) / max(span_x, span_y)

        def to_canvas(x: float, y: float) -> tuple[float, float]:
            cx = pad + (x - min_x) * scale
            cy = self._canvas_size - pad - (y - min_y) * scale
            return cx, cy

        # Grid lines
        for v in range(int(min_x), int(max_x) + 1, max(1, int(span_x / 5))):
            x1, y1 = to_canvas(v, min_y)
            x2, y2 = to_canvas(v, max_y)
            c.create_line(x1, y1, x2, y2, fill="#2a2a2a")
        for v in range(int(min_y), int(max_y) + 1, max(1, int(span_y / 5))):
            x1, y1 = to_canvas(min_x, v)
            x2, y2 = to_canvas(max_x, v)
            c.create_line(x1, y1, x2, y2, fill="#2a2a2a")

        # Origin crosshair
        ox, oy = to_canvas(0, 0)
        c.create_line(ox - 6, oy, ox + 6, oy, fill="#444444", width=1)
        c.create_line(ox, oy - 6, ox, oy + 6, fill="#444444", width=1)

        # Toolpath segments
        prev_cx, prev_cy = to_canvas(points[0][0], points[0][1])
        for i, (x, y, pen_down) in enumerate(points[1:], start=1):
            cx, cy = to_canvas(x, y)
            done_seg = i <= split_at
            if pen_down:
                color = "#00cc66" if done_seg else "#1a5c35"   # bright / dim green
                c.create_line(prev_cx, prev_cy, cx, cy,
                              fill=color, width=1.5 if done_seg else 1.0)
            else:
                color = "#5555aa" if done_seg else "#2a2a44"   # bright / dim blue-grey
                c.create_line(prev_cx, prev_cy, cx, cy,
                              fill=color, width=1, dash=(3, 4))
            prev_cx, prev_cy = cx, cy

        # Current pen position marker (head of the 'done' region)
        if 0 < split_at <= len(points):
            px, py, _ = points[split_at - 1]
            hcx, hcy = to_canvas(px, py)
            c.create_oval(hcx - 5, hcy - 5, hcx + 5, hcy + 5,
                          fill="#ff4444", outline="#ffffff", width=1)

        # Home and start markers
        hx, hy = to_canvas(0, 0)
        c.create_oval(hx - 4, hy - 4, hx + 4, hy + 4, fill="#ffaa00", outline="")
        sx, sy = to_canvas(points[0][0], points[0][1])
        c.create_oval(sx - 3, sy - 3, sx + 3, sy + 3, fill="#ffffff", outline="")

        # Size label
        c.create_text(pad, self._canvas_size - 6,
                      text=f"{span_x:.1f} × {span_y:.1f} mm",
                      fill="#888888", anchor="sw", font=("Helvetica", 9))
