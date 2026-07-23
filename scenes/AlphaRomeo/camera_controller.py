"""
camera_controller.py  —  Integrated pyluxcore render controller for the Alpha Romeo scene.

No external process launches. The render session runs in-process:
  - Camera orbit/zoom  → scene edit followed by a debounced session restart
  - Exposure change    → config.Parse + session restart           (fast, no window close)
  - Pipeline switch    → flip display index only                  (instant, no restart)

Controls:
  Left-drag on render viewport → pan
  Right-drag on render viewport → orbit (azimuth / elevation)
  Left-drag on controller       → pan
  Right-drag on controller      → orbit (azimuth / elevation)
  Scroll wheel                 → zoom
  R                       → reset camera
  Space                   → force film refresh
"""

import os, sys, math, re, queue, threading, json
from array import array

# ── Bootstrap pyluxcore ────────────────────────────────────────────────────────
LUXCORE_ROOT   = r"C:\Users\gcroc\Projects\LuxCore"
PYLUXCORE_PATH = os.path.join(LUXCORE_ROOT, r"out\build\src\pyluxcore\Release")
LUXCORE_BIN    = os.path.join(LUXCORE_ROOT, r"out\install\Release\bin")
SCENE_FILE     = os.path.join(LUXCORE_ROOT, r"scenes\AlphaRomeo\ModoAlphaRomeo.scn")
CFG_FILE       = os.path.join(LUXCORE_ROOT, r"scenes\AlphaRomeo\ModoAlphaRomeo.cfg")

SCENE_DIR = os.path.join(LUXCORE_ROOT, r"scenes\AlphaRomeo")
SETTINGS_FILE = os.path.join(SCENE_DIR, "camera_controller_settings.json")

os.add_dll_directory(LUXCORE_BIN)
sys.path.insert(0, PYLUXCORE_PATH)
os.chdir(SCENE_DIR)  # .scn and .ply paths are relative to the scene directory

import pyluxcore
from PIL import Image, ImageTk
import tkinter as tk
from tkinter import filedialog, ttk

# ── Defaults ──────────────────────────────────────────────────────────────────
DEFAULT_AZ          = 80.0
DEFAULT_EL          = 5.0
DEFAULT_SWITCH_SECS = 5
DEFAULT_TARGET      = [0.0, 2.084, 0.833]
FILM_W              = 1280
FILM_H              = 720
RENDER_RESOLUTIONS  = ("640 x 360", "1280 x 720", "1920 x 1080",
                       "2560 x 1440", "3840 x 2160")
CONTROL_W           = 216
CONTROL_H           = 120
PREVIEW_W           = 192
PREVIEW_H           = 108
CAMERA_FOV_DEG      = 45.0
REFRESH_MS          = 250
PREVIEW_RESTART_MS  = 75
PREVIEW_REFRESH_MS  = 75
PREVIEW_MIN_PASSES  = 2
FULL_RESTART_MS     = 400
WORLD_UP            = [0.0, 0.0, 1.0]
WINDOW_TITLE        = "nPower Software LuxCore Renderer"
WINDOW_ICON         = r"D:\nPowerSoftware.com\NewImages\HexigonLogoFLat.png"

# ── Math helpers ──────────────────────────────────────────────────────────────
def _norm(v):
    l = math.sqrt(sum(c*c for c in v))
    return [c/l for c in v] if l > 1e-10 else v

def _cross(a, b):
    return [a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0]]

def _dot(a, b):
    return sum(a[i]*b[i] for i in range(3))

def orbit(target, dist, az_deg, el_deg):
    az = math.radians(az_deg)
    el = math.radians(max(-89, min(89, el_deg)))
    return [target[0] + dist*math.cos(el)*math.sin(az),
            target[1] + dist*math.cos(el)*math.cos(az),
            target[2] + dist*math.sin(el)]

def cam_axes(orig, target):
    fwd   = _norm([target[i]-orig[i] for i in range(3)])
    right = _norm(_cross(fwd, WORLD_UP))
    up    = _norm(_cross(right, fwd))
    return right, up, fwd

def proj_axis(ax, cr, cu, scale=28):
    return _dot(ax, cr)*scale, -_dot(ax, cu)*scale

# ── Exposure I/O ──────────────────────────────────────────────────────────────
def read_exposure(path):
    with open(path) as f:
        for line in f:
            m = re.match(r"film\.imagepipelines\.0\.0\.prescale\s*=\s*([\d.eE+\-]+)", line.strip())
            if m:
                return 1.0 / max(float(m.group(1)), 1e-6)
            m = re.match(r"film\.imagepipelines\.0\.0\.exposure\s*=\s*([\d.eE+\-]+)", line.strip())
            if m:
                return float(m.group(1))
    return 1.0

def _read_controller_settings():
    try:
        with open(SETTINGS_FILE, encoding="utf-8") as settings_file:
            settings = json.load(settings_file)
            return settings if isinstance(settings, dict) else {}
    except (OSError, ValueError):
        return {}

def _setting_float(settings, name, default, minimum, maximum):
    try:
        return max(minimum, min(maximum, float(settings.get(name, default))))
    except (TypeError, ValueError):
        return default

def _setting_target(settings):
    target = settings.get("target")
    if isinstance(target, (list, tuple)) and len(target) == 3:
        try:
            return [float(value) for value in target]
        except (TypeError, ValueError):
            pass
    return list(DEFAULT_TARGET)
def _setting_hdr_file(settings):
    hdr_file = settings.get("hdr_file")
    if (isinstance(hdr_file, str) and hdr_file.lower().endswith(".hdr")
            and os.path.isfile(hdr_file)):
        return os.path.normpath(hdr_file)
    return None

def _ignore_luxcore_log(_message):
    pass

# ── Controller ────────────────────────────────────────────────────────────────
class CameraController(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title(WINDOW_TITLE)
        self.resizable(False, False)
        self._window_icon = tk.PhotoImage(file=WINDOW_ICON)
        self.iconphoto(True, self._window_icon)
        self._settings = _read_controller_settings()
        self._settings_ready = False
        self._hdr_file = _setting_hdr_file(self._settings)
        self._hdr_drop_wndprocs = {}
        self._hdr_drop_callback = None
        self._hdr_drop_api = None
        saved_resolution = self._settings.get("render_resolution")
        if saved_resolution not in RENDER_RESOLUTIONS:
            saved_resolution = "1280 x 720"
        self.az = tk.DoubleVar(value=_setting_float(
            self._settings, "azimuth", DEFAULT_AZ, -3600.0, 3600.0))
        self.el = tk.DoubleVar(value=_setting_float(
            self._settings, "elevation", DEFAULT_EL, -89.0, 89.0))
        self.dist = tk.DoubleVar(value=_setting_float(
            self._settings, "distance", 20.0, 1.0, 50.0))
        self.exposure = tk.DoubleVar(value=_setting_float(
            self._settings, "exposure", read_exposure(CFG_FILE), 0.001, 20.0))
        self.switch_sec = tk.IntVar(value=round(_setting_float(
            self._settings, "auto_oidn_seconds", DEFAULT_SWITCH_SECS, 1.0, 120.0)))
        self.render_resolution = tk.StringVar(value=saved_resolution)
        self._render_width, self._render_height = (
            int(value) for value in saved_resolution.split(" x "))
        self.pipeline   = 0

        self._drag_x    = 0
        self._drag_y    = 0
        self._switch_id = None
        self._skip_frames = 0
        self._gate_pass   = 0   # minimum pass count before showing new frame
        self._target = _setting_target(self._settings)
        self._session   = None
        self._config    = None
        self._scene     = None
        self._restart_lock = threading.Lock()
        self._restart_results = queue.Queue()
        self._camera_restart_pending = False
        self._camera_revision = 0
        self._camera_snapshot = None
        self._preview_restart_id = None
        self._preview_restart_in_progress = False
        self._full_restart_id = None
        self._session_mode = "full"
        self._render_stopped = False
        self._render_backend = "PATHOCL / OptiX"
        self._luxcore_initialized = False

        self._film_buf  = array('f', [0.0] * (FILM_W * FILM_H * 3))
        self._rgba_buf  = array('b', [0]   * (FILM_W * FILM_H * 4))
        self._preview_film_buf = array('f', [0.0] * (PREVIEW_W * PREVIEW_H * 3))
        self._preview_rgba_buf = array('b', [0]   * (PREVIEW_W * PREVIEW_H * 4))
        self._tk_image  = None
        self._last_film_image = None

        # One window: controller panel on the left, render viewport on the right.
        self._main_frame = tk.Frame(self)
        self._main_frame.pack()
        self._control_panel = tk.Frame(self._main_frame)
        self._control_panel.grid(row=0, column=0, sticky="ns")
        self._render_canvas = tk.Canvas(self._main_frame,
                                        width=FILM_W, height=FILM_H, bg="black",
                                        cursor="fleur")
        self._render_canvas.grid(row=0, column=1)
        self._render_win = self
        # Create a single persistent image item — updated in place, no ghosting
        self._canvas_img_id = self._render_canvas.create_image(0, 0, anchor=tk.NW)
        # Left-drag pans the camera in the render viewport.
        self._render_canvas.bind("<Button-1>",       self._pan_start)
        self._render_canvas.bind("<B1-Motion>",      self._pan_move)
        # Right-drag rotates the camera in the render viewport.
        self._render_canvas.bind("<Button-3>",       self._drag_start)
        self._render_canvas.bind("<B3-Motion>",      self._drag_move)
        self._render_canvas.bind("<MouseWheel>",     self._render_scroll)
        self._render_canvas.bind("<Button-4>",       self._render_scroll)
        self._render_canvas.bind("<Button-5>",       self._render_scroll)
        self._pan_drag_x = 0
        self._pan_drag_y = 0

        self._build_ui()
        self._settings_ready = True
        self.switch_sec.trace_add("write", self._on_setting_variable_changed)
        self.protocol("WM_DELETE_WINDOW", self._on_close)
        self._update_info()
        self.after(25, self._process_restart_results)
        self.after(100, self._enable_hdr_file_drop)
        self.after(300, self._start_session)

    def _save_settings(self):
        if not self._settings_ready:
            return
        settings = {
            "azimuth": self.az.get(),
            "elevation": self.el.get(),
            "distance": self.dist.get(),
            "target": self._target,
            "exposure": self.exposure.get(),
            "auto_oidn_seconds": self.switch_sec.get(),
            "render_resolution": self.render_resolution.get(),
        }
        if self._hdr_file:
            settings["hdr_file"] = self._hdr_file
        temp_file = SETTINGS_FILE + ".tmp"
        try:
            with open(temp_file, "w", encoding="utf-8") as settings_file:
                json.dump(settings, settings_file, indent=2)
            os.replace(temp_file, SETTINGS_FILE)
        except OSError:
            try:
                os.remove(temp_file)
            except OSError:
                pass

    def _on_setting_variable_changed(self, *_):
        self._save_settings()

    def _on_close(self):
        self._save_settings()
        if not self._render_stopped:
            self._stop_rendering()
        self.destroy()
    # ── HDR file drop ────────────────────────────────────────────────────────
    def _enable_hdr_file_drop(self):
        """Accept .hdr files dropped on the root window or render canvas."""
        if os.name != "nt":
            return
        try:
            import ctypes
            from ctypes import wintypes

            user32 = ctypes.WinDLL("user32", use_last_error=True)
            shell32 = ctypes.WinDLL("shell32", use_last_error=True)
            long_ptr = ctypes.c_ssize_t
            wndproc_type = ctypes.WINFUNCTYPE(
                long_ptr, wintypes.HWND, wintypes.UINT, wintypes.WPARAM,
                wintypes.LPARAM)

            user32.SetWindowLongPtrW.argtypes = [
                wintypes.HWND, ctypes.c_int, long_ptr]
            user32.SetWindowLongPtrW.restype = long_ptr
            user32.CallWindowProcW.argtypes = [
                long_ptr, wintypes.HWND, wintypes.UINT, wintypes.WPARAM,
                wintypes.LPARAM]
            user32.CallWindowProcW.restype = long_ptr
            shell32.DragAcceptFiles.argtypes = [wintypes.HWND, wintypes.BOOL]
            shell32.DragQueryFileW.argtypes = [
                wintypes.HANDLE, wintypes.UINT, wintypes.LPWSTR, wintypes.UINT]
            shell32.DragQueryFileW.restype = wintypes.UINT
            shell32.DragFinish.argtypes = [wintypes.HANDLE]

            wm_dropfiles = 0x0233
            gwl_wndproc = -4

            def window_proc(hwnd, message, wparam, lparam):
                try:
                    if message == wm_dropfiles:
                        hdrop = wintypes.HANDLE(wparam)
                        try:
                            file_count = shell32.DragQueryFileW(
                                hdrop, 0xFFFFFFFF, None, 0)
                            hdr_file = None
                            for index in range(file_count):
                                length = shell32.DragQueryFileW(
                                    hdrop, index, None, 0)
                                path_buffer = ctypes.create_unicode_buffer(length + 1)
                                shell32.DragQueryFileW(
                                    hdrop, index, path_buffer, length + 1)
                                candidate = os.path.normpath(path_buffer.value)
                                if candidate.lower().endswith(".hdr"):
                                    hdr_file = candidate
                                    break
                        finally:
                            shell32.DragFinish(hdrop)

                        if hdr_file:
                            self.after_idle(self._on_hdr_file_drop, hdr_file)
                        else:
                            self.after_idle(
                                self._show_hdr_drop_error,
                                "Drop a Radiance HDR file (.hdr) on the render viewport")
                        return 0
                except Exception:
                    pass

                previous_proc = self._hdr_drop_wndprocs.get(int(hwnd))
                return user32.CallWindowProcW(
                    previous_proc, hwnd, message, wparam, lparam)

            self._hdr_drop_callback = wndproc_type(window_proc)
            self._hdr_drop_api = (user32, shell32)
            for hwnd in (self.winfo_id(), self._render_canvas.winfo_id()):
                previous_proc = user32.SetWindowLongPtrW(
                    hwnd, gwl_wndproc,
                    long_ptr(ctypes.cast(self._hdr_drop_callback,
                                         ctypes.c_void_p).value))
                if previous_proc:
                    self._hdr_drop_wndprocs[int(hwnd)] = previous_proc
                    shell32.DragAcceptFiles(hwnd, True)
        except Exception:
            self._show_hdr_drop_error("HDR file drop could not be enabled")

    def _show_hdr_drop_error(self, message):
        self._info.config(text=message)
        self._render_win.title(f"{WINDOW_TITLE} — {message}")

    def _on_hdr_file_drop(self, hdr_file):
        hdr_file = os.path.normpath(hdr_file)
        if not hdr_file.lower().endswith(".hdr") or not os.path.isfile(hdr_file):
            self._show_hdr_drop_error("Dropped HDR file is unavailable")
            return

        if self._render_stopped or not self._scene or not self._session:
            self._hdr_file = hdr_file
            self._save_settings()
            self._render_win.title(
                f"{WINDOW_TITLE} — HDRI selected: {os.path.basename(hdr_file)}")
            return

        self._camera_revision += 1
        self._camera_snapshot = self._capture_camera_snapshot()
        self._camera_restart_pending = True
        for attr in ("_preview_restart_id", "_full_restart_id"):
            timer_id = getattr(self, attr)
            if timer_id:
                self.after_cancel(timer_id)
                setattr(self, attr, None)
        self._render_win.title(
            f"{WINDOW_TITLE} — Loading HDRI: {os.path.basename(hdr_file)}")
        threading.Thread(
            target=self._do_restart_session,
            args=(self._render_width, self._render_height, "full",
                  self._camera_snapshot, hdr_file),
            daemon=True).start()

    # ── UI ────────────────────────────────────────────────────────────────────
    def _build_ui(self):
        panel = self._control_panel
        pad = dict(padx=6, pady=2)
        scale_len = CONTROL_W - 16

        self._info = tk.Label(panel, text="Starting...", font=("Consolas", 8),
                              fg="#444", justify=tk.LEFT, anchor="w")

        tk.Label(panel, text="Distance").grid(row=1, column=0, sticky="w", **pad)
        tk.Scale(panel, from_=1, to=50, resolution=0.1, orient=tk.HORIZONTAL,
                 variable=self.dist, length=scale_len, showvalue=False,
                 command=lambda _: self._on_camera()).grid(row=2, column=0, **pad)

        tk.Label(panel, text="Exposure").grid(row=3, column=0, sticky="w", **pad)
        tk.Scale(panel, from_=0.001, to=20.0, resolution=0.01, orient=tk.HORIZONTAL,
                 variable=self.exposure, length=scale_len, showvalue=False,
                 command=lambda _: self._on_exposure()).grid(row=4, column=0, **pad)

        pip_frame = tk.Frame(panel)
        pip_frame.grid(row=5, column=0, pady=(2, 0))
        tk.Button(pip_frame, text="Raw", width=8,
                  command=lambda: self._set_pipeline(0)).pack(side=tk.LEFT, padx=2)
        tk.Button(pip_frame, text="OIDN", width=8,
                  command=lambda: self._set_pipeline(1)).pack(side=tk.LEFT, padx=2)

        delay_frame = tk.Frame(panel)
        delay_frame.grid(row=6, column=0, pady=(2, 0))
        tk.Label(delay_frame, text="Auto OIDN").pack(side=tk.LEFT, padx=(2, 3))
        tk.Spinbox(delay_frame, from_=1, to=120, textvariable=self.switch_sec,
                   width=3, font=("Segoe UI", 8)).pack(side=tk.LEFT)
        tk.Label(delay_frame, text="sec").pack(side=tk.LEFT, padx=(3, 2))
        resolution_frame = tk.Frame(panel)
        resolution_frame.grid(row=7, column=0, pady=(3, 0))
        tk.Label(resolution_frame, text="Resolution").pack(side=tk.LEFT, padx=(2, 4))
        resolution_menu = ttk.Combobox(
            resolution_frame, textvariable=self.render_resolution,
            values=RENDER_RESOLUTIONS, state="readonly", width=12)
        resolution_menu.pack(side=tk.LEFT)
        resolution_menu.bind("<<ComboboxSelected>>", self._set_render_resolution)

        tk.Label(panel, text="Left: pan  •  Right: orbit  •  Scroll: zoom\n"
                             "Drop an .hdr file on the render to change the HDRI",
                 wraplength=CONTROL_W - 12, justify=tk.CENTER,
                 font=("Segoe UI", 8), fg="#888").grid(row=8, column=0, pady=(4, 0))
        self._canvas = tk.Canvas(panel, width=CONTROL_W, height=CONTROL_H, bg="#1a1a2e",
                                 cursor="fleur", highlightthickness=1, highlightbackground="#444")
        self._canvas.grid(row=9, column=0, padx=6, pady=4)
        self._canvas.bind("<Button-1>",        self._pan_start)
        self._canvas.bind("<B1-Motion>",       self._pan_move)
        self._canvas.bind("<Button-3>",        self._drag_start)
        self._canvas.bind("<B3-Motion>",       self._drag_move)
        self._canvas.bind("<MouseWheel>",      self._scroll)
        self._canvas.bind("<Button-4>",        self._scroll)
        self._canvas.bind("<Button-5>",        self._scroll)
        self.bind("<r>", lambda _: self._reset())
        self.bind("<R>", lambda _: self._reset())

        btn_frame = tk.Frame(panel)
        btn_frame.grid(row=10, column=0, pady=2)
        for i, (lbl, az, el) in enumerate([("Front",0,12),("Side",90,10),("Rear",180,12),
                                            ("Low",30,4),("Hero",25,18),("Top",0,75)]):
            tk.Button(btn_frame, text=lbl, width=6,
                      command=lambda a=az, e=el: self._set_preset(a, e)
                      ).grid(row=i // 3, column=i % 3, padx=2, pady=1)

        tk.Button(panel, text="Save Film", bg="#2a6aba", fg="white",
                  font=("Segoe UI", 9, "bold"), width=22,
                  command=self._save_film
                  ).grid(row=11, column=0, pady=(3, 6))
        self._render_button = tk.Button(
            panel, text="Stop Rendering", bg="#9c2929", fg="white",
            font=("Segoe UI", 9, "bold"), width=22,
            command=self._stop_rendering)
        self._render_button.grid(row=12, column=0, pady=(0, 6))
        tk.Label(panel, text="Controls:", font=("Segoe UI", 10, "bold"),
                 anchor="w").grid(row=13, column=0, sticky="w", padx=8)
        tk.Label(panel,
                 text="Left-drag: pan\nRight-drag: rotate\nScroll forward: zoom in\nScroll back: zoom out",
                 justify=tk.LEFT, anchor="w", font=("Segoe UI", 8), fg="#666"
                 ).grid(row=14, column=0, sticky="w", padx=8, pady=(0, 6))

        self._draw_minimap()

    # ── Session ───────────────────────────────────────────────────────────────
    def _start_session(self):
        if self._render_stopped:
            return
        try:
            if not self._luxcore_initialized:
                pyluxcore.Init(_ignore_luxcore_log)
                self._luxcore_initialized = True
            # Add scene dir to resolver so relative paths in .scn/.cfg work
            pyluxcore.ClearFileNameResolverPaths()
            pyluxcore.AddFileNameResolverPath(".")
            pyluxcore.AddFileNameResolverPath(SCENE_DIR)
            pyluxcore.AddFileNameResolverPath(LUXCORE_ROOT)
            props = pyluxcore.Properties("ModoAlphaRomeo.cfg")
            # Use the CUDA device and the OptiX hardware accelerator. CPU film
            # processing is retained so the Tk controller can read both raw and
            # OIDN output without changing its display path.
            props.Set(pyluxcore.Property("renderengine.type", ["PATHOCL"]))
            props.Set(pyluxcore.Property("accelerator.type", ["OPTIX"]))
            props.Set(pyluxcore.Property("opencl.cpu.use", [False]))
            props.Set(pyluxcore.Property("opencl.gpu.use", [True]))
            props.Set(pyluxcore.Property("opencl.native.threads.count", [0]))
            props.Set(pyluxcore.Property("film.hw.enable", [False]))
            props.Set(pyluxcore.Property("film.width",  [self._render_width]))
            props.Set(pyluxcore.Property("film.height", [self._render_height]))
            props.Set(pyluxcore.Property("context.verbose", [False]))

            # Create Scene separately (GetScene() is non-copyable in pybind11)
            scene_file = props.Get("scene.file").GetString()
            self._scene  = pyluxcore.Scene(scene_file)
            if self._hdr_file:
                self._scene.Parse(pyluxcore.Properties().Set(
                    pyluxcore.Property("scene.infinitelight.file",
                                       [self._hdr_file])))
            self._config = pyluxcore.RenderConfig(props, self._scene)
            self._apply_camera_props()

            self._session = pyluxcore.RenderSession(self._config)
            self._session.Start()
            self._session_mode = "full"
            self._render_backend = "PATHOCL / OptiX"

            self.pipeline = 0
            self._schedule_switch()
            self.after(REFRESH_MS, self._update_film)
            self._info.config(text="Rendering...")
        except Exception as ex:
            self._info.config(text=f"Error: {ex}")
            self._render_win.title(f"{WINDOW_TITLE} — Startup failed: {ex}")

    def _stop_rendering(self):
        """Stop refinement and preserve the most recently displayed film."""
        if self._render_stopped:
            return
        self._render_stopped = True
        for attr in ("_preview_restart_id", "_full_restart_id"):
            timer_id = getattr(self, attr)
            if timer_id:
                self.after_cancel(timer_id)
                setattr(self, attr, None)
        if self._switch_id:
            self.after_cancel(self._switch_id)
            self._switch_id = None
        self._camera_restart_pending = False
        with self._restart_lock:
            if self._session and self._session.IsStarted():
                self._session.Stop()
        self._render_button.config(
            text="Start Rendering", bg="#2a6a3a",
            command=self._restart_rendering)
        self._save_settings()
        self._render_win.title(f"{WINDOW_TITLE} — Rendering stopped")
    def _restart_rendering(self):
        """Start a new session after stopping without recreating the UI."""
        if not self._render_stopped:
            return
        self._render_stopped = False
        self._camera_restart_pending = False
        self._preview_restart_in_progress = False
        self._camera_snapshot = None
        while True:
            try:
                self._restart_results.get_nowait()
            except queue.Empty:
                break
        self._render_button.config(
            text="Stop Rendering", bg="#9c2929",
            command=self._stop_rendering)
        self._save_settings()
        self._render_win.title(f"{WINDOW_TITLE} — Restarting...")
        self.after(1, self._start_session)

    def _do_restart_session(self, width, height, mode, camera_snapshot,
                            hdr_file=None):
        """Restart a session at the requested resolution in a background thread."""
        succeeded = False
        error_message = None
        try:
            with self._restart_lock:
                try:
                    if self._render_stopped:
                        return
                    if self._session and self._session.IsStarted():
                        self._session.Stop()
                    _, _, _, _, exp, _ = camera_snapshot
                    prescale = 1.0 / max(exp, 0.001)
                    self._config.Parse(pyluxcore.Properties()
                        .Set(pyluxcore.Property("film.imagepipelines.0.0.prescale", [prescale]))
                        .Set(pyluxcore.Property("film.imagepipelines.1.1.prescale", [prescale]))
                        .Set(pyluxcore.Property("film.width", [width]))
                        .Set(pyluxcore.Property("film.height", [height])))
                    if hdr_file:
                        self._scene.Parse(pyluxcore.Properties().Set(
                            pyluxcore.Property("scene.infinitelight.file",
                                               [hdr_file])))
                    self._apply_camera_snapshot(camera_snapshot)
                    self._session = pyluxcore.RenderSession(self._config)
                    self._session.Start()
                    self._session_mode = mode
                    succeeded = True
                except Exception as ex:
                    error_message = str(ex)
        finally:
            self._restart_results.put(
                (mode, camera_snapshot[-1], succeeded, error_message, hdr_file))

    def _process_restart_results(self):
        try:
            while True:
                self._finish_restart(*self._restart_results.get_nowait())
        except queue.Empty:
            pass
        self.after(25, self._process_restart_results)

    def _finish_restart(self, mode, started_revision, succeeded, error_message,
                        hdr_file=None):
        if mode == "preview":
            self._preview_restart_in_progress = False
        if self._render_stopped:
            return
        if not succeeded:
            self._camera_restart_pending = False
            self._info.config(text=f"Restart failed: {error_message or 'unknown error'}")
            return
        if hdr_file:
            self._hdr_file = hdr_file
            self._save_settings()
        # A preview session is a clean, complete image even when input has
        # advanced since it began. Show it for responsive feedback, then queue
        # the next snapshot. Full-resolution output remains exact-only.
        if started_revision != self._camera_revision:
            self._camera_restart_pending = mode != "preview"
            self._queue_camera_restarts()
            return
        self._camera_restart_pending = False
        self._reset_switch()

    def _queue_camera_restarts(self):
        if self._render_stopped:
            return
        # Restart previews at a fixed cadence rather than accumulating samples
        # from several camera positions in one film.
        if (not self._preview_restart_id
                and not self._preview_restart_in_progress):
            self._preview_restart_id = self.after(
                PREVIEW_RESTART_MS,
                self._restart_preview_session)
        if self._full_restart_id:
            self.after_cancel(self._full_restart_id)
        self._full_restart_id = self.after(
            FULL_RESTART_MS,
            self._restart_full_session)

    def _restart_preview_session(self):
        self._preview_restart_id = None
        if self._render_stopped:
            return
        self._preview_restart_in_progress = True
        camera_snapshot = self._camera_snapshot
        threading.Thread(
            target=self._do_restart_session,
            args=(PREVIEW_W, PREVIEW_H, "preview", camera_snapshot),
            daemon=True).start()

    def _restart_full_session(self):
        self._full_restart_id = None
        if self._render_stopped:
            return
        camera_snapshot = self._camera_snapshot
        threading.Thread(
            target=self._do_restart_session,
            args=(self._render_width, self._render_height, "full", camera_snapshot),
            daemon=True).start()

    # ── Camera ───────────────────────────────────────────────────────────────
    def _cam_orig(self):
        return orbit(self._target, self.dist.get(), self.az.get(), self.el.get())

    def _capture_camera_snapshot(self):
        return (tuple(self._target), self.dist.get(), self.az.get(), self.el.get(),
                max(0.001, self.exposure.get()), self._camera_revision)

    def _set_render_resolution(self, _=None):
        width, height = (int(value) for value in self.render_resolution.get().split(" x "))
        if (width, height) == (self._render_width, self._render_height):
            return
        self._render_width, self._render_height = width, height
        self._save_settings()
        if self._render_stopped or not self._scene or not self._session:
            return
        self._camera_revision += 1
        self._camera_snapshot = self._capture_camera_snapshot()
        self._camera_restart_pending = True
        threading.Thread(
            target=self._do_restart_session,
            args=(width, height, "full", self._camera_snapshot),
            daemon=True).start()
    def _zoom_at_cursor(self, e, viewport_w, viewport_h):
        delta = -e.delta / 120 if hasattr(e, "delta") and e.delta else (
            1 if e.num == 5 else -1)
        old_dist = self.dist.get()
        new_dist = max(1.0, old_dist + delta * 1.0)
        if new_dist == old_dist:
            return

        orig = self._cam_orig()
        right, up, fwd = cam_axes(orig, self._target)
        screen_x = 2.0 * ((e.x + 0.5) / viewport_w) - 1.0
        screen_y = 1.0 - 2.0 * ((e.y + 0.5) / viewport_h)
        tan_half_fov = math.tan(math.radians(CAMERA_FOV_DEG * 0.5))
        ray_dir = _norm([
            fwd[i] + screen_x * tan_half_fov * (viewport_w / viewport_h) * right[i]
                   + screen_y * tan_half_fov * up[i]
            for i in range(3)
        ])
        ray_forward_dot = _dot(ray_dir, fwd)
        if ray_forward_dot > 1e-6:
            # Keep the focal-plane point that was under the cursor fixed.
            focal_point = [
                orig[i] + ray_dir[i] * old_dist / ray_forward_dot
                for i in range(3)
            ]
            orbit_dir = _norm([orig[i] - self._target[i] for i in range(3)])
            self._target = [
                focal_point[i] - new_dist * orbit_dir[i]
                - ray_dir[i] * new_dist / ray_forward_dot
                for i in range(3)
            ]

        self.dist.set(new_dist)
        self._on_camera()

    def _apply_camera_props(self):
        orig = self._cam_orig()
        self._scene.Parse(pyluxcore.Properties()
            .Set(pyluxcore.Property("scene.camera.lookat.orig",   list(orig)))
            .Set(pyluxcore.Property("scene.camera.lookat.target", list(self._target)))
            .Set(pyluxcore.Property("scene.camera.up",            [0.0, 0.0, 1.0])))

    def _apply_camera_snapshot(self, camera_snapshot):
        target, dist, az, el, _, _ = camera_snapshot
        orig = orbit(target, dist, az, el)
        self._scene.Parse(pyluxcore.Properties()
            .Set(pyluxcore.Property("scene.camera.lookat.orig",   list(orig)))
            .Set(pyluxcore.Property("scene.camera.lookat.target", list(target)))
            .Set(pyluxcore.Property("scene.camera.up",            [0.0, 0.0, 1.0])))

    def _clear_display(self):
        """Immediately paint the render canvas black to prevent ghosting."""
        black = bytes(FILM_W * FILM_H * 4)  # fast zero-fill
        img = Image.frombuffer("RGBA", (FILM_W, FILM_H), black, "raw", "RGBA", 0, 1)
        self._tk_image = ImageTk.PhotoImage(img)
        self._render_canvas.itemconfigure(self._canvas_img_id, image=self._tk_image)

    def _on_camera(self, refresh_ui=True):
        if refresh_ui:
            self._update_info()
        self._save_settings()
        if self._scene and self._session and not self._render_stopped:
            self._camera_revision += 1
            self._camera_snapshot = self._capture_camera_snapshot()
            # Keep the active film untouched until a replacement session is
            # ready; it cannot then mix samples from two camera positions.
            self._camera_restart_pending = True
            self._queue_camera_restarts()

    def _on_exposure(self):
        self._update_info()
        self._save_settings()
        if self._scene and self._session and not self._render_stopped:
            self._camera_revision += 1
            self._camera_snapshot = self._capture_camera_snapshot()
            self._camera_restart_pending = True
            threading.Thread(
                target=self._do_restart_session,
                args=(self._render_width, self._render_height, "full", self._camera_snapshot),
                daemon=True).start()

    # ── Film display ──────────────────────────────────────────────────────────
    def _fit_image_to_viewport(self, img):
        scale = min(FILM_W / img.width, FILM_H / img.height)
        width = max(1, round(img.width * scale))
        height = max(1, round(img.height * scale))
        if (width, height) == img.size:
            return img
        resample = Image.Resampling.LANCZOS if scale < 1.0 else Image.Resampling.BILINEAR
        return img.resize((width, height), resample)

    def _capture_film_frame(self, fit_for_viewport=True):
        """Copy a stable renderer film while blocking concurrent replacement."""
        with self._restart_lock:
            if (not self._session or not self._session.IsStarted()
                    or self._camera_restart_pending):
                return None

            self._session.UpdateStats()
            film = self._session.GetFilm()
            is_preview = self._session_mode == "preview"
            stats = self._session.GetStats()
            if (is_preview
                    and stats.Get("stats.renderengine.pass").GetInt()
                        < PREVIEW_MIN_PASSES):
                return "warming"

            width = film.GetWidth()
            height = film.GetHeight()
            if is_preview:
                if len(self._preview_film_buf) != width * height * 3:
                    self._preview_film_buf = array('f', [0.0] * (width * height * 3))
                    self._preview_rgba_buf = array('b', [0] * (width * height * 4))
                film_buf = self._preview_film_buf
                rgba_buf = self._preview_rgba_buf
            else:
                if len(self._film_buf) != width * height * 3:
                    self._film_buf = array('f', [0.0] * (width * height * 3))
                    self._rgba_buf = array('b', [0] * (width * height * 4))
                film_buf = self._film_buf
                rgba_buf = self._rgba_buf

            # OIDN is useful after the full-resolution render settles, but it
            # is unstable on a freshly restarted, low-sample preview.
            display_pipeline = 0 if is_preview else self.pipeline
            film.GetOutputFloat(pyluxcore.FilmOutputType.RGB_IMAGEPIPELINE,
                                film_buf, display_pipeline)
            pyluxcore.ConvertFilmChannelOutput_3xFloat_To_4xUChar(
                width, height, film_buf, rgba_buf, False)
            native_img = Image.frombuffer("RGBA", (width, height),
                                          bytes(rgba_buf[:width * height * 4]),
                                          "raw", "RGBA", 0, 1)
            self._last_film_image = native_img.copy()
            img = self._fit_image_to_viewport(native_img) if fit_for_viewport else native_img
            return img, is_preview, stats, display_pipeline

    def _update_film(self):
        try:
            frame = self._capture_film_frame()
            if frame == "warming":
                self._render_win.title(f"{WINDOW_TITLE} — Preview | warming up")
                self.after(PREVIEW_REFRESH_MS, self._update_film)
                return
            if frame:
                img, is_preview, stats, display_pipeline = frame
                self._tk_image = ImageTk.PhotoImage(img)
                self._render_canvas.itemconfigure(self._canvas_img_id, image=self._tk_image)

                t   = stats.Get("stats.renderengine.time").GetFloat()
                sps = stats.Get("stats.renderengine.total.samplesec").GetFloat() / 1e6
                pip = "OIDN" if display_pipeline == 1 else "Raw"
                mode = "Preview | " if is_preview else ""
                self._render_win.title(
                    f"{WINDOW_TITLE} — {mode}{t:.0f}s | {sps:.2f}Msps | "
                    f"{pip} | {self._render_backend}")
        except Exception:
            pass
        refresh_ms = PREVIEW_REFRESH_MS if self._session_mode == "preview" else REFRESH_MS
        self.after(refresh_ms, self._update_film)

    # ── Pipeline ──────────────────────────────────────────────────────────────
    def _set_pipeline(self, idx):
        self.pipeline = idx
        self._update_info()
        if self._switch_id:
            self.after_cancel(self._switch_id)
            self._switch_id = None

    def _schedule_switch(self):
        if self._switch_id:
            self.after_cancel(self._switch_id)
        self._switch_id = self.after(self.switch_sec.get() * 1000, self._auto_switch)

    def _reset_switch(self):
        self.pipeline = 0
        self._schedule_switch()
        self._update_info()

    def _auto_switch(self):
        self._switch_id = None
        self.pipeline   = 1
        self._update_info()

    # ── Save ──────────────────────────────────────────────────────────────────
    def _save_film(self):
        path = filedialog.asksaveasfilename(
            parent=self,
            title="Save Rendered Image",
            defaultextension=".png",
            filetypes=[
                ("PNG image", "*.png"),
                ("JPEG image", "*.jpg *.jpeg"),
            ],
        )
        if not path:
            return
        try:
            if self._render_stopped:
                img = self._last_film_image
                if img is None:
                    raise RuntimeError("No rendered film was captured before stopping")
            else:
                frame = self._capture_film_frame(fit_for_viewport=False)
                if frame == "warming":
                    raise RuntimeError("Preview is still warming up")
                if not frame:
                    raise RuntimeError("No stable rendered film is available")
                img, _, _, _ = frame
            if os.path.splitext(path)[1].lower() in (".jpg", ".jpeg"):
                img.convert("RGB").save(path, "JPEG", quality=95)
            else:
                img.save(path, "PNG")
            self._render_win.title(f"{WINDOW_TITLE} — Saved: {os.path.basename(path)}")
        except Exception as ex:
            self._render_win.title(f"{WINDOW_TITLE} — Save failed: {ex}")

    # ── Minimap ───────────────────────────────────────────────────────────────
    def _draw_minimap(self):
        self._canvas.delete("all")
        w, h   = CONTROL_W, CONTROL_H
        cx, cy = w // 2, h // 2 - 6
        az, el = self.az.get(), self.el.get()
        r = min(w, h) * 0.23

        self._canvas.create_oval(cx-r, cy-r, cx+r, cy+r, outline="#334")
        self._canvas.create_line(cx-r*1.25, cy, cx+r*1.25, cy, fill="#334")
        self._canvas.create_line(cx, cy-r*1.25, cx, cy+r*1.25, fill="#334")

        dx =  r * math.sin(math.radians(az)) * math.cos(math.radians(el))
        dy = -r * math.cos(math.radians(az)) * math.cos(math.radians(el)) * 0.5 \
             - r * math.sin(math.radians(el)) * 0.5
        self._canvas.create_oval(cx+dx-5, cy+dy-5, cx+dx+5, cy+dy+5,
                                 fill="#4a9eff", outline="#88ccff", width=2)
        self._canvas.create_line(cx, cy, cx+dx, cy+dy, fill="#4a9eff")
        for lbl, pos in [("FRONT", (cx, cy-r*1.65)), ("REAR", (cx, cy+r*1.65)),
                          ("L", (cx-r*1.7, cy)), ("R", (cx+r*1.7, cy))]:
            self._canvas.create_text(*pos, text=lbl, fill="#556", font=("Consolas",7))

        orig = self._cam_orig()
        cr, cu, _ = cam_axes(orig, self._target)
        gx, gy = 26, h-16
        axes = [([1,0,0],"#ff4444","X"),([0,1,0],"#44ff44","Y"),([0,0,1],"#4488ff","Z")]
        axes.sort(key=lambda a: -(proj_axis(a[0],cr,cu,20)[0]**2+proj_axis(a[0],cr,cu,20)[1]**2))
        for ax, col, lbl in axes:
            sx, sy = proj_axis(ax, cr, cu, 20)
            self._canvas.create_line(gx, gy, gx+sx, gy+sy, fill=col, width=2, arrow=tk.LAST)
            self._canvas.create_text(gx+sx*1.35, gy+sy*1.35, text=lbl,
                                     fill=col, font=("Consolas",8,"bold"))

        pip_col = "#44ff44" if self.pipeline == 1 else "#ffaa44"
        self._canvas.create_text(w-24, h-8,
                                 text="OIDN" if self.pipeline==1 else "RAW",
                                 fill=pip_col, font=("Consolas",8,"bold"))

    def _update_info(self):
        az, el = self.az.get(), self.el.get()
        self._info.config(text=f"  az={az:6.1f}°  el={el:5.1f}°"
                               f"  dist={self.dist.get():5.2f}"
                               f"  exp={self.exposure.get():.3f}"
                               f"  pipe={self.pipeline}")
        self._draw_minimap()

    # ── Input ─────────────────────────────────────────────────────────────────
    def _drag_start(self, e):
        self._drag_x, self._drag_y = e.x, e.y

    def _drag_move(self, e):
        self.az.set(self.az.get() + (e.x - self._drag_x) * 0.5)
        self.el.set(max(-89, min(89, self.el.get() + (e.y - self._drag_y) * 0.3)))
        self._drag_x, self._drag_y = e.x, e.y
        self._update_info()
        self._on_camera(refresh_ui=False)

    def _scroll(self, e):
        self._zoom_at_cursor(e, self._canvas.winfo_width(), self._canvas.winfo_height())

    # ── Render canvas input ───────────────────────────────────────────────────
    def _pan_start(self, e):
        self._pan_drag_x, self._pan_drag_y = e.x, e.y

    def _pan_move(self, e):
        dx = e.x - self._pan_drag_x
        dy = e.y - self._pan_drag_y
        self._pan_drag_x, self._pan_drag_y = e.x, e.y

        # Scale pan speed by distance so it feels consistent at any zoom.
        # The full-size render viewport needs a lower per-pixel sensitivity
        # than the compact controller canvas.
        scale = self.dist.get() / 600.0
        if e.widget is self._render_canvas:
            scale *= 0.5
        orig   = self._cam_orig()
        cr, cu, _ = cam_axes(orig, self._target)

        # Move the camera opposite to the pointer so the rendered content follows it.
        offset = [(-dx * scale * cr[i]) + (dy * scale * cu[i]) for i in range(3)]

        self._target = [self._target[i] + offset[i] for i in range(3)]
        self._on_camera()

    def _render_scroll(self, e):
        """Zoom while retaining the focal-plane point under the cursor."""
        self._zoom_at_cursor(e, FILM_W, FILM_H)

    def _set_preset(self, az, el):
        self.az.set(az); self.el.set(el)
        self._on_camera()

    def _reset(self):
        self._set_preset(DEFAULT_AZ, DEFAULT_EL)


if __name__ == "__main__":
    app = CameraController()
    app.mainloop()
