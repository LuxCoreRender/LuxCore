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

import os, sys, math, re, queue, threading, json, socket, struct
from array import array
try:
    from tkinterdnd2 import COPY, DND_FILES, TkinterDnD
except ImportError:
    COPY = DND_FILES = TkinterDnD = None

# ── Bootstrap pyluxcore ────────────────────────────────────────────────────────
LUXCORE_ROOT   = r"C:\Users\gcroc\Projects\LuxCore"
PYLUXCORE_PATH = os.path.join(LUXCORE_ROOT, r"out\build\src\pyluxcore\Release")
LUXCORE_BIN    = os.path.join(LUXCORE_ROOT, r"out\install\Release\bin")
SCENE_FILE     = os.path.join(LUXCORE_ROOT, r"scenes\AlphaRomeo\ModoAlphaRomeo.scn")
CFG_FILE       = os.path.join(LUXCORE_ROOT, r"scenes\AlphaRomeo\ModoAlphaRomeo.cfg")
CUDA_12_4_NVRTC = (
    r"C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v12.4\bin\nvrtc64_120_0.dll")

SCENE_DIR = os.path.join(LUXCORE_ROOT, r"scenes\AlphaRomeo")
SETTINGS_FILE = os.path.join(SCENE_DIR, "camera_controller_settings.json")

os.add_dll_directory(LUXCORE_BIN)
_nvrtc_library = os.environ.get("LUXRAYS_NVRTC_LIBRARY", CUDA_12_4_NVRTC)
if os.path.isfile(_nvrtc_library):
    # CUDA 13.3 emits PTX 9.3, which the installed OptiX driver cannot load.
    # CUEW reads this absolute-path override before trying its normal DLL list.
    os.environ["LUXRAYS_NVRTC_LIBRARY"] = _nvrtc_library
    _nvrtc_dll_directory = os.add_dll_directory(os.path.dirname(_nvrtc_library))
sys.path.insert(0, PYLUXCORE_PATH)
os.chdir(SCENE_DIR)  # .scn and .ply paths are relative to the scene directory

import pyluxcore
from PIL import Image, ImageTk
import tkinter as tk
from tkinter import filedialog, ttk

# ── Defaults ──────────────────────────────────────────────────────────────────
DEFAULT_HDRI_FILE   = "hdre_055.hdr"  # fallback if settings has no valid hdr_file
# Downsample large HDR/EXR lighting maps in memory to a 2K-wide proxy.
# The camera-only background retains the original source resolution.
HDRI_DOWNSAMPLE_SCALE = 0.125
HDRI_DOWNSAMPLE_MIN_SIZE = 64
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

# ── External control server ────────────────────────────────────────────────
# camera_controller_settings.json "control_port" or the LUXCORE_CONTROL_PORT
# environment variable selects the TCP port; 0 disables the server.
CONTROL_HOST         = "127.0.0.1"
DEFAULT_CONTROL_PORT = 8765
CONTROL_MAX_HEADER   = 1 << 20    # 1 MB JSON header limit
CONTROL_MAX_PAYLOAD  = 512 << 20  # 512 MB binary payload limit per message
UPLOAD_APPLY_MS      = 500        # auto-apply delay after streamed uploads

# ── Math helpers ──────────────────────────────────────────────────────
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

def read_hdri_gain(path):
    """Read the first channel of scene.lights.*.gain or scene.infinitelight.gain from a .scn file."""
    with open(path) as f:
        for line in f:
            m = re.match(
                r"scene\.(?:lights\.[^.]+|infinitelight)\.gain\s*=\s*([\d.eE+\-]+)",
                line.strip())
            if m:
                return float(m.group(1))
    return 0.01

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
def _setting_bool(settings, name, default):
    value = settings.get(name, default)
    return value if isinstance(value, bool) else default

def _setting_target(settings):
    target = settings.get("target")
    if isinstance(target, (list, tuple)) and len(target) == 3:
        try:
            return [float(value) for value in target]
        except (TypeError, ValueError):
            pass
    return list(DEFAULT_TARGET)
def _is_env_map(path):
    """Return True for file extensions LuxCore accepts as environment maps."""
    return path.lower().endswith((".hdr", ".exr"))

def _setting_hdr_file(settings):
    hdr_file = settings.get("hdr_file")
    if (isinstance(hdr_file, str) and _is_env_map(hdr_file)
            and os.path.isfile(hdr_file)):
        return os.path.normpath(hdr_file)
    return None

def _ignore_luxcore_log(_message):
    pass
def _environment_storage(path):
    """Use half storage for large maps without changing their pixel dimensions."""
    file_mb = os.path.getsize(path) / (1024 * 1024) if os.path.isfile(path) else 0
    return "half" if file_mb > 50 else "float"

def _set_environment_visibility(props, prefix, camera_visible, direct_visible,
                                indirect_visible):
    props.Set(pyluxcore.Property(
        f"{prefix}.visibility.camera.enable", [camera_visible]))
    props.Set(pyluxcore.Property(
        f"{prefix}.visibility.direct.enable", [direct_visible]))
    props.Set(pyluxcore.Property(
        f"{prefix}.visibility.indirect.diffuse.enable", [indirect_visible]))
    props.Set(pyluxcore.Property(
        f"{prefix}.visibility.indirect.glossy.enable", [indirect_visible]))
    props.Set(pyluxcore.Property(
        f"{prefix}.visibility.indirect.specular.enable", [indirect_visible]))

def _apply_environment_maps(scene, source_file, gain, render_hdri_background=True):
    """Use a reduced lighting map plus either an HDRI or white camera background."""
    storage = _environment_storage(source_file)
    props = pyluxcore.Properties()
    lighting_prefix = "scene.lights.hdri"
    background_prefix = "scene.lights.hdri_background"
    props.Set(pyluxcore.Property(f"{lighting_prefix}.type", ["infinite"]))
    props.Set(pyluxcore.Property(f"{lighting_prefix}.file", [source_file]))
    props.Set(pyluxcore.Property(f"{lighting_prefix}.colorspace", ["nop"]))
    props.Set(pyluxcore.Property(f"{lighting_prefix}.storage", [storage]))
    props.Set(pyluxcore.Property(f"{lighting_prefix}.gain", [gain, gain, gain]))
    props.Set(pyluxcore.Property(f"{lighting_prefix}.resizepolicy.enable", [True]))
    _set_environment_visibility(props, lighting_prefix, False, True, True)

    if render_hdri_background:
        props.Set(pyluxcore.Property(f"{background_prefix}.type", ["infinite"]))
        props.Set(pyluxcore.Property(f"{background_prefix}.file", [source_file]))
        props.Set(pyluxcore.Property(f"{background_prefix}.colorspace", ["nop"]))
        props.Set(pyluxcore.Property(f"{background_prefix}.storage", [storage]))
        props.Set(pyluxcore.Property(f"{background_prefix}.gain", [gain, gain, gain]))
        props.Set(pyluxcore.Property(
            f"{background_prefix}.resizepolicy.enable", [False]))
    else:
        props.Set(pyluxcore.Property(
            f"{background_prefix}.type", ["constantinfinite"]))
        props.Set(pyluxcore.Property(f"{background_prefix}.color", [1.0, 1.0, 1.0]))

    _set_environment_visibility(props, background_prefix, True, False, False)

    scene.Parse(props)
    scene.RemoveUnusedImageMaps()

# ── External control protocol ───────────────────────────────────────────────
def _recv_exact(conn, count):
    """Read exactly count bytes from a socket or raise ConnectionError."""
    data = bytearray()
    while len(data) < count:
        chunk = conn.recv(min(65536, count - len(data)))
        if not chunk:
            raise ConnectionError("Control connection closed")
        data.extend(chunk)
    return bytes(data)

# The C# MeshHeader writes its buffers in this fixed order.
_UPLOAD_SECTIONS = (("Vertices", "vertices"), ("Normals", "normals"),
                    ("UVs", "uvs"), ("Indices", "indices"))

def _header_command(header):
    """Return the command name from cmd/Command/command header keys."""
    for key in ("cmd", "Command", "command"):
        value = header.get(key)
        if isinstance(value, str) and value:
            return value.lower()
    return None

def _control_buffer_specs(header):
    """Map a header to its ordered binary buffer layout."""
    buffers = header.get("buffers")
    if isinstance(buffers, list):
        specs = []
        for spec in buffers:
            if not isinstance(spec, dict):
                raise ValueError("Each buffers entry must be a JSON object")
            role = spec.get("role")
            if not isinstance(role, str) or not role:
                raise ValueError("Each buffer needs a role name")
            specs.append((role, int(spec.get("bytes", -1))))
        return specs
    # upload_mesh style: named sections carrying ByteLength, fixed order.
    specs = []
    for key, role in _UPLOAD_SECTIONS:
        section = header.get(key)
        if isinstance(section, dict):
            specs.append((role, int(section.get("ByteLength", 0))))
    return specs

def _read_control_message(conn):
    """Read one framed message: header size, JSON header, then binary blobs."""
    (header_size,) = struct.unpack("<I", _recv_exact(conn, 4))
    if not 0 < header_size <= CONTROL_MAX_HEADER:
        raise ValueError(f"Control header size out of range: {header_size}")
    header = json.loads(_recv_exact(conn, header_size).decode("utf-8"))
    if not isinstance(header, dict):
        raise ValueError("Control header must be a JSON object")
    blobs = {}
    total = 0
    for role, size in _control_buffer_specs(header):
        if role in blobs:
            raise ValueError("Each buffer needs a unique role name")
        if size < 0:
            raise ValueError(f"The {role} buffer size is invalid")
        total += size
        if total > CONTROL_MAX_PAYLOAD:
            raise ValueError("Control payload size out of range")
        blobs[role] = _recv_exact(conn, size)
    return header, blobs

def _send_control_message(conn, reply):
    """Send one framed JSON reply."""
    payload = json.dumps(reply).encode("utf-8")
    conn.sendall(struct.pack("<I", len(payload)) + payload)

# ── Controller ────────────────────────────────────────────────────────────────
class CameraController(TkinterDnD.Tk if TkinterDnD else tk.Tk):
    def __init__(self):
        super().__init__()
        self.title(WINDOW_TITLE)
        self.resizable(False, False)
        self._window_icon = tk.PhotoImage(file=WINDOW_ICON)
        self.iconphoto(True, self._window_icon)
        self._settings = _read_controller_settings()
        self._settings_ready = False
        self._hdr_file = _setting_hdr_file(self._settings)
        self._control_port_setting = int(_setting_float(
            self._settings, "control_port", DEFAULT_CONTROL_PORT, 0, 65535))
        self._control_port = self._control_port_setting
        env_port = os.environ.get("LUXCORE_CONTROL_PORT")
        if env_port is not None:
            try:
                env_value = int(env_port)
                if 0 <= env_value <= 65535:
                    self._control_port = env_value
            except ValueError:
                pass
        self._control_socket = None
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
        saved_hdri_gain = _setting_float(
            self._settings, "hdri_gain", read_hdri_gain(SCENE_FILE), 0.0001, 100.0)
        self._hdri_gain_log = tk.DoubleVar(
            value=math.log10(max(0.0001, min(100.0, saved_hdri_gain))))
        self._render_hdri_background = tk.BooleanVar(value=_setting_bool(
            self._settings, "render_hdri_background", True))
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
        self._control_commands = queue.Queue()
        self._pending_meshes = {}
        self._pending_scene_props = []
        self._pending_config_props = []
        self._upload_apply_id = None
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
        self._render_hdri_background.trace_add(
            "write", self._on_render_hdri_background_changed)
        self.protocol("WM_DELETE_WINDOW", self._on_close)
        self._update_info()
        self.after(25, self._process_restart_results)
        self.after(25, self._process_control_commands)
        self._enable_hdr_file_drop()
        self._start_control_server()
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
            "hdri_gain": 10.0 ** self._hdri_gain_log.get(),
            "render_hdri_background": self._render_hdri_background.get(),
            "auto_oidn_seconds": self.switch_sec.get(),
            "render_resolution": self.render_resolution.get(),
            "control_port": self._control_port_setting,
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
        if self._control_socket:
            try:
                self._control_socket.close()
            except OSError:
                pass
            self._control_socket = None
        if not self._render_stopped:
            self._stop_rendering()
        self.destroy()
    # ── HDR file drop ────────────────────────────────────────────────────────
    def _enable_hdr_file_drop(self):
        """Accept .hdr and .exr files through tkinterdnd2's native OLE integration."""
        if not DND_FILES:
            self._show_hdr_drop_error(
                "HDR file drop requires: python -m pip install tkinterdnd2")
            return
        self._render_canvas.drop_target_register(DND_FILES)
        self._render_canvas.dnd_bind("<<Drop>>", self._on_hdr_file_drop_event)

    def _on_hdr_file_drop_event(self, event):
        try:
            dropped_files = self.tk.splitlist(event.data)
        except tk.TclError:
            dropped_files = ()
        for candidate in dropped_files:
            if _is_env_map(candidate):
                self._on_hdr_file_drop(candidate)
                return COPY
        self._show_hdr_drop_error(
            "Drop a .hdr or .exr environment map on the render viewport")
        return COPY

    def _show_hdr_drop_error(self, message):
        self._info.config(text=message)
        self._render_win.title(f"{WINDOW_TITLE} — {message}")

    def _on_hdr_file_drop(self, hdr_file):
        hdr_file = os.path.normpath(hdr_file)
        if not _is_env_map(hdr_file) or not os.path.isfile(hdr_file):
            self._show_hdr_drop_error("Dropped environment map file is unavailable")
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
        file_mb = os.path.getsize(hdr_file) / (1024 * 1024)
        size_note = f"  ({file_mb:.0f} MB — may take a moment)" if file_mb > 50 else ""
        self._render_win.title(
            f"{WINDOW_TITLE} — Loading HDRI: {os.path.basename(hdr_file)}{size_note}")
        gain = 10.0 ** self._hdri_gain_log.get()
        threading.Thread(
            target=self._do_restart_session,
            args=(self._render_width, self._render_height, "full",
                  self._camera_snapshot, hdr_file, gain,
                  self._render_hdri_background.get()),
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

        tk.Label(panel, text="HDRI Gain").grid(row=5, column=0, sticky="w", **pad)
        tk.Scale(panel, from_=-2.0, to=3.0, resolution=0.01, orient=tk.HORIZONTAL,
                 variable=self._hdri_gain_log, length=scale_len, showvalue=False,
                 command=lambda _: self._on_hdri_gain()).grid(row=6, column=0, **pad)
        tk.Checkbutton(panel, text="Render HDRI Background",
                       variable=self._render_hdri_background
                       ).grid(row=7, column=0, sticky="w", **pad)

        pip_frame = tk.Frame(panel)
        pip_frame.grid(row=8, column=0, pady=(2, 0))
        tk.Button(pip_frame, text="Raw", width=8,
                  command=lambda: self._set_pipeline(0)).pack(side=tk.LEFT, padx=2)
        tk.Button(pip_frame, text="OIDN", width=8,
                  command=lambda: self._set_pipeline(1)).pack(side=tk.LEFT, padx=2)

        delay_frame = tk.Frame(panel)
        delay_frame.grid(row=9, column=0, pady=(2, 0))
        tk.Label(delay_frame, text="Auto OIDN").pack(side=tk.LEFT, padx=(2, 3))
        tk.Spinbox(delay_frame, from_=1, to=120, textvariable=self.switch_sec,
                   width=3, font=("Segoe UI", 8)).pack(side=tk.LEFT)
        tk.Label(delay_frame, text="sec").pack(side=tk.LEFT, padx=(3, 2))
        resolution_frame = tk.Frame(panel)
        resolution_frame.grid(row=10, column=0, pady=(3, 0))
        tk.Label(resolution_frame, text="Resolution").pack(side=tk.LEFT, padx=(2, 4))
        resolution_menu = ttk.Combobox(
            resolution_frame, textvariable=self.render_resolution,
            values=RENDER_RESOLUTIONS, state="readonly", width=12)
        resolution_menu.pack(side=tk.LEFT)
        resolution_menu.bind("<<ComboboxSelected>>", self._set_render_resolution)

        tk.Label(panel, text="Left: pan  •  Right: orbit  •  Scroll: zoom\n"
                             "Drop a .hdr or .exr file on the render to change HDRI",
                 wraplength=CONTROL_W - 12, justify=tk.CENTER,
                 font=("Segoe UI", 8), fg="#888").grid(row=11, column=0, pady=(4, 0))
        self._canvas = tk.Canvas(panel, width=CONTROL_W, height=CONTROL_H, bg="#1a1a2e",
                                 cursor="fleur", highlightthickness=1, highlightbackground="#444")
        self._canvas.grid(row=12, column=0, padx=6, pady=4)
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
        btn_frame.grid(row=13, column=0, pady=2)
        for i, (lbl, az, el) in enumerate([("Front",0,12),("Side",90,10),("Rear",180,12),
                                            ("Low",30,4),("Hero",25,18),("Top",0,75)]):
            tk.Button(btn_frame, text=lbl, width=6,
                      command=lambda a=az, e=el: self._set_preset(a, e)
                      ).grid(row=i // 3, column=i % 3, padx=2, pady=1)

        tk.Button(panel, text="Save Film", bg="#2a6aba", fg="white",
                  font=("Segoe UI", 9, "bold"), width=22,
                  command=self._save_film
                  ).grid(row=14, column=0, pady=(3, 6))
        self._render_button = tk.Button(
            panel, text="Stop Rendering", bg="#9c2929", fg="white",
            font=("Segoe UI", 9, "bold"), width=22,
            command=self._stop_rendering)
        self._render_button.grid(row=15, column=0, pady=(0, 6))
        tk.Label(panel, text="Controls:", font=("Segoe UI", 10, "bold"),
                 anchor="w").grid(row=16, column=0, sticky="w", padx=8)
        tk.Label(panel,
                 text="Left-drag: pan\nRight-drag: rotate\nScroll forward: zoom in\nScroll back: zoom out",
                 justify=tk.LEFT, anchor="w", font=("Segoe UI", 8), fg="#666"
                 ).grid(row=17, column=0, sticky="w", padx=8, pady=(0, 6))

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
            props.Set(pyluxcore.Property("scene.images.resizepolicy.type", ["FIXED"]))
            props.Set(pyluxcore.Property("scene.images.resizepolicy.scale",
                                         [HDRI_DOWNSAMPLE_SCALE]))
            props.Set(pyluxcore.Property("scene.images.resizepolicy.minsize",
                                         [HDRI_DOWNSAMPLE_MIN_SIZE]))
            props.Set(pyluxcore.Property("film.hw.enable", [False]))
            props.Set(pyluxcore.Property("film.width",  [self._render_width]))
            props.Set(pyluxcore.Property("film.height", [self._render_height]))
            props.Set(pyluxcore.Property("context.verbose", [False]))

            # Supply the resize policy to Scene itself: image maps load during
            # scene parsing, before RenderConfig owns the finished scene.
            scene_file = props.Get("scene.file").GetString()
            scene_props = pyluxcore.Properties(scene_file)
            resize_props = pyluxcore.Properties()
            resize_props.Set(pyluxcore.Property(
                "scene.images.resizepolicy.type", ["FIXED"]))
            resize_props.Set(pyluxcore.Property(
                "scene.images.resizepolicy.scale", [HDRI_DOWNSAMPLE_SCALE]))
            resize_props.Set(pyluxcore.Property(
                "scene.images.resizepolicy.minsize", [HDRI_DOWNSAMPLE_MIN_SIZE]))
            self._scene = pyluxcore.Scene(scene_props, resize_props)
            _source_file = self._hdr_file or DEFAULT_HDRI_FILE
            _gain = 10.0 ** self._hdri_gain_log.get()
            _apply_environment_maps(
                self._scene, _source_file, _gain,
                self._render_hdri_background.get())
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
                            hdr_file=None, hdri_gain=None,
                            render_hdri_background=None, scene_update=None):
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
                    self._config.Parse(pyluxcore.Properties()
                        .Set(pyluxcore.Property("film.imagepipelines.0.0.exposure", [exp]))
                        .Set(pyluxcore.Property("film.imagepipelines.1.1.exposure", [exp]))
                        .Set(pyluxcore.Property("film.width", [width]))
                        .Set(pyluxcore.Property("film.height", [height])))
                    if (hdr_file or hdri_gain is not None
                            or render_hdri_background is not None):
                        _source_file = hdr_file if hdr_file else (
                            self._hdr_file or DEFAULT_HDRI_FILE
                        )
                        _gain = hdri_gain if hdri_gain is not None else (
                            10.0 ** self._hdri_gain_log.get())
                        _apply_environment_maps(
                            self._scene, _source_file, _gain,
                            render_hdri_background)
                    if scene_update:
                        config_texts, meshes, scene_texts = scene_update
                        for text in config_texts:
                            extra = pyluxcore.Properties()
                            extra.SetFromString(text)
                            self._config.Parse(extra)
                        for mesh_name, data in meshes.items():
                            mesh_kwargs = {}
                            if data["normals"] is not None:
                                mesh_kwargs["normals"] = data["normals"]
                            if data["uvs"] is not None:
                                mesh_kwargs["uvs"] = [data["uvs"]]
                            self._scene.DefineMeshExt(
                                mesh_name, data["points"], data["triangles"],
                                **mesh_kwargs)
                        for text in scene_texts:
                            extra = pyluxcore.Properties()
                            extra.SetFromString(text)
                            self._scene.Parse(extra)
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

    def _on_hdri_gain(self):
        self._update_info()
        self._save_settings()
        if self._scene and self._session and not self._render_stopped:
            gain = 10.0 ** self._hdri_gain_log.get()
            self._camera_revision += 1
            self._camera_snapshot = self._capture_camera_snapshot()
            self._camera_restart_pending = True
            threading.Thread(
                target=self._do_restart_session,
                args=(self._render_width, self._render_height, "full",
                      self._camera_snapshot, None, gain,
                      self._render_hdri_background.get()),
                daemon=True).start()
    def _on_render_hdri_background_changed(self, *_):
        self._update_info()
        self._save_settings()
        if self._scene and self._session and not self._render_stopped:
            gain = 10.0 ** self._hdri_gain_log.get()
            self._camera_revision += 1
            self._camera_snapshot = self._capture_camera_snapshot()
            self._camera_restart_pending = True
            threading.Thread(
                target=self._do_restart_session,
                args=(self._render_width, self._render_height, "full",
                      self._camera_snapshot, None, gain,
                      self._render_hdri_background.get()),
                daemon=True).start()

    # ── External control server ──────────────────────────────────────────────
    def _start_control_server(self):
        """Accept localhost command connections from external programs."""
        if self._control_port <= 0:
            return
        try:
            server = socket.create_server((CONTROL_HOST, self._control_port))
        except OSError as ex:
            self._info.config(
                text=f"Control port {self._control_port} unavailable: {ex}")
            return
        self._control_socket = server
        threading.Thread(target=self._control_accept_loop, args=(server,),
                         daemon=True).start()

    def _control_accept_loop(self, server):
        while True:
            try:
                conn, _ = server.accept()
            except OSError:
                return
            threading.Thread(target=self._control_client_loop, args=(conn,),
                             daemon=True).start()

    def _control_client_loop(self, conn):
        """Serve one connection; commands run on the Tk thread via a queue."""
        with conn:
            while True:
                try:
                    header, blobs = _read_control_message(conn)
                except (ConnectionError, OSError):
                    return
                except Exception as ex:
                    try:
                        _send_control_message(conn, {"ok": False, "error": str(ex)})
                    except OSError:
                        pass
                    return
                reply_queue = queue.Queue(maxsize=1)
                self._control_commands.put((header, blobs, reply_queue))
                try:
                    reply = reply_queue.get(timeout=60.0)
                except queue.Empty:
                    reply = {"ok": False, "error": "Command timed out"}
                try:
                    _send_control_message(conn, reply)
                except OSError:
                    return

    def _process_control_commands(self):
        try:
            while True:
                header, blobs, reply_queue = self._control_commands.get_nowait()
                try:
                    reply = self._execute_control_command(header, blobs)
                except Exception as ex:
                    reply = {"ok": False, "error": str(ex)}
                try:
                    reply_queue.put_nowait(reply)
                except queue.Full:
                    pass
        except queue.Empty:
            pass
        self.after(25, self._process_control_commands)

    def _execute_control_command(self, header, blobs):
        cmd = _header_command(header)
        if cmd == "status":
            return self._control_status()
        if cmd == "camera":
            if "az" in header:
                self.az.set(float(header["az"]))
            if "el" in header:
                self.el.set(max(-89.0, min(89.0, float(header["el"]))))
            if "dist" in header:
                self.dist.set(max(1.0, min(50.0, float(header["dist"]))))
            self._on_camera()
            return {"ok": True, "azimuth": self.az.get(),
                    "elevation": self.el.get(), "distance": self.dist.get()}
        if cmd == "target":
            xyz = header.get("xyz")
            if not isinstance(xyz, (list, tuple)) or len(xyz) != 3:
                raise ValueError("target needs xyz: [x, y, z]")
            self._target = [float(value) for value in xyz]
            self._on_camera()
            return {"ok": True, "target": list(self._target)}
        if cmd == "preset":
            self._set_preset(float(header["az"]), float(header["el"]))
            return {"ok": True}
        if cmd == "reset":
            self._reset()
            return {"ok": True}
        if cmd == "exposure":
            self.exposure.set(max(0.001, min(20.0, float(header["value"]))))
            self._on_exposure()
            return {"ok": True, "exposure": self.exposure.get()}
        if cmd == "hdri_gain":
            gain = max(0.0001, min(100.0, float(header["value"])))
            self._hdri_gain_log.set(math.log10(gain))
            self._on_hdri_gain()
            return {"ok": True, "hdri_gain": 10.0 ** self._hdri_gain_log.get()}
        if cmd == "hdri_file":
            path = os.path.normpath(str(header.get("path", "")))
            if not _is_env_map(path) or not os.path.isfile(path):
                raise ValueError("hdri_file needs an existing .hdr or .exr path")
            self._on_hdr_file_drop(path)
            return {"ok": True, "hdr_file": path}
        if cmd == "background":
            wanted = bool(header.get("hdri", True))
            if wanted != self._render_hdri_background.get():
                self._render_hdri_background.set(wanted)  # trace restarts render
            return {"ok": True, "render_hdri_background": wanted}
        if cmd == "resolution":
            width, height = int(header["width"]), int(header["height"])
            if not (16 <= width <= 8192 and 16 <= height <= 8192):
                raise ValueError("resolution must be between 16 and 8192 pixels")
            self.render_resolution.set(f"{width} x {height}")
            self._set_render_resolution()
            return {"ok": True, "width": self._render_width,
                    "height": self._render_height}
        if cmd == "pipeline":
            index = int(header.get("index", 0))
            if index not in (0, 1):
                raise ValueError("pipeline index must be 0 (raw) or 1 (OIDN)")
            self._set_pipeline(index)
            return {"ok": True, "pipeline": self.pipeline}
        if cmd == "stop":
            self._stop_rendering()
            return {"ok": True}
        if cmd == "start":
            self._restart_rendering()
            return {"ok": True}
        if cmd == "save_film":
            saved = self._write_film_image(str(header.get("path", "")))
            return {"ok": True, "path": saved}
        if cmd == "define_mesh":
            return self._stage_control_mesh(header, blobs)
        if cmd == "upload_mesh":
            return self._stage_uploaded_mesh(header, blobs)
        if cmd == "scene_props":
            text = header.get("text")
            if not isinstance(text, str) or not text.strip():
                raise ValueError("scene_props needs LuxCore property text")
            self._pending_scene_props.append(text)
            return {"ok": True,
                    "staged_scene_props": len(self._pending_scene_props)}
        if cmd == "config_props":
            text = header.get("text")
            if not isinstance(text, str) or not text.strip():
                raise ValueError("config_props needs LuxCore property text")
            self._pending_config_props.append(text)
            return {"ok": True,
                    "staged_config_props": len(self._pending_config_props)}
        if cmd == "apply":
            return self._apply_staged_updates()
        if cmd == "shutdown":
            self.after(50, self._on_close)
            return {"ok": True}
        raise ValueError(f"Unknown control command: {cmd}")

    def _control_status(self):
        status = {
            "ok": True,
            "azimuth": self.az.get(),
            "elevation": self.el.get(),
            "distance": self.dist.get(),
            "target": list(self._target),
            "exposure": self.exposure.get(),
            "hdri_gain": 10.0 ** self._hdri_gain_log.get(),
            "render_hdri_background": bool(self._render_hdri_background.get()),
            "hdr_file": self._hdr_file or DEFAULT_HDRI_FILE,
            "width": self._render_width,
            "height": self._render_height,
            "pipeline": self.pipeline,
            "render_stopped": self._render_stopped,
            "staged_meshes": sorted(self._pending_meshes),
            "staged_scene_props": len(self._pending_scene_props),
            "staged_config_props": len(self._pending_config_props),
        }
        if self._restart_lock.acquire(timeout=0.25):
            try:
                if (self._session and self._session.IsStarted()
                        and not self._camera_restart_pending):
                    self._session.UpdateStats()
                    stats = self._session.GetStats()
                    status["passes"] = stats.Get(
                        "stats.renderengine.pass").GetInt()
                    status["render_seconds"] = stats.Get(
                        "stats.renderengine.time").GetFloat()
                    status["samples_per_second"] = stats.Get(
                        "stats.renderengine.total.samplesec").GetFloat()
            finally:
                self._restart_lock.release()
        else:
            status["busy"] = True
        return status

    def _stage_control_mesh(self, header, blobs):
        """Stage a mesh from raw little-endian buffers until the next apply."""
        try:
            import numpy
        except ImportError as ex:
            raise RuntimeError(
                "define_mesh requires numpy: python -m pip install numpy") from ex
        name = header.get("name")
        if not isinstance(name, str) or not name.strip():
            raise ValueError("define_mesh needs a mesh name")
        name = name.strip()

        def read_buffer(role, dtype, stride, required):
            data = blobs.get(role)
            if data is None:
                if required:
                    raise ValueError(f"define_mesh is missing the {role} buffer")
                return None
            item_size = numpy.dtype(dtype).itemsize * stride
            if not data or len(data) % item_size:
                raise ValueError(
                    f"The {role} buffer must hold N x {stride} {dtype} values")
            return numpy.frombuffer(data, dtype=dtype).reshape(-1, stride)

        points = read_buffer("points", numpy.float32, 3, True)
        triangles = read_buffer("triangles", numpy.uint32, 3, True)
        normals = read_buffer("normals", numpy.float32, 3, False)
        uvs = read_buffer("uvs", numpy.float32, 2, False)
        if normals is not None and len(normals) != len(points):
            raise ValueError("normals must provide one entry per vertex")
        if uvs is not None and len(uvs) != len(points):
            raise ValueError("uvs must provide one entry per vertex")
        if int(triangles.max()) >= len(points):
            raise ValueError("A triangle index is out of range")
        self._pending_meshes[name] = {
            "points": points, "triangles": triangles,
            "normals": normals, "uvs": uvs,
        }
        return {"ok": True, "mesh": name,
                "vertices": int(len(points)), "triangles": int(len(triangles))}

    def _stage_uploaded_mesh(self, header, blobs):
        """Accept the C# upload_mesh layout: vertices, normals, uvs, indices."""
        try:
            import numpy
        except ImportError as ex:
            raise RuntimeError(
                "upload_mesh requires numpy: python -m pip install numpy") from ex
        raw_name = (header.get("MeshName") or header.get("meshName")
                    or header.get("mesh_name"))
        if not isinstance(raw_name, str) or not raw_name.strip():
            raise ValueError("upload_mesh needs a MeshName")
        # Property names cannot carry spaces or dots, so sanitize the identity.
        name = re.sub(r"[^0-9A-Za-z_\-]", "_", raw_name.strip())

        def section_components(key, default):
            section = header.get(key)
            if isinstance(section, dict):
                element_size = int(section.get("ElementSize", 0))
                if element_size in (8, 12):
                    return element_size // 4
                fmt = str(section.get("Format", ""))
                if fmt.endswith("x3"):
                    return 3
                if fmt.endswith("x2"):
                    return 2
            return default

        vertex_data = blobs.get("vertices")
        index_data = blobs.get("indices")
        if not vertex_data or not index_data:
            raise ValueError("upload_mesh needs vertex and index buffers")
        if len(vertex_data) % 12:
            raise ValueError("The vertex buffer must hold N x 3 float32 values")
        points = numpy.frombuffer(vertex_data, dtype=numpy.float32).reshape(-1, 3)

        index_format = ""
        if isinstance(header.get("Indices"), dict):
            index_format = str(header["Indices"].get("Format", ""))
        index_dtype = numpy.uint32 if "uint" in index_format else numpy.int32
        if len(index_data) % 4:
            raise ValueError("The index buffer must hold 32-bit integers")
        flat_indices = numpy.frombuffer(index_data, dtype=index_dtype)
        if flat_indices.size == 0 or flat_indices.size % 3:
            raise ValueError("The index count must be a positive multiple of 3")
        if int(flat_indices.min()) < 0 or int(flat_indices.max()) >= len(points):
            raise ValueError("A triangle index is out of range")
        triangles = numpy.ascontiguousarray(
            flat_indices.astype(numpy.uint32).reshape(-1, 3))

        normals = None
        normal_data = blobs.get("normals")
        if normal_data:
            if len(normal_data) % 12:
                raise ValueError("The normals buffer must hold N x 3 float32 values")
            normals = numpy.frombuffer(
                normal_data, dtype=numpy.float32).reshape(-1, 3)
            if len(normals) != len(points):
                raise ValueError("normals must provide one entry per vertex")

        uvs = None
        uv_data = blobs.get("uvs")
        if uv_data:
            components = section_components("UVs", 3)
            if len(uv_data) % (4 * components):
                raise ValueError(
                    f"The uvs buffer must hold N x {components} float32 values")
            uv_array = numpy.frombuffer(
                uv_data, dtype=numpy.float32).reshape(-1, components)
            if components == 3:
                # IwVector3f UVs carry an unused third component.
                uv_array = numpy.ascontiguousarray(uv_array[:, :2])
            if len(uv_array) != len(points):
                raise ValueError("uvs must provide one entry per vertex")
            uvs = uv_array

        self._pending_meshes[name] = {
            "points": points, "triangles": triangles,
            "normals": normals, "uvs": uvs,
        }
        create_object = bool(header.get(
            "CreateObject", header.get("create_object", True)))
        if create_object:
            self._pending_scene_props.append("\n".join((
                f"scene.materials.{name}_mat.type = matte",
                f"scene.materials.{name}_mat.kd = 0.6 0.6 0.6",
                f"scene.objects.{name}.shape = {name}",
                f"scene.objects.{name}.material = {name}_mat",
            )))
        self._schedule_upload_apply()
        return {"ok": True, "mesh": name,
                "vertices": int(len(points)),
                "triangles": int(len(triangles)),
                "object_created": create_object}

    def _schedule_upload_apply(self):
        """Debounce one automatic apply after a burst of streamed uploads."""
        if self._upload_apply_id:
            self.after_cancel(self._upload_apply_id)
        self._upload_apply_id = self.after(
            UPLOAD_APPLY_MS, self._auto_apply_uploads)

    def _auto_apply_uploads(self):
        self._upload_apply_id = None
        if self._render_stopped or not self._scene or not self._session:
            return
        if not (self._pending_meshes or self._pending_scene_props
                or self._pending_config_props):
            return
        try:
            self._apply_staged_updates()
        except Exception as ex:
            self._info.config(text=f"Auto apply failed: {ex}")

    def _apply_staged_updates(self):
        """Apply staged meshes and property text with one render restart."""
        if self._upload_apply_id:
            self.after_cancel(self._upload_apply_id)
            self._upload_apply_id = None
        if not (self._pending_meshes or self._pending_scene_props
                or self._pending_config_props):
            raise ValueError("Nothing staged: send define_mesh, scene_props, "
                             "or config_props first")
        if self._render_stopped or not self._scene or not self._session:
            raise RuntimeError("Rendering is stopped: send start before apply")
        update = (list(self._pending_config_props),
                  dict(self._pending_meshes),
                  list(self._pending_scene_props))
        self._pending_config_props = []
        self._pending_meshes = {}
        self._pending_scene_props = []
        self._camera_revision += 1
        self._camera_snapshot = self._capture_camera_snapshot()
        self._camera_restart_pending = True
        threading.Thread(
            target=self._do_restart_session,
            args=(self._render_width, self._render_height, "full",
                  self._camera_snapshot),
            kwargs={"scene_update": update},
            daemon=True).start()
        return {"ok": True,
                "meshes": sorted(update[1]),
                "scene_prop_blocks": len(update[2]),
                "config_prop_blocks": len(update[0])}

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
    def _write_film_image(self, path):
        """Write the current film to path as PNG or JPEG and return the path."""
        if not path:
            raise ValueError("A film output path is required")
        path = os.path.abspath(path)
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
        return path

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
            saved = self._write_film_image(path)
            self._render_win.title(
                f"{WINDOW_TITLE} — Saved: {os.path.basename(saved)}")
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
        gain = 10.0 ** self._hdri_gain_log.get()
        self._info.config(text=f"  az={az:6.1f}°  el={el:5.1f}°"
                               f"  dist={self.dist.get():5.2f}"
                               f"  exp={self.exposure.get():.3f}"
                               f"  hdri={gain:.3f}"
                               f"  bg={'HDRI' if self._render_hdri_background.get() else 'white'}"
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
