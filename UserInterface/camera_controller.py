"""
camera_controller.py  —  Integrated pyluxcore render controller.

Starts an empty stage (scene.scn) lit by the default HDRI; geometry and
camera updates stream in through the TCP control interface (see README.md).

No external process launches. The render session runs in-process:
  - Camera orbit/zoom  → scene edit followed by a debounced session restart
  - Exposure change    → config.Parse + session restart           (fast, no window close)
  - Pipeline switch    → flip display index only                  (instant, no restart)

Controls:
  Left-drag on render viewport → pan
  Right-drag on render viewport → orbit (azimuth / elevation)
  Scroll wheel                 → zoom
  R                       → reset camera
  Space                   → force film refresh
"""

import os, sys, math, re, queue, threading, json, socket, struct, ctypes, hashlib, tempfile
from array import array
try:
    from tkinterdnd2 import COPY, DND_FILES, TkinterDnD
except ImportError:
    COPY = DND_FILES = TkinterDnD = None

# ── Bootstrap pyluxcore ──────────────────────────────────────────────────────
UI_DIR         = os.path.dirname(os.path.abspath(__file__))
LUXCORE_ROOT   = os.path.dirname(UI_DIR)
PYLUXCORE_PATH = os.path.join(LUXCORE_ROOT, r"out\build\src\pyluxcore\Release")
LUXCORE_BIN    = os.path.join(LUXCORE_ROOT, r"out\install\Release\bin")
SCENE_DIR      = UI_DIR
SCENE_FILE     = os.path.join(SCENE_DIR, "scene.scn")
CFG_FILE       = os.path.join(SCENE_DIR, "render.cfg")
CUDA_12_4_NVRTC = (
    r"C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v12.4\bin\nvrtc64_120_0.dll")

SETTINGS_FILE = os.path.join(UI_DIR, "camera_controller_settings.json")

if sys.stdout is None or sys.stderr is None:
    # pythonw.exe runs without a console; keep tracebacks in render.log.
    _console_log = open(os.path.join(UI_DIR, "render.log"), "a",
                        buffering=1, encoding="utf-8")
    sys.stdout = sys.stdout or _console_log
    sys.stderr = sys.stderr or _console_log

_dll_search_paths = (os.path.dirname(sys.executable), LUXCORE_BIN, PYLUXCORE_PATH)
os.environ["PATH"] = os.pathsep.join(
    dict.fromkeys((*_dll_search_paths, os.environ.get("PATH", ""))))
_python_dll_directory = os.add_dll_directory(os.path.dirname(sys.executable))
_luxcore_dll_directory = os.add_dll_directory(LUXCORE_BIN)
_nvrtc_library = os.environ.get("LUXRAYS_NVRTC_LIBRARY", CUDA_12_4_NVRTC)
if os.path.isfile(_nvrtc_library):
    # CUDA 13.3 emits PTX 9.3, which the installed OptiX driver cannot load.
    # CUEW reads this absolute-path override before trying its normal DLL list.
    os.environ["LUXRAYS_NVRTC_LIBRARY"] = _nvrtc_library
    _nvrtc_dll_directory = os.add_dll_directory(os.path.dirname(_nvrtc_library))
sys.path.insert(0, PYLUXCORE_PATH)
os.chdir(SCENE_DIR)  # scene and HDRI paths are relative to this directory

import pyluxcore
from PIL import Image, ImageTk
import tkinter as tk
from tkinter import colorchooser, filedialog, ttk

# ── Defaults ──────────────────────────────────────────────────────────────────
DEFAULT_HDRI_FILE   = "hdre_055.hdr"  # fallback if settings has no valid hdr_file
HDRI_ALIGNMENT_CACHE_DIR = os.path.join(
    tempfile.gettempdir(), "luxcore-hdri-alignment")
HDRI_ALIGNMENT_CACHE_VERSION = 1
HDRI_GROUND_SOURCE_TOKEN = "__LUXCORE_ALIGNED_HDRI_SOURCE__"
# Downsample large HDR/EXR lighting maps in memory to a 2K-wide proxy.
# The camera-only background retains the original source resolution.
HDRI_DOWNSAMPLE_SCALE = 0.125
HDRI_DOWNSAMPLE_MIN_SIZE = 64
DEFAULT_AZ          = 80.0
DEFAULT_EL          = 5.0
DEFAULT_SWITCH_SECS = 5
DEFAULT_TARGET      = [0.0, 0.0, 0.5]
FILM_W              = 1280
FILM_H              = 720
RENDER_RESOLUTIONS  = ("640 x 360", "1280 x 720", "1920 x 1080",
                       "2560 x 1440", "3840 x 2160")
RENDER_SCALE_FACTORS = {
    "1/20": (1, 20), "1/10": (1, 10), "1/5": (1, 5),
    "1/3": (1, 3), "1/2": (1, 2), "1": (1, 1),
    "2": (2, 1), "3": (3, 1), "4": (4, 1), "5": (5, 1),
}
RENDER_SCALE_OPTIONS = tuple(RENDER_SCALE_FACTORS)
DEFAULT_RENDER_SCALE = "1"
RENDER_MIN_DIMENSION = 16
RENDER_MAX_DIMENSION = 8192
WINDOW_GEOMETRY_RE = re.compile(
    r"^(?P<width>\d+)x(?P<height>\d+)(?P<x>[+-]\d+)(?P<y>[+-]\d+)$")
WINDOW_GEOMETRY_MIN_WIDTH = 320
WINDOW_GEOMETRY_MIN_HEIGHT = 240
WINDOW_GEOMETRY_MAX_SIZE = 16384
WINDOW_GEOMETRY_MAX_POSITION = 32768
WINDOW_GEOMETRY_POLL_MS = 250
CONTROL_W           = 216
PREVIEW_W           = 192
PREVIEW_H           = 108
REFRESH_MS          = 250
OIDN_REFRESH_MS     = 2000   # denoising a full frame on the CPU is slow
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

# ── Ambient-occlusion (clay) mode ───────────────────────────────────────────
AO_WHITE_MATERIAL = "\n".join((
    "scene.materials.ao_white.type = matte",
    "scene.materials.ao_white.kd = 1.0 1.0 1.0"))
# The flat overhead lamp matches the tracked geometry's XY bounding box
# and floats directly over it, so the softbox is exactly as big as the
# object. Its established emission is retained independently of size; the
# lamp is hidden from glossy/specular continuation rays to keep the AO view
# free of visible softbox reflections.
AO_LAMP_NAME = "ao_lamp"
AO_LAMP_EMISSION = 1.4
AO_LAMP_HEIGHT_FACTOR = 0.75    # lamp height above the bounds top vs extent
AO_LAMP_MIN_HALF_EXTENT = 0.05  # minimum lamp half-size vs extent
# The overhead softbox remains the dominant AO illumination. A small white
# dome fill prevents curved, self-occluded areas (such as a torus crest)
# from collapsing to absolute black because of local normal discontinuities.
AO_DOME_FILL_GAIN = 0.15
AO_PATH_DEPTHS = "\n".join((
    "path.pathdepth.total = 3",
    "path.pathdepth.diffuse = 2",
    "path.pathdepth.glossy = 2",
    "path.pathdepth.specular = 2"))
DEFAULT_PATH_DEPTHS = "\n".join((
    "path.pathdepth.total = 6",
    "path.pathdepth.diffuse = 4",
    "path.pathdepth.glossy = 4",
    "path.pathdepth.specular = 6"))
# A plain linear tonemap makes the unit-gain AO dome read as pure white,
# independent of the Reinhard exposure the normal pipelines use.
AO_TONEMAP = "\n".join((
    "film.imagepipelines.0.0.type = TONEMAP_LINEAR",
    "film.imagepipelines.0.0.scale = 1.0",
    "film.imagepipelines.1.1.type = TONEMAP_LINEAR",
    "film.imagepipelines.1.1.scale = 1.0"))
# Tone-map HDRI renders with a chromatic highlight roll-off. LuxLinear simply
# multiplies the RGB channels, so high HDRI gains clip channels at display
# white and flatten bright saturated colors.
HDR_TONEMAP_REFERENCE_EXPOSURE = 5.0
HDR_TONEMAP_REFERENCE_POSTSCALE = 1.2
HDR_TONEMAP_BURN = 3.75
HDR_TONEMAP_GAMMA = 2.2

def _reinhard_tonemap(exposure):
    """Complete normal HDRI pipelines with exposure scaling.

    RenderConfig.Parse() replaces a pipeline's plug-in list when it receives
    any properties for that pipeline. Keep the gamma correction and OIDN
    plug-ins in restart properties so camera/HDRI restarts match startup.
    """
    postscale = (HDR_TONEMAP_REFERENCE_POSTSCALE * exposure
                 / HDR_TONEMAP_REFERENCE_EXPOSURE)
    return "\n".join((
        "film.imagepipelines.0.0.type = TONEMAP_REINHARD02",
        "film.imagepipelines.0.0.prescale = 1.0",
        f"film.imagepipelines.0.0.postscale = {postscale}",
        f"film.imagepipelines.0.0.burn = {HDR_TONEMAP_BURN}",
        "film.imagepipelines.0.1.type = GAMMA_CORRECTION",
        f"film.imagepipelines.0.1.value = {HDR_TONEMAP_GAMMA}",
        "film.imagepipelines.1.0.type = INTEL_OIDN",
        "film.imagepipelines.1.0.prefilter.enable = 0",
        "film.imagepipelines.1.1.type = TONEMAP_REINHARD02",
        "film.imagepipelines.1.1.prescale = 1.0",
        f"film.imagepipelines.1.1.postscale = {postscale}",
        f"film.imagepipelines.1.1.burn = {HDR_TONEMAP_BURN}",
        "film.imagepipelines.1.2.type = GAMMA_CORRECTION",
        f"film.imagepipelines.1.2.value = {HDR_TONEMAP_GAMMA}"))

# ── HDRI ground plane ────────────────────────────────────────────────────────────────────
# Two coincident disks around Z = 0. The shadow catcher (just above) is
# fully transparent wherever the environment is unoccluded, so the
# full-resolution HDRI background shows through; where meshes block the
# environment light it shades with a texture that projects the HDRI's
# lower hemisphere straight up onto the plane (nadir at the disk center,
# horizon at the rim), so shadows read as darkened ground. An archglass
# mirror layer at Z = 0 below it adds Fresnel-weighted reflections of the
# meshes: subtle seen from above, stronger at grazing angles, exactly
# like a polished floor. It sits below the catcher so the catcher's
# upward occlusion tests never cross it.
GROUND_NAME = "hdri_ground"
GROUND_MESH_NAME = "hdri_ground_mesh"
GROUND_MIRROR_NAME = "hdri_ground_mirror"
GROUND_MIRROR_MESH_NAME = "hdri_ground_mirror_mesh"
GROUND_SEGMENTS = 64          # radial columns around the disk
GROUND_RINGS = 24             # rows at uniform view-angle steps
GROUND_RADIUS_FACTOR = 20.0   # disk radius vs scene extent (2× original)
GROUND_RADIUS_SCALE = 5.0     # extra enlargement of the final disk radius
GROUND_CATCHER_LIFT = 2e-4    # catcher height above the mirror vs extent
GROUND_MIRROR_KR = 1.0        # scales Fresnel reflection; 1.0 = physical
GROUND_GRAY_KD = 0.4          # white-backdrop mode: floor albedo
GROUND_GRAY_ROUGHNESS = 0.04  # white-backdrop mode: glossy roughness
GROUND_GRAY_MIRROR_ROUGHNESS = 0.001
GROUND_MIRROR_IOR = 100.0     # near-perfect Fresnel mirror at full strength

# ── Math helpers ──────────────────────────────────────────────────────
def _norm(v):
    """Return a unit-length copy of a vector, preserving near-zero vectors."""
    l = math.sqrt(sum(c*c for c in v))
    return [c/l for c in v] if l > 1e-10 else v

def _cross(a, b):
    """Return the three-dimensional cross product of two vectors."""
    return [a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0]]

def _dot(a, b):
    """Return the three-dimensional dot product of two vectors."""
    return sum(a[i]*b[i] for i in range(3))

def orbit(target, dist, az_deg, el_deg):
    """Return a camera position orbiting a target at the requested spherical angles."""
    az = math.radians(az_deg)
    el = math.radians(max(-89, min(89, el_deg)))
    return [target[0] + dist*math.cos(el)*math.sin(az),
            target[1] + dist*math.cos(el)*math.cos(az),
            target[2] + dist*math.sin(el)]

def cam_axes(orig, target):
    """Return normalized camera right, up, and forward axes for an eye and target."""
    fwd   = _norm([target[i]-orig[i] for i in range(3)])
    right = _norm(_cross(fwd, WORLD_UP))
    up    = _norm(_cross(right, fwd))
    return right, up, fwd


def _luxcore_fieldofview(fov_degrees, axis, width, height):
    """Convert a vertical/horizontal FOV to LuxCore's fieldofview.

    LuxCore's fieldofview spans the film axis whose screen window is [-1, 1]:
    the horizontal axis for landscape films and the vertical axis for
    portrait films (see ProjectiveCamera::Update).
    """
    frame = width / float(height)
    half_angle = math.radians(fov_degrees) * 0.5
    if frame >= 1.0:
        if axis == "horizontal":
            return fov_degrees
        return math.degrees(2.0 * math.atan(math.tan(half_angle) * frame))
    if axis == "vertical":
        return fov_degrees
    return math.degrees(2.0 * math.atan(math.tan(half_angle) / frame))

# ── Exposure I/O ──────────────────────────────────────────────────────────────
def read_exposure(path):
    """Read the current exposure from a render configuration, including legacy encodings."""
    legacy_exposure = None
    legacy_prescale = None
    with open(path) as f:
        for line in f:
            m = re.match(r"film\.imagepipelines\.0\.0\.postscale\s*=\s*([\d.eE+\-]+)", line.strip())
            if m:
                return (float(m.group(1)) * HDR_TONEMAP_REFERENCE_EXPOSURE
                        / HDR_TONEMAP_REFERENCE_POSTSCALE)
            m = re.match(r"film\.imagepipelines\.0\.0\.prescale\s*=\s*([\d.eE+\-]+)", line.strip())
            if m:
                legacy_prescale = float(m.group(1))
            m = re.match(r"film\.imagepipelines\.0\.0\.exposure\s*=\s*([\d.eE+\-]+)", line.strip())
            if m:
                legacy_exposure = float(m.group(1))
    if legacy_exposure is not None:
        return legacy_exposure
    if legacy_prescale is not None:
        return 1.0 / max(legacy_prescale, 1e-6)
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
    """Load persisted controller settings, returning an empty mapping when unavailable."""
    try:
        with open(SETTINGS_FILE, encoding="utf-8") as settings_file:
            settings = json.load(settings_file)
            return settings if isinstance(settings, dict) else {}
    except (OSError, ValueError):
        return {}

def _setting_float(settings, name, default, minimum, maximum):
    """Return a numeric setting clamped to an inclusive range or its fallback value."""
    try:
        return max(minimum, min(maximum, float(settings.get(name, default))))
    except (TypeError, ValueError):
        return default
def _setting_bool(settings, name, default):
    """Return a boolean setting only when its stored value is explicitly boolean."""
    value = settings.get(name, default)
    return value if isinstance(value, bool) else default

def _setting_color(settings, name, default):
    """Return a normalized six-digit hexadecimal color setting or its fallback value."""
    value = settings.get(name, default)
    if (isinstance(value, str)
            and re.fullmatch(r"#[0-9a-fA-F]{6}", value)):
        return value.lower()
    return default

def _setting_render_resolution(settings):
    """Validate and normalize a persisted base viewport resolution."""
    value = settings.get("render_resolution")
    if not isinstance(value, str):
        return "1280 x 720"
    match = re.fullmatch(r"\s*(\d+)\s*x\s*(\d+)\s*", value)
    if not match:
        return "1280 x 720"
    width, height = (int(component) for component in match.groups())
    if not 16 <= width <= 8192 or not 16 <= height <= 8192:
        return "1280 x 720"
    return f"{width} x {height}"
def _setting_scale(settings, name, default=DEFAULT_RENDER_SCALE):
    """Return a supported named render scale or the default scale."""
    value = settings.get(name, default)
    return value if value in RENDER_SCALE_FACTORS else DEFAULT_RENDER_SCALE
def _setting_window_scale(settings):
    # render_scale was the short-lived name used before final-film scaling
    # became independent; retain it as a window-scale migration fallback.
    """Read the window scale, migrating the legacy render_scale setting when necessary."""
    return _setting_scale(
        settings, "window_scale", settings.get("render_scale", DEFAULT_RENDER_SCALE))

def _setting_film_scale(settings):
    """Read and validate the persisted final-film scale."""
    return _setting_scale(settings, "film_scale")
def _setting_final_film_resolution(settings):
    """Return a validated exact external film-size override, if one is persisted."""
    value = settings.get("final_film_resolution")
    if not isinstance(value, str):
        return None
    match = re.fullmatch(r"\s*(\d+)\s*x\s*(\d+)\s*", value)
    if not match:
        return None
    width, height = (int(component) for component in match.groups())
    if not (RENDER_MIN_DIMENSION <= width <= RENDER_MAX_DIMENSION
            and RENDER_MIN_DIMENSION <= height <= RENDER_MAX_DIMENSION):
        return None
    return width, height

def _scaled_viewport_resolution(width, height, scale_label):
    """Return an aspect-preserving viewport size for a named window scale."""
    numerator, denominator = RENDER_SCALE_FACTORS[scale_label]
    scaled_width = max(1, round(width * numerator / denominator))
    scaled_height = max(1, round(height * numerator / denominator))
    return scaled_width, scaled_height

def _scaled_film_resolution(width, height, scale_label):
    """Return a viewport-relative film size, clamped uniformly to 8K."""
    numerator, denominator = RENDER_SCALE_FACTORS[scale_label]
    scale = min(numerator / denominator,
                RENDER_MAX_DIMENSION / max(width, height))
    scaled_width = max(RENDER_MIN_DIMENSION, min(
        RENDER_MAX_DIMENSION, round(width * scale)))
    scaled_height = max(RENDER_MIN_DIMENSION, min(
        RENDER_MAX_DIMENSION, round(height * scale)))
    return scaled_width, scaled_height

def _setting_window_geometry(settings):
    """Validate and normalize a persisted window size and virtual-desktop position."""
    value = settings.get("window_geometry")
    if not isinstance(value, str):
        return None
    match = WINDOW_GEOMETRY_RE.fullmatch(value)
    if not match:
        return None
    width = int(match.group("width"))
    height = int(match.group("height"))
    x = int(match.group("x"))
    y = int(match.group("y"))
    if not (WINDOW_GEOMETRY_MIN_WIDTH <= width <= WINDOW_GEOMETRY_MAX_SIZE
            and WINDOW_GEOMETRY_MIN_HEIGHT <= height <= WINDOW_GEOMETRY_MAX_SIZE
            and abs(x) <= WINDOW_GEOMETRY_MAX_POSITION
            and abs(y) <= WINDOW_GEOMETRY_MAX_POSITION):
        return None
    return f"{width}x{height}{x:+d}{y:+d}"

def _hex_color_to_rgb(value):
    """Convert a #RRGGBB color string to normalized RGB channel values."""
    return tuple(int(value[index:index + 2], 16) / 255.0
                 for index in (1, 3, 5))

def _setting_target(settings):
    """Return a validated three-component camera target or the default target."""
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
    """Return a valid persisted HDRI file path, if the source still exists."""
    hdr_file = settings.get("hdr_file")
    if (isinstance(hdr_file, str) and _is_env_map(hdr_file)
            and os.path.isfile(hdr_file)):
        return os.path.normpath(hdr_file)
    return None

def _ignore_luxcore_log(_message):
    """Discard LuxCore log messages routed through the quiet logger callback."""
    pass
def _environment_storage(path):
    """Retain full HDR precision for environment and HDRI-ground maps."""
    return "float"
def _hdri_yaw_matrix(rotation_degrees):
    """Return LuxCore's column-major world-Z yaw transform for an HDRI."""
    yaw = math.radians(rotation_degrees)
    cos_yaw, sin_yaw = math.cos(yaw), math.sin(yaw)
    return (
        cos_yaw, sin_yaw, 0.0, 0.0,
        -sin_yaw, cos_yaw, 0.0, 0.0,
        0.0, 0.0, 1.0, 0.0,
        0.0, 0.0, 0.0, 1.0,
    )

def _hdri_vertical_offset_pixels(height_degrees, image_height):
    """Convert a height control value to a half-rate equirectangular row offset."""
    return float(height_degrees) * float(image_height) / 360.0
def _read_radiance_header(stream):
    """Read a Radiance RGBE header and its canonical equirectangular dimensions."""
    header = bytearray()
    first_line = stream.readline()
    if not first_line.startswith(b"#?"):
        raise ValueError("The HDRI is not a Radiance RGBE file")
    header.extend(first_line)
    while True:
        line = stream.readline()
        if not line:
            raise ValueError("The Radiance HDRI header is incomplete")
        header.extend(line)
        if line in (b"\n", b"\r\n"):
            break
    if b"FORMAT=32-bit_rle_rgbe" not in header:
        raise ValueError("Only 32-bit_rle_rgbe Radiance HDRIs can be vertically shifted")
    resolution = stream.readline().decode("ascii", "replace").strip()
    match = re.fullmatch(r"-Y\s+(\d+)\s+\+X\s+(\d+)", resolution)
    if not match:
        raise ValueError(
            "HDRI vertical offset requires standard '-Y height +X width' orientation")
    height, width = (int(value) for value in match.groups())
    if width <= 0 or height <= 0:
        raise ValueError("The Radiance HDRI dimensions are invalid")
    return bytes(header), width, height

def _read_radiance_scanline(stream, width):
    """Read one RGBE row, accepting modern and legacy Radiance encodings."""
    first = stream.read(4)
    if len(first) != 4:
        raise ValueError("The Radiance HDRI ended before all pixels were read")
    if (8 <= width <= 0x7fff and first[0] == 2 and first[1] == 2
            and not (first[2] & 0x80)
            and ((first[2] << 8) | first[3]) == width):
        components = []
        for _ in range(4):
            values = bytearray()
            while len(values) < width:
                code = stream.read(1)
                if not code:
                    raise ValueError("The Radiance HDRI ended inside an RLE scanline")
                count = code[0]
                if count > 128:
                    count -= 128
                    value = stream.read(1)
                    if not value or count == 0 or len(values) + count > width:
                        raise ValueError("The Radiance HDRI has invalid RLE data")
                    values.extend(value * count)
                else:
                    literal = stream.read(count)
                    if count == 0 or len(literal) != count or len(values) + count > width:
                        raise ValueError("The Radiance HDRI has invalid RLE data")
                    values.extend(literal)
            components.append(values)
        row = bytearray(width * 4)
        for component, values in enumerate(components):
            row[component::4] = values
        return bytes(row)

    row = bytearray(width * 4)
    row[:4] = first
    previous = first
    index = 1
    repeat_shift = 0
    while index < width:
        pixel = stream.read(4)
        if len(pixel) != 4:
            raise ValueError("The Radiance HDRI ended inside a legacy scanline")
        if pixel[:3] == b"\x01\x01\x01":
            count = pixel[3] << repeat_shift
            if count == 0 or index + count > width:
                raise ValueError("The Radiance HDRI has invalid legacy RLE data")
            for _ in range(count):
                row[index * 4:(index + 1) * 4] = previous
                index += 1
            repeat_shift += 8
        else:
            row[index * 4:(index + 1) * 4] = pixel
            previous = pixel
            index += 1
            repeat_shift = 0
    return bytes(row)

def _write_radiance_component(stream, values):
    """Write one modern Radiance RLE component stream."""
    index = 0
    length = len(values)
    while index < length:
        run = 1
        while (index + run < length and run < 127
               and values[index + run] == values[index]):
            run += 1
        if run >= 4:
            stream.write(bytes((128 + run, values[index])))
            index += run
            continue
        literal_start = index
        while index < length:
            run = 1
            while (index + run < length and run < 127
                   and values[index + run] == values[index]):
                run += 1
            if run >= 4 or index - literal_start >= 128:
                break
            index += 1
        literal = values[literal_start:index]
        stream.write(bytes((len(literal),)))
        stream.write(literal)

def _write_radiance_scanline(stream, row, width):
    """Write an RGBE row using modern Radiance RLE when its width permits it."""
    if not 8 <= width <= 0x7fff:
        stream.write(row)
        return
    stream.write(bytes((2, 2, width >> 8, width & 0xff)))
    for component in range(4):
        _write_radiance_component(stream, row[component::4])

def _rgbe_row_to_rgb(row):
    """Decode one packed RGBE row to scene-linear float RGB values."""
    try:
        import numpy
    except ImportError as ex:
        raise RuntimeError(
            "HDRI vertical offset requires numpy: python -m pip install numpy") from ex
    rgbe = numpy.frombuffer(row, dtype=numpy.uint8).reshape(-1, 4)
    rgb = numpy.zeros((len(rgbe), 3), dtype=numpy.float32)
    active = rgbe[:, 3] != 0
    if numpy.any(active):
        scale = numpy.ldexp(
            numpy.ones(int(numpy.count_nonzero(active)), dtype=numpy.float32),
            rgbe[active, 3].astype(numpy.int16) - 136)
        rgb[active] = rgbe[active, :3].astype(numpy.float32) * scale[:, None]
    return rgb

def _rgb_row_to_rgbe(rgb):
    """Encode scene-linear float RGB values to one packed RGBE row."""
    try:
        import numpy
    except ImportError as ex:
        raise RuntimeError(
            "HDRI vertical offset requires numpy: python -m pip install numpy") from ex
    rgb = numpy.maximum(rgb, 0.0)
    maximum = rgb.max(axis=1)
    row = numpy.zeros((len(rgb), 4), dtype=numpy.uint8)
    active = maximum > 1e-32
    if numpy.any(active):
        mantissa, exponent = numpy.frexp(maximum[active])
        scale = mantissa * 256.0 / maximum[active]
        row[active, :3] = numpy.clip(
            rgb[active] * scale[:, None], 0.0, 255.0).astype(numpy.uint8)
        row[active, 3] = numpy.clip(exponent + 128, 0, 255).astype(numpy.uint8)
    return row.tobytes()

def _interpolate_rgbe_rows(first, second, fraction):
    """Linearly interpolate two RGBE rows in scene-linear radiance space."""
    first_rgb = _rgbe_row_to_rgb(first)
    second_rgb = _rgbe_row_to_rgb(second)
    return _rgb_row_to_rgbe(first_rgb * (1.0 - fraction) + second_rgb * fraction)

def _remap_radiance_hdr(source_file, destination_file, vertical_offset_pixels):
    """Shift a Radiance RGBE panorama without loading the entire image at once."""
    with open(source_file, "rb") as source:
        header, width, height = _read_radiance_header(source)
        with open(destination_file, "wb") as destination:
            destination.write(header)
            destination.write(f"-Y {height} +X {width}\n".encode("ascii"))
            rows = {}
            last_loaded = -1

            def source_row(index):
                nonlocal last_loaded
                while last_loaded < index:
                    last_loaded += 1
                    rows[last_loaded] = _read_radiance_scanline(source, width)
                return rows[index]

            for output_y in range(height):
                source_y = min(
                    float(height - 1),
                    max(0.0, output_y + vertical_offset_pixels))
                lower = int(math.floor(source_y))
                fraction = source_y - lower
                upper = min(lower + 1, height - 1)
                first = source_row(lower)
                if fraction > 1e-8 and upper != lower:
                    row = _interpolate_rgbe_rows(first, source_row(upper), fraction)
                else:
                    row = first
                _write_radiance_scanline(destination, row, width)
                for index in tuple(rows):
                    if index < lower:
                        del rows[index]
def _openexr_dimensions(source_file):
    """Return an OpenEXR HDRI's dimensions without decoding any channels."""
    try:
        import OpenEXR
    except ImportError as ex:
        raise RuntimeError("EXR HDRI vertical offset requires OpenEXR") from ex
    source = OpenEXR.InputFile(source_file)
    try:
        data_window = source.header()["dataWindow"]
        width = data_window.max.x - data_window.min.x + 1
        height = data_window.max.y - data_window.min.y + 1
        if width <= 0 or height <= 0:
            raise ValueError("The EXR HDRI dimensions are invalid")
        return width, height
    finally:
        source.close()

def _remap_openexr(source_file, destination_file, vertical_offset_pixels):
    """Shift an OpenEXR panorama while preserving its scene-linear float RGB."""
    try:
        import Imath
        import OpenEXR
        import numpy
    except ImportError as ex:
        raise RuntimeError(
            "EXR HDRI vertical offset requires OpenEXR and numpy") from ex
    width, height = _openexr_dimensions(source_file)
    source = OpenEXR.InputFile(source_file)
    try:
        positions = numpy.clip(
            numpy.arange(height, dtype=numpy.float32) + vertical_offset_pixels,
            0.0, float(height - 1))
        lower = numpy.floor(positions).astype(numpy.intp)
        upper = numpy.minimum(lower + 1, height - 1)
        fraction = (positions - lower).astype(numpy.float32)[:, None]
        pixel_type = Imath.PixelType(Imath.PixelType.FLOAT)
        channels = {}
        for channel_name in ("R", "G", "B"):
            try:
                pixels = numpy.frombuffer(
                    source.channel(channel_name, pixel_type),
                    dtype=numpy.float32).reshape(height, width)
            except Exception as ex:
                raise ValueError(
                    f"The EXR HDRI does not have a readable {channel_name} channel") from ex
            shifted = (pixels[lower] * (1.0 - fraction)
                       + pixels[upper] * fraction)
            channels[channel_name] = numpy.ascontiguousarray(
                shifted, dtype=numpy.float32).tobytes()
        header = OpenEXR.Header(width, height)
        header["channels"] = {
            name: Imath.Channel(pixel_type) for name in ("R", "G", "B")}
        output = OpenEXR.OutputFile(destination_file, header)
        try:
            output.writePixels(channels)
        finally:
            output.close()
    finally:
        source.close()

def _aligned_hdri_file(source_file, height_degrees):
    """Return the source HDRI or a cached equirectangular vertical remap."""
    source_file = os.path.abspath(source_file)
    if abs(float(height_degrees)) < 1e-8:
        return source_file
    if not os.path.isfile(source_file):
        raise ValueError(f"HDRI file is unavailable: {source_file}")
    extension = os.path.splitext(source_file)[1].lower()
    if extension not in (".hdr", ".exr"):
        raise ValueError("HDRI vertical offset requires a .hdr or .exr file")
    source_info = os.stat(source_file)
    cache_key = "\0".join((
        str(HDRI_ALIGNMENT_CACHE_VERSION), os.path.normcase(source_file),
        str(source_info.st_size), str(source_info.st_mtime_ns),
        f"{float(height_degrees):.4f}"))
    cache_name = hashlib.sha256(cache_key.encode("utf-8")).hexdigest() + extension
    os.makedirs(HDRI_ALIGNMENT_CACHE_DIR, exist_ok=True)
    cache_file = os.path.join(HDRI_ALIGNMENT_CACHE_DIR, cache_name)
    if os.path.isfile(cache_file) and os.path.getsize(cache_file) > 0:
        return cache_file
    temporary_file = (
        f"{cache_file}.{os.getpid()}.{threading.get_ident()}.tmp")
    try:
        if extension == ".hdr":
            with open(source_file, "rb") as source:
                _, _, height = _read_radiance_header(source)
            _remap_radiance_hdr(
                source_file, temporary_file,
                _hdri_vertical_offset_pixels(height_degrees, height))
        else:
            _, height = _openexr_dimensions(source_file)
            _remap_openexr(
                source_file, temporary_file,
                _hdri_vertical_offset_pixels(height_degrees, height))
        os.replace(temporary_file, cache_file)
    finally:
        try:
            os.remove(temporary_file)
        except FileNotFoundError:
            pass
    return cache_file

def _hdri_ground_uv(phi, alpha, transform):
    """Map a ground-projection direction through the HDRI transform to UV."""
    # The disk's original projection is a direction in the lower hemisphere:
    # alpha=0 is the nadir at its center and alpha=pi/2 its horizon at the rim.
    sin_alpha, cos_alpha = math.sin(alpha), math.cos(alpha)
    world_x = sin_alpha * math.cos(phi)
    world_y = sin_alpha * math.sin(phi)
    world_z = -cos_alpha
    # The infinite light samples inverse(lightToWorld) * world direction.
    # For the orthonormal rotation matrix above that inverse is its transpose.
    local_x = (world_x * transform[0] + world_y * transform[1]
               + world_z * transform[2])
    local_y = (world_x * transform[4] + world_y * transform[5]
               + world_z * transform[6])
    local_z = (world_x * transform[8] + world_y * transform[9]
               + world_z * transform[10])
    u = (math.atan2(local_y, local_x) / (2.0 * math.pi)) % 1.0
    v = math.acos(max(-1.0, min(1.0, local_z))) / math.pi
    return u, v

def _set_environment_visibility(props, prefix, camera_visible, direct_visible,
                                indirect_diffuse, indirect_glossy,
                                indirect_specular):
    """Set LuxCore ray-visibility flags for an environment-light property prefix."""
    props.Set(pyluxcore.Property(
        f"{prefix}.visibility.camera.enable", [camera_visible]))
    props.Set(pyluxcore.Property(
        f"{prefix}.visibility.direct.enable", [direct_visible]))
    props.Set(pyluxcore.Property(
        f"{prefix}.visibility.indirect.diffuse.enable", [indirect_diffuse]))
    props.Set(pyluxcore.Property(
        f"{prefix}.visibility.indirect.glossy.enable", [indirect_glossy]))
    props.Set(pyluxcore.Property(
        f"{prefix}.visibility.indirect.specular.enable", [indirect_specular]))

def _apply_environment_maps(scene, source_file, gain, render_hdri_background=True,
                            ao_mode=False, hdri_height=0.0,
                            hdri_rotation=0.0):
    """Use a reduced lighting map plus an HDRI, white, or AO-dome background."""
    props = pyluxcore.Properties()
    lighting_prefix = "scene.lights.hdri"
    background_prefix = "scene.lights.hdri_background"
    if ao_mode:
        # Ambient-occlusion clay mode: a flat camera-invisible area light
        # above the scene (built separately from the mesh bounds) remains
        # dominant. A weak white dome fill prevents total-black crests, and
        # a camera-only dome provides a medium-gray backdrop. These gains are
        # fixed so AO is independent of the HDRI Gain slider; 0.22 linear
        # displays as ~50% gray after the linear AO tonemap and gamma 2.2.
        ao_gain = AO_DOME_FILL_GAIN
        ao_background_gain = 0.22
        props.Set(pyluxcore.Property(
            f"{lighting_prefix}.type", ["constantinfinite"]))
        props.Set(pyluxcore.Property(f"{lighting_prefix}.color", [1.0, 1.0, 1.0]))
        props.Set(pyluxcore.Property(
            f"{lighting_prefix}.gain", [ao_gain, ao_gain, ao_gain]))
        _set_environment_visibility(props, lighting_prefix, False, True,
                                    True, True, True)
        props.Set(pyluxcore.Property(
            f"{background_prefix}.type", ["constantinfinite"]))
        props.Set(pyluxcore.Property(
            f"{background_prefix}.color", [1.0, 1.0, 1.0]))
        props.Set(pyluxcore.Property(
            f"{background_prefix}.gain",
            [ao_background_gain, ao_background_gain, ao_background_gain]))
        _set_environment_visibility(props, background_prefix, True, False,
                                    False, False, True)
        scene.Parse(props)
        scene.RemoveUnusedImageMaps()
        return None
    source_file = _aligned_hdri_file(source_file, hdri_height)
    storage = _environment_storage(source_file)
    transform = _hdri_yaw_matrix(hdri_rotation)
    props.Set(pyluxcore.Property(f"{lighting_prefix}.type", ["infinite"]))
    props.Set(pyluxcore.Property(f"{lighting_prefix}.file", [source_file]))
    props.Set(pyluxcore.Property(f"{lighting_prefix}.colorspace", ["nop"]))
    props.Set(pyluxcore.Property(f"{lighting_prefix}.storage", [storage]))
    props.Set(pyluxcore.Property(f"{lighting_prefix}.gain", [gain, gain, gain]))
    props.Set(pyluxcore.Property(
        f"{lighting_prefix}.transformation", list(transform)))
    props.Set(pyluxcore.Property(f"{lighting_prefix}.resizepolicy.enable", [True]))
    # With an HDRI camera background, specular continuations use the sharp
    # background map. Otherwise they use this lighting map.
    _set_environment_visibility(props, lighting_prefix, False, True,
                                True, True, not render_hdri_background)

    if render_hdri_background:
        props.Set(pyluxcore.Property(f"{background_prefix}.type", ["infinite"]))
        props.Set(pyluxcore.Property(f"{background_prefix}.file", [source_file]))
        props.Set(pyluxcore.Property(f"{background_prefix}.colorspace", ["nop"]))
        props.Set(pyluxcore.Property(f"{background_prefix}.storage", [storage]))
        props.Set(pyluxcore.Property(f"{background_prefix}.gain", [gain, gain, gain]))
        props.Set(pyluxcore.Property(
            f"{background_prefix}.transformation", list(transform)))
        props.Set(pyluxcore.Property(
            f"{background_prefix}.resizepolicy.enable", [False]))
        # The full-resolution background remains visible to camera,
        # reflected, and transmitted rays.
        _set_environment_visibility(props, background_prefix, True, False,
                                    False, False, True)
    else:
        props.Set(pyluxcore.Property(
            f"{background_prefix}.type", ["constantinfinite"]))
        props.Set(pyluxcore.Property(f"{background_prefix}.color", [1.0, 1.0, 1.0]))
        _set_environment_visibility(props, background_prefix, True, False,
                                    False, False, False)

    scene.Parse(props)
    scene.RemoveUnusedImageMaps()
    return source_file

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
        """Initialize the controller window, persisted state, UI, renderer state, and control server."""
        super().__init__()
        self.title(WINDOW_TITLE)
        self.resizable(False, False)
        self._window_icon = tk.PhotoImage(file=WINDOW_ICON)
        self.iconphoto(True, self._window_icon)
        self._settings = _read_controller_settings()
        self._settings_ready = False
        self._saved_window_geometry = _setting_window_geometry(self._settings)
        self._window_geometry_save_id = None
        self._window_geometry_poll_id = None
        self._hdr_file = _setting_hdr_file(self._settings)
        self._active_hdri_file = self._hdr_file or DEFAULT_HDRI_FILE
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
        saved_resolution = _setting_render_resolution(self._settings)
        saved_window_scale = _setting_window_scale(self._settings)
        saved_film_scale = _setting_film_scale(self._settings)
        saved_final_film_resolution = _setting_final_film_resolution(
            self._settings)
        self.az = tk.DoubleVar(value=_setting_float(
            self._settings, "azimuth", DEFAULT_AZ, -3600.0, 3600.0))
        self.el = tk.DoubleVar(value=_setting_float(
            self._settings, "elevation", DEFAULT_EL, -89.0, 89.0))
        self._camera_distance = _setting_float(
            self._settings, "distance", 20.0, 0.001, 1.0e9)
        self.exposure = tk.DoubleVar(value=_setting_float(
            self._settings, "exposure", read_exposure(CFG_FILE), 0.001, 20.0))
        saved_hdri_gain = _setting_float(
            self._settings, "hdri_gain", read_hdri_gain(SCENE_FILE), 0.0001, 100.0)
        self._hdri_gain_log = tk.DoubleVar(
            value=math.log10(max(0.0001, min(100.0, saved_hdri_gain))))
        self._hdri_gain_jog = tk.DoubleVar(value=0.0)
        self._hdri_gain_jog_in_progress = False
        self._hdri_height = tk.DoubleVar(value=_setting_float(
            self._settings, "hdri_height", 0.0, -45.0, 45.0))
        self._hdri_rotation = tk.DoubleVar(value=_setting_float(
            self._settings, "hdri_rotation", 0.0, -180.0, 180.0))
        self._render_hdri_background = tk.BooleanVar(value=_setting_bool(
            self._settings, "render_hdri_background", True))
        self._ao_mode = tk.BooleanVar(value=_setting_bool(
            self._settings, "ao_mode", False))
        self._hdri_ground = tk.BooleanVar(value=_setting_bool(
            self._settings, "hdri_ground", False))
        self._ground_color = tk.StringVar(value=_setting_color(
            self._settings, "ground_color", "#666666"))
        legacy_ground_reflectivity = _setting_float(
            self._settings, "ground_hdri_reflectivity",
            _setting_float(self._settings, "ground_gray_reflectivity",
                           GROUND_MIRROR_KR, 0.0, 1.0),
            0.0, 1.0)
        self._ground_reflectivity = tk.DoubleVar(value=_setting_float(
            self._settings, "ground_reflectivity", legacy_ground_reflectivity,
            0.0, 1.0))
        self.switch_sec = tk.IntVar(value=round(_setting_float(
            self._settings, "auto_oidn_seconds", DEFAULT_SWITCH_SECS, 1.0, 120.0)))
        self.render_resolution = tk.StringVar(value=saved_resolution)
        self.window_scale = tk.StringVar(value=saved_window_scale)
        self.film_scale = tk.StringVar(value=saved_film_scale)
        self._base_viewport_width, self._base_viewport_height = (
            int(value) for value in saved_resolution.split(" x "))
        self._viewport_width, self._viewport_height = _scaled_viewport_resolution(
            self._base_viewport_width, self._base_viewport_height,
            saved_window_scale)
        self._film_resolution_override = saved_final_film_resolution
        self._render_width, self._render_height = (
            saved_final_film_resolution or _scaled_film_resolution(
                self._viewport_width, self._viewport_height, saved_film_scale))
        self.pipeline   = 0

        self._drag_x    = 0
        self._drag_y    = 0
        self._switch_id = None
        self._skip_frames = 0
        self._gate_pass   = 0   # minimum pass count before showing new frame
        self._target = _setting_target(self._settings)
        self._camera_up = [0.0, 0.0, 1.0]
        self._camera_fov = None          # optional (degrees, axis) override
        self._scene_fieldofview = 45.0   # scene default, read at session start
        self._session   = None
        self._config    = None
        self._scene     = None
        self._restart_lock = threading.Lock()
        self._restart_results = queue.Queue()
        self._control_commands = queue.Queue()
        self._pending_meshes = {}
        self._pending_scene_props = []
        self._pending_config_props = []
        self._uploaded_objects = {}  # object name -> (shape, material)
        self._mesh_bounds = {}       # mesh name -> (min xyz, max xyz)
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
        self._control_shell = tk.Frame(self._main_frame)
        self._control_shell.grid(row=0, column=0, rowspan=2, sticky="ns")
        self._control_viewport = tk.Canvas(
            self._control_shell, width=CONTROL_W, height=FILM_H,
            highlightthickness=0, borderwidth=0)
        self._control_scrollbar = ttk.Scrollbar(
            self._control_shell, orient=tk.VERTICAL,
            command=self._control_viewport.yview)
        self._control_viewport.configure(
            yscrollcommand=self._control_scrollbar.set)
        self._control_viewport.pack(side=tk.LEFT, fill=tk.Y)
        self._control_scrollbar.pack(side=tk.RIGHT, fill=tk.Y)
        self._control_panel = tk.Frame(self._control_viewport)
        self._control_panel_window = self._control_viewport.create_window(
            0, 0, anchor=tk.NW, window=self._control_panel, width=CONTROL_W)
        self._control_panel.bind(
            "<Configure>", self._update_control_panel_scrollregion)
        self._render_canvas = tk.Canvas(self._main_frame,
                                        width=FILM_W, height=FILM_H, bg="black",
                                        cursor="fleur")
        self._render_canvas.grid(row=0, column=1)
        self._render_footer_frame = tk.Frame(self._main_frame)
        self._render_footer_frame.grid(row=1, column=1, sticky="ew", pady=(4, 0))
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
        self._apply_display_size()

        self._build_ui()
        self._settings_ready = True
        self.bind("<Configure>", self._on_window_configure, add="+")
        if self._saved_window_geometry:
            self.after_idle(self._restore_window_geometry)
        self._window_geometry_poll_id = self.after(
            WINDOW_GEOMETRY_POLL_MS, self._poll_window_geometry)
        self.switch_sec.trace_add("write", self._on_setting_variable_changed)
        self._render_hdri_background.trace_add(
            "write", self._on_render_hdri_background_changed)
        self._ao_mode.trace_add("write", self._on_ao_mode_changed)
        self._hdri_ground.trace_add("write", self._on_hdri_ground_changed)
        self.protocol("WM_DELETE_WINDOW", self._on_close)
        self._update_info()
        self.after(25, self._process_restart_results)
        self.after(25, self._process_control_commands)
        self._enable_hdr_file_drop()
        self._start_control_server()
        self.after(300, self._start_session)

    def _save_settings(self):
        """Persist the current controller, viewport, render, and window settings atomically."""
        if not self._settings_ready:
            return
        settings = {
            "azimuth": self.az.get(),
            "elevation": self.el.get(),
            "distance": self._camera_distance,
            "target": self._target,
            "exposure": self.exposure.get(),
            "hdri_gain": 10.0 ** self._hdri_gain_log.get(),
            "hdri_height": self._hdri_height.get(),
            "hdri_rotation": self._hdri_rotation.get(),
            "render_hdri_background": self._render_hdri_background.get(),
            "ao_mode": self._ao_mode.get(),
            "hdri_ground": self._hdri_ground.get(),
            "ground_color": self._ground_color.get(),
            "ground_reflectivity": self._ground_reflectivity.get(),
            "auto_oidn_seconds": self.switch_sec.get(),
            "render_resolution": self.render_resolution.get(),
            "window_scale": self.window_scale.get(),
            "film_scale": self.film_scale.get(),
            "control_port": self._control_port_setting,
        }
        geometry = self._current_window_geometry()
        if geometry:
            self._saved_window_geometry = geometry
        if self._saved_window_geometry:
            settings["window_geometry"] = self._saved_window_geometry
        if self._hdr_file:
            settings["hdr_file"] = self._hdr_file
        if self._film_resolution_override:
            width, height = self._film_resolution_override
            settings["final_film_resolution"] = f"{width} x {height}"
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
    def _current_window_geometry(self):
        """Return a validated snapshot of the current top-level geometry."""
        try:
            x, y = self.winfo_rootx(), self.winfo_rooty()
            if os.name == "nt":
                frame = (ctypes.c_long * 4)()
                hwnd = int(self.wm_frame(), 0)
                if ctypes.windll.user32.GetWindowRect(hwnd, ctypes.byref(frame)):
                    x, y = frame[0], frame[1]
            # Windows reports hidden or minimized windows at this sentinel
            # location. Retain the last real placement instead of persisting it.
            if x <= -32000 or y <= -32000:
                return None
            geometry = f"{self.winfo_width()}x{self.winfo_height()}{x:+d}{y:+d}"
        except tk.TclError:
            return None
        return _setting_window_geometry({"window_geometry": geometry})

    def _restore_window_geometry(self):
        """Restore size and absolute virtual-desktop position after layout."""
        if not self._saved_window_geometry:
            return
        match = WINDOW_GEOMETRY_RE.fullmatch(self._saved_window_geometry)
        if not match:
            return
        width = int(match.group("width"))
        height = int(match.group("height"))
        x = int(match.group("x"))
        y = int(match.group("y"))
        self.geometry(f"{width}x{height}")
        # The after-idle callback can run before Windows maps the top-level
        # frame, in which case SetWindowPos() is silently ignored.
        self.after(50, self._set_window_position, x, y)

    def _set_window_position(self, x, y):
        """Move the top-level to an absolute Windows virtual-desktop position."""
        try:
            hwnd = int(self.wm_frame(), 0)
            flags = 0x0001 | 0x0004 | 0x0010  # no size, z-order, or activation
            if ctypes.windll.user32.SetWindowPos(hwnd, None, x, y, 0, 0, flags):
                return
        except (AttributeError, OSError, ValueError, tk.TclError):
            pass
        self.geometry(f"{self.winfo_width()}x{self.winfo_height()}{x:+d}{y:+d}")
    def _save_window_geometry(self):
        """Complete a debounced geometry save by persisting current settings."""
        self._window_geometry_save_id = None
        self._save_settings()

    def _on_window_configure(self, event):
        """Debounce persistent geometry updates while the window is moved."""
        if event.widget is not self or not self._settings_ready:
            return
        if self._window_geometry_save_id:
            self.after_cancel(self._window_geometry_save_id)
        self._window_geometry_save_id = self.after(250, self._save_window_geometry)
    def _poll_window_geometry(self):
        """Persist native monitor moves that do not emit Tk Configure events."""
        self._window_geometry_poll_id = None
        if self._settings_ready:
            geometry = self._current_window_geometry()
            if geometry and geometry != self._saved_window_geometry:
                self._saved_window_geometry = geometry
                self._save_settings()
        if self.winfo_exists():
            self._window_geometry_poll_id = self.after(
                WINDOW_GEOMETRY_POLL_MS, self._poll_window_geometry)

    def _on_setting_variable_changed(self, *_):
        """Persist a Tk setting variable after it changes."""
        self._save_settings()

    def _on_close(self):
        """Stop rendering, close the control socket, persist settings, and destroy the window."""
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
        """Parse a native file-drop event and load the first valid environment map."""
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
        """Display a file-drop failure in the status area and window title."""
        self._info.config(text=message)
        self._render_win.title(f"{WINDOW_TITLE} — {message}")

    def _on_hdr_file_drop(self, hdr_file):
        """Validate a dropped environment map and select or reload it for rendering."""
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
        scene_update = None
        if (self._hdri_ground.get() and not self._ao_mode.get()
                and self._render_hdri_background.get()):
            # The HDRI-catcher texture must follow the new env map.
            # The gray floor used when the background is off has no
            # texture dependency, so it needs no rebuild.
            ground_meshes, ground_text = self._hdri_ground_update(hdr_file)
            scene_update = ([], ground_meshes, [ground_text], [])
        self._start_restart_worker(
            self._render_width, self._render_height, "full",
            self._camera_snapshot, hdr_file=hdr_file, hdri_gain=gain,
            render_hdri_background=self._render_hdri_background.get(),
            ao_mode=self._ao_mode.get(), scene_update=scene_update)

    # ── UI ────────────────────────────────────────────────────────────────────
    def _build_ui(self):
        """Create the scrollable controller panel and render-footer controls."""
        panel = self._control_panel
        pad = dict(padx=6, pady=2)

        self._info = tk.Label(panel, text="Starting...", font=("Consolas", 8),
                              fg="#444", justify=tk.LEFT, anchor="w")
        tk.Label(panel, text="Controls:", font=("Segoe UI", 10, "bold"),
                 anchor="w").grid(row=0, column=0, sticky="w", padx=8)
        tk.Label(panel,
                 text="Left-drag: pan\nRight-drag: rotate\nScroll forward: zoom in\n"
                      "Scroll back: zoom out\nDrag and Drop HDR or EXR into scene",
                 justify=tk.LEFT, anchor="w", font=("Segoe UI", 8), fg="#666"
                 ).grid(row=1, column=0, sticky="w", padx=8, pady=(0, 6))

        # Distance is driven by the scroll wheel and the control interface;
        # exposure by the exposure control command and saved settings.
        tk.Checkbutton(panel, text="Render HDRI Background",
                       variable=self._render_hdri_background
                       ).grid(row=2, column=0, sticky="w", **pad)
        tk.Checkbutton(panel, text="Ambient Occlusion (clay)",
                       variable=self._ao_mode
                       ).grid(row=3, column=0, sticky="w", **pad)
        tk.Checkbutton(panel, text="Ground Plane",
                       variable=self._hdri_ground
                       ).grid(row=4, column=0, sticky="w", **pad)

        delay_frame = tk.Frame(panel)
        delay_frame.grid(row=5, column=0, pady=(2, 0))
        tk.Label(delay_frame, text="Auto OIDN").pack(side=tk.LEFT, padx=(2, 3))
        tk.Spinbox(delay_frame, from_=1, to=120, textvariable=self.switch_sec,
                   width=3, font=("Segoe UI", 8)).pack(side=tk.LEFT)
        tk.Label(delay_frame, text="sec").pack(side=tk.LEFT, padx=(3, 2))
        resolution_frame = tk.Frame(panel)
        resolution_frame.grid(row=6, column=0, pady=(3, 0))
        tk.Label(resolution_frame, text="Viewport Base").pack(
            side=tk.LEFT, padx=(2, 4))
        resolution_menu = ttk.Combobox(
            resolution_frame, textvariable=self.render_resolution,
            values=RENDER_RESOLUTIONS, state="readonly", width=12)
        resolution_menu.pack(side=tk.LEFT)
        resolution_menu.bind("<<ComboboxSelected>>", self._set_render_resolution)
        scale_frame = tk.Frame(panel)
        scale_frame.grid(row=7, column=0, pady=(1, 0))
        tk.Label(scale_frame, text="Window Scale").pack(
            side=tk.LEFT, padx=(2, 4))
        window_scale_menu = ttk.Combobox(
            scale_frame, textvariable=self.window_scale,
            values=RENDER_SCALE_OPTIONS, state="readonly", width=5)
        window_scale_menu.pack(side=tk.LEFT)
        window_scale_menu.bind("<<ComboboxSelected>>", self._set_window_scale)
        film_scale_frame = tk.Frame(panel)
        film_scale_frame.grid(row=8, column=0, pady=(1, 0))
        tk.Label(film_scale_frame, text="Final Film Scale").pack(
            side=tk.LEFT, padx=(2, 4))
        film_scale_menu = ttk.Combobox(
            film_scale_frame, textvariable=self.film_scale,
            values=RENDER_SCALE_OPTIONS, state="readonly", width=5)
        film_scale_menu.pack(side=tk.LEFT)
        film_scale_menu.bind("<<ComboboxSelected>>", self._set_film_scale)
        self._film_size_label = tk.Label(
            film_scale_frame,
            text=f"{self._render_width} x {self._render_height}",
            font=("Consolas", 8), fg="#666")
        self._film_size_label.pack(side=tk.LEFT, padx=(4, 0))

        self.bind("<r>", lambda _: self._reset())
        self.bind("<R>", lambda _: self._reset())

        self._preset_choice = tk.StringVar(value="Camera View")
        preset_values = ("Camera View", "Front", "Back", "Left Side", "Right Side",
                         "Hero Front Left", "Hero Front Right", "Hero Back Left",
                         "Hero Back Right", "Top", "Bottom")
        preset_menu = ttk.Combobox(
            panel, textvariable=self._preset_choice, values=preset_values,
            state="readonly", width=22)
        preset_menu.grid(row=9, column=0, pady=(4, 2))
        preset_menu.bind("<<ComboboxSelected>>", self._select_preset)

        tk.Button(panel, text="Save Film", bg="#2a6aba", fg="white",
                  font=("Segoe UI", 9, "bold"), width=22,
                  command=self._save_film
                  ).grid(row=10, column=0, pady=(3, 6))
        self._render_button = tk.Button(
            panel, text="Stop Rendering", bg="#9c2929", fg="white",
            font=("Segoe UI", 9, "bold"), width=22,
            command=self._stop_rendering)
        self._render_button.grid(row=11, column=0, pady=(0, 6))
        hdri_height_frame = tk.Frame(panel)
        hdri_height_frame.grid(row=12, column=0, sticky="ew", padx=6)
        tk.Label(hdri_height_frame, text="HDRI Height").grid(
            row=0, column=0, sticky="w")
        self._hdri_height_value_label = tk.Label(
            hdri_height_frame, width=7, anchor="e",
            font=("Consolas", 9), fg="#444")
        self._hdri_height_value_label.grid(row=0, column=1, sticky="e")
        tk.Scale(hdri_height_frame, from_=-45.0, to=45.0, resolution=0.1,
                 orient=tk.HORIZONTAL, variable=self._hdri_height,
                 length=188, showvalue=False, command=self._on_hdri_alignment
                 ).grid(row=1, column=0, columnspan=2, sticky="ew")
        hdri_rotation_frame = tk.Frame(panel)
        hdri_rotation_frame.grid(row=13, column=0, sticky="ew", padx=6, pady=(1, 0))
        tk.Label(hdri_rotation_frame, text="HDRI Rotation").grid(
            row=0, column=0, sticky="w")
        self._hdri_rotation_value_label = tk.Label(
            hdri_rotation_frame, width=7, anchor="e",
            font=("Consolas", 9), fg="#444")
        self._hdri_rotation_value_label.grid(row=0, column=1, sticky="e")
        tk.Scale(hdri_rotation_frame, from_=-180.0, to=180.0, resolution=0.1,
                 orient=tk.HORIZONTAL, variable=self._hdri_rotation,
                 length=188, showvalue=False, command=self._on_hdri_alignment
                 ).grid(row=1, column=0, columnspan=2, sticky="ew")

        footer = self._render_footer_frame
        gain_frame = tk.Frame(footer)
        gain_frame.grid(row=0, column=0, sticky="w")
        tk.Label(gain_frame, text="HDRI Gain").pack(side=tk.LEFT, padx=(2, 4))
        self._hdri_gain_value_label = tk.Label(
            gain_frame, width=6, anchor="w", font=("Consolas", 9), fg="#444")
        self._hdri_gain_value_label.pack(side=tk.LEFT)
        tk.Scale(gain_frame, from_=-1.0, to=1.0, resolution=0.01,
                 orient=tk.HORIZONTAL, variable=self._hdri_gain_jog,
                 length=144, showvalue=False,
                 command=self._adjust_hdri_gain).pack(side=tk.LEFT, padx=(4, 0))
        pipeline_frame = tk.Frame(footer)
        pipeline_frame.grid(row=0, column=1, sticky="w", padx=(8, 0))
        tk.Label(pipeline_frame, text="Pipeline").pack(side=tk.LEFT, padx=(0, 4))
        self._pipeline_choice = tk.StringVar(
            value="OIDN" if self.pipeline == 1 else "Raw")
        pipeline_menu = ttk.Combobox(
            pipeline_frame, textvariable=self._pipeline_choice,
            values=("Raw", "OIDN"), state="readonly", width=5)
        pipeline_menu.pack(side=tk.LEFT)
        pipeline_menu.bind("<<ComboboxSelected>>", self._select_pipeline)
        ground_frame = tk.Frame(footer)
        ground_frame.grid(row=1, column=0, columnspan=2, sticky="w", pady=(2, 0))
        tk.Label(ground_frame, text="Ground Color").pack(side=tk.LEFT, padx=(2, 4))
        self._ground_color_button = tk.Button(
            ground_frame, text="Color", width=8, command=self._choose_ground_color)
        self._ground_color_button.pack(side=tk.LEFT)
        tk.Label(ground_frame, text="Reflection strength").pack(
            side=tk.LEFT, padx=(8, 4))
        tk.Scale(ground_frame, from_=0.0, to=1.0, resolution=0.01,
                 orient=tk.HORIZONTAL, variable=self._ground_reflectivity,
                 length=128, showvalue=False,
                 command=lambda _: self._on_ground_appearance_changed()
                 ).pack(side=tk.LEFT)
        self._update_hdri_gain_label()
        self._update_hdri_alignment_labels()
        self._update_ground_color_button()

    # ── Session ───────────────────────────────────────────────────────────────
    def _start_session(self):
        """Create and start the initial LuxCore scene, configuration, and full-resolution session."""
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
            props = pyluxcore.Properties(CFG_FILE)
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
            if self._ao_mode.get():
                props.SetFromString(AO_PATH_DEPTHS)
                props.SetFromString(AO_TONEMAP)

            # Supply the resize policy to Scene itself: image maps load during
            # scene parsing, before RenderConfig owns the finished scene.
            scene_file = props.Get("scene.file").GetString()
            scene_props = pyluxcore.Properties(scene_file)
            if scene_props.IsDefined("scene.camera.fieldofview"):
                self._scene_fieldofview = scene_props.Get(
                    "scene.camera.fieldofview").GetFloat()
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
            self._active_hdri_file = _apply_environment_maps(
                self._scene, _source_file, _gain,
                self._render_hdri_background.get(), self._ao_mode.get(),
                self._hdri_height.get(), self._hdri_rotation.get())
            if self._ao_mode.get():
                lamp_props = pyluxcore.Properties()
                lamp_props.SetFromString(self._ao_lamp_scene_text())
                self._scene.Parse(lamp_props)
            if self._hdri_ground.get():
                ground_meshes, ground_text = self._hdri_ground_update()
                for mesh_name, mesh_data in ground_meshes.items():
                    mesh_kwargs = {}
                    if mesh_data["normals"] is not None:
                        mesh_kwargs["normals"] = mesh_data["normals"]
                    if mesh_data["uvs"] is not None:
                        mesh_kwargs["uvs"] = [mesh_data["uvs"]]
                    self._scene.DefineMeshExt(
                        mesh_name, mesh_data["points"],
                        mesh_data["triangles"], **mesh_kwargs)
                ground_props = pyluxcore.Properties()
                if HDRI_GROUND_SOURCE_TOKEN in ground_text:
                    ground_text = ground_text.replace(
                        HDRI_GROUND_SOURCE_TOKEN,
                        self._active_hdri_file.replace("\\", "/"))
                ground_props.SetFromString(ground_text)
                self._scene.Parse(ground_props)
            self._config = pyluxcore.RenderConfig(props, self._scene)
            self._apply_camera_props()

            self._session = pyluxcore.RenderSession(self._config)
            self._session.Start()
            self._session_mode = "full"
            self._render_backend = "PATHOCL / OptiX"

            self._set_pipeline(0)
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

    def _start_restart_worker(self, width, height, mode, camera_snapshot,
                              hdr_file=None, hdri_gain=None,
                              render_hdri_background=None, ao_mode=None,
                              hdri_height=None, hdri_rotation=None,
                              scene_update=None):
        """Capture Tk state, then launch a renderer-only restart worker."""
        if render_hdri_background is None:
            render_hdri_background = bool(self._render_hdri_background.get())
        if ao_mode is None:
            ao_mode = bool(self._ao_mode.get())
        if hdri_height is None:
            hdri_height = float(self._hdri_height.get())
        if hdri_rotation is None:
            hdri_rotation = float(self._hdri_rotation.get())
        threading.Thread(
            target=self._do_restart_session,
            args=(width, height, mode, camera_snapshot),
            kwargs={
                "hdr_file": hdr_file,
                "hdri_gain": hdri_gain,
                "render_hdri_background": render_hdri_background,
                "ao_mode": bool(ao_mode),
                "hdri_height": hdri_height,
                "hdri_rotation": hdri_rotation,
                "scene_update": scene_update,
            },
            daemon=True).start()

    def _do_restart_session(self, width, height, mode, camera_snapshot, *,
                            hdr_file=None, hdri_gain=None,
                            render_hdri_background=None, ao_mode,
                            hdri_height=0.0, hdri_rotation=0.0,
                            scene_update=None):
        """Restart a session at the requested resolution in a background thread."""
        succeeded = False
        error_message = None
        active_hdri_file = None
        try:
            with self._restart_lock:
                try:
                    if self._render_stopped:
                        return
                    if self._session and self._session.IsStarted():
                        self._session.Stop()
                    _, _, _, _, _, _, exp, _ = camera_snapshot
                    restart_props = (pyluxcore.Properties()
                        .Set(pyluxcore.Property("film.width", [width]))
                        .Set(pyluxcore.Property("film.height", [height])))
                    effective_ao_mode = bool(ao_mode)
                    if not effective_ao_mode:
                        restart_props.SetFromString(_reinhard_tonemap(exp))
                    if (hdr_file or hdri_gain is not None
                            or render_hdri_background is not None
                            or ao_mode is not None):
                        _source_file = hdr_file if hdr_file else (
                            self._hdr_file or DEFAULT_HDRI_FILE
                        )
                        _gain = hdri_gain if hdri_gain is not None else (
                            10.0 ** self._hdri_gain_log.get())
                        active_hdri_file = _apply_environment_maps(
                            self._scene, _source_file, _gain,
                            render_hdri_background, bool(ao_mode),
                            hdri_height, hdri_rotation)
                    if scene_update:
                        (config_texts, meshes, scene_texts,
                         delete_objects) = scene_update
                        for object_name in delete_objects:
                            if object_name == AO_LAMP_NAME:
                                # Keep its generated triangle lights alive
                                # until the stopped PATHOCL session is
                                # released. The batched API moves those
                                # lights to the scene trash bin; the singular
                                # API used to destroy them immediately.
                                self._scene.DeleteObjects([object_name])
                            else:
                                try:
                                    self._scene.DeleteObject(object_name)
                                except Exception:
                                    pass  # already absent
                        for text in config_texts:
                            restart_props.SetFromString(text)
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
                            if HDRI_GROUND_SOURCE_TOKEN in text:
                                if not active_hdri_file:
                                    raise RuntimeError(
                                        "HDRI ground needs an active environment map")
                                text = text.replace(
                                    HDRI_GROUND_SOURCE_TOKEN,
                                    active_hdri_file.replace("\\", "/"))
                            extra = pyluxcore.Properties()
                            extra.SetFromString(text)
                            self._scene.Parse(extra)
                    self._apply_camera_snapshot(camera_snapshot, width, height)
                    # Scene edits may replace lights and image maps. Parse the
                    # config only after all scene updates so its light strategy
                    # references the new environment rather than the stopped
                    # session's light definitions.
                    self._config.Parse(restart_props)
                    self._session = pyluxcore.RenderSession(self._config)
                    self._session.Start()
                    self._session_mode = mode
                    succeeded = True
                except Exception as ex:
                    error_message = str(ex)
        finally:
            self._restart_results.put(
                (mode, camera_snapshot[-1], succeeded, error_message, hdr_file,
                 active_hdri_file))

    def _process_restart_results(self):
        """Consume completed background restart results on the Tk event loop."""
        try:
            while True:
                self._finish_restart(*self._restart_results.get_nowait())
        except queue.Empty:
            pass
        self.after(25, self._process_restart_results)

    def _finish_restart(self, mode, started_revision, succeeded, error_message,
                        hdr_file=None, active_hdri_file=None):
        """Apply a completed restart result and queue newer camera work when required."""
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
        if active_hdri_file:
            self._active_hdri_file = active_hdri_file
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
        """Schedule responsive preview and debounced full-resolution camera restarts."""
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
        """Start a low-resolution preview render for the latest camera snapshot."""
        self._preview_restart_id = None
        if self._render_stopped:
            return
        self._preview_restart_in_progress = True
        camera_snapshot = self._camera_snapshot
        self._start_restart_worker(
            PREVIEW_W, PREVIEW_H, "preview", camera_snapshot)

    def _restart_full_session(self):
        """Start a full-resolution render for the latest camera snapshot."""
        self._full_restart_id = None
        if self._render_stopped:
            return
        camera_snapshot = self._camera_snapshot
        self._start_restart_worker(
            self._render_width, self._render_height, "full", camera_snapshot)

    # ── Camera ───────────────────────────────────────────────────────────
    def _cam_orig(self):
        """Return the current orbit camera eye position."""
        return orbit(self._target, self._camera_distance,
                     self.az.get(), self.el.get())

    def _set_camera_distance(self, distance):
        """Store the camera distance used by orbit, snapshots, and status."""
        self._camera_distance = max(0.001, float(distance))

    def _capture_camera_snapshot(self):
        """Capture immutable camera and exposure state for an asynchronous restart."""
        return (tuple(self._target), self._camera_distance,
                self.az.get(), self.el.get(),
                tuple(self._camera_up), self._camera_fov,
                max(0.001, self.exposure.get()), self._camera_revision)

    def _set_render_resolution(self, _=None):
        """Apply the selected base viewport resolution from the UI."""
        width, height = (int(value) for value in self.render_resolution.get().split(" x "))
        self._set_base_viewport_resolution(width, height, restart=True)

    def _set_window_scale(self, _=None):
        """Apply the selected window scale and restart rendering if dimensions changed."""
        self._update_viewport_and_film(restart=True)

    def _set_final_film_resolution(self, width, height, restart):
        """Set exact renderer output dimensions without resizing the viewport."""
        self._film_resolution_override = width, height
        old_film_size = self._render_width, self._render_height
        self._apply_display_size()
        changed = (self._render_width, self._render_height) != old_film_size
        if hasattr(self, "_film_size_label"):
            self._film_size_label.config(
                text=f"{self._render_width} x {self._render_height}")
        self._save_settings()
        if not changed or not restart:
            return changed
        if self._render_stopped or not self._scene or not self._session:
            return changed
        self._camera_revision += 1
        self._camera_snapshot = self._capture_camera_snapshot()
        self._camera_restart_pending = True
        self._start_restart_worker(
            self._render_width, self._render_height, "full",
            self._camera_snapshot)
        return changed

    def _set_film_scale(self, _=None):
        """Clear any exact film override and apply the selected final-film scale."""
        self._film_resolution_override = None
        self._update_viewport_and_film(restart=True)

    def _set_base_viewport_resolution(self, width, height, restart):
        """Set the unscaled viewport baseline and update its active film."""
        self._base_viewport_width = width
        self._base_viewport_height = height
        self.render_resolution.set(f"{width} x {height}")
        return self._update_viewport_and_film(restart)

    def _update_viewport_and_film(self, restart):
        """Apply the two scale settings and optionally restart the session."""
        window_scale = _setting_scale(
            {"window_scale": self.window_scale.get()}, "window_scale")
        film_scale = _setting_scale(
            {"film_scale": self.film_scale.get()}, "film_scale")
        if window_scale != self.window_scale.get():
            self.window_scale.set(window_scale)
        if film_scale != self.film_scale.get():
            self.film_scale.set(film_scale)
        self._viewport_width, self._viewport_height = _scaled_viewport_resolution(
            self._base_viewport_width, self._base_viewport_height, window_scale)
        old_film_size = self._render_width, self._render_height
        old_display_size = self._display_w, self._display_h
        self._apply_display_size()
        self._resize_window_to_content()
        changed = ((self._render_width, self._render_height) != old_film_size
                   or (self._display_w, self._display_h) != old_display_size)
        if hasattr(self, "_film_size_label"):
            self._film_size_label.config(
                text=f"{self._render_width} x {self._render_height}")
        self._save_settings()
        if not changed or not restart:
            return changed
        if self._render_stopped or not self._scene or not self._session:
            return changed
        self._camera_revision += 1
        self._camera_snapshot = self._capture_camera_snapshot()
        self._camera_restart_pending = True
        self._start_restart_worker(
            self._render_width, self._render_height, "full",
            self._camera_snapshot)
        return changed

    def _zoom_at_cursor(self, e, viewport_w, viewport_h):
        """Zoom while preserving the focal-plane point under the viewport cursor."""
        delta = -e.delta / 120 if hasattr(e, "delta") and e.delta else (
            1 if e.num == 5 else -1)
        old_dist = self._camera_distance
        # One-unit steps near the default scene scale, proportional steps
        # for large externally-set distances.
        zoom_step = max(1.0, old_dist * 0.05)
        new_dist = max(0.001, old_dist + delta * zoom_step)
        if new_dist == old_dist:
            return

        orig = self._cam_orig()
        right, up, fwd = cam_axes(orig, self._target)
        screen_x = 2.0 * ((e.x + 0.5) / viewport_w) - 1.0
        screen_y = 1.0 - 2.0 * ((e.y + 0.5) / viewport_h)
        # Derive the vertical half-tangent from the active LuxCore fieldofview
        # so the point under the cursor stays fixed at any zoom level.
        lux_fov = self._camera_fieldofview(
            self._camera_fov, self._render_width, self._render_height)
        half_tan = math.tan(math.radians(lux_fov) * 0.5)
        frame = self._render_width / self._render_height
        tan_half_fov = half_tan / frame if frame >= 1.0 else half_tan
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

        self._set_camera_distance(new_dist)
        self._on_camera()

    def _camera_fieldofview(self, _fov, _width, _height):
        """Return the scene FOV so the rendered HDRI keeps its wide framing.

        External look-at messages retain their FOV metadata for protocol
        compatibility, but never alter the LuxCore camera projection. This
        keeps the HDRI background and scene geometry at the scene's 90-degree
        camera angle through local navigation and asynchronous restarts.
        """
        return self._scene_fieldofview

    def _apply_camera_props(self):
        """Apply the current camera state to the active LuxCore scene."""
        orig = self._cam_orig()
        self._scene.Parse(pyluxcore.Properties()
            .Set(pyluxcore.Property("scene.camera.lookat.orig",   list(orig)))
            .Set(pyluxcore.Property("scene.camera.lookat.target", list(self._target)))
            .Set(pyluxcore.Property("scene.camera.up",            list(self._camera_up)))
            .Set(pyluxcore.Property("scene.camera.fieldofview",   [
                self._camera_fieldofview(self._camera_fov,
                                         self._render_width,
                                         self._render_height)])))

    def _apply_camera_snapshot(self, camera_snapshot, width, height):
        """Apply a captured camera state to a scene using the given film dimensions."""
        target, dist, az, el, up, fov, _, _ = camera_snapshot
        orig = orbit(target, dist, az, el)
        self._scene.Parse(pyluxcore.Properties()
            .Set(pyluxcore.Property("scene.camera.lookat.orig",   list(orig)))
            .Set(pyluxcore.Property("scene.camera.lookat.target", list(target)))
            .Set(pyluxcore.Property("scene.camera.up",            list(up)))
            .Set(pyluxcore.Property("scene.camera.fieldofview",   [
                self._camera_fieldofview(fov, width, height)])))

    def _clear_display(self):
        """Immediately paint the render canvas black to prevent ghosting."""
        black = bytes(self._display_w * self._display_h * 4)  # fast zero-fill
        img = Image.frombuffer("RGBA", (self._display_w, self._display_h),
                               black, "raw", "RGBA", 0, 1)
        self._tk_image = ImageTk.PhotoImage(img)
        self._render_canvas.itemconfigure(self._canvas_img_id, image=self._tk_image)

    def _on_camera(self, refresh_ui=True):
        """Persist a camera change and queue preview plus full-resolution replacements."""
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
        """Persist an exposure change and restart the active render session."""
        self._update_info()
        self._save_settings()
        if self._scene and self._session and not self._render_stopped:
            self._camera_revision += 1
            self._camera_snapshot = self._capture_camera_snapshot()
            self._camera_restart_pending = True
            self._start_restart_worker(
                self._render_width, self._render_height, "full",
                self._camera_snapshot)

    def _update_ground_color_button(self):
        """Synchronize the ground-color button swatch with its persisted color."""
        color = self._ground_color.get()
        self._ground_color_button.config(
            bg=color, activebackground=color,
            fg="white" if sum(_hex_color_to_rgb(color)) < 1.5 else "black")

    def _choose_ground_color(self):
        """Prompt for a ground color and apply it when the user confirms."""
        _, color = colorchooser.askcolor(
            color=self._ground_color.get(), parent=self,
            title="Choose Ground Color")
        if color:
            self._ground_color.set(color.lower())
            self._update_ground_color_button()
            self._on_ground_appearance_changed()

    def _on_ground_appearance_changed(self):
        """Persist and apply the active ground mode's color/reflectivity."""
        self._save_settings()
        if not (self._hdri_ground.get() and self._scene and self._session
                and not self._render_stopped and not self._ao_mode.get()):
            return
        ground_meshes, ground_text = self._hdri_ground_update()
        deletions = ([] if self._render_hdri_background.get()
                     else [GROUND_MIRROR_NAME])
        self._camera_revision += 1
        self._camera_snapshot = self._capture_camera_snapshot()
        self._camera_restart_pending = True
        self._start_restart_worker(
            self._render_width, self._render_height, "full",
            self._camera_snapshot,
            scene_update=([], ground_meshes, [ground_text], deletions))

    def _on_hdri_gain(self):
        """Update HDRI gain UI and restart the render with the adjusted environment intensity."""
        self._update_hdri_gain_label()
        self._update_info()
        self._save_settings()
        if self._scene and self._session and not self._render_stopped:
            gain = 10.0 ** self._hdri_gain_log.get()
            self._camera_revision += 1
            self._camera_snapshot = self._capture_camera_snapshot()
            self._camera_restart_pending = True
            self._start_restart_worker(
                self._render_width, self._render_height, "full",
                self._camera_snapshot, hdri_gain=gain,
                render_hdri_background=self._render_hdri_background.get(),
                ao_mode=self._ao_mode.get())

    def _adjust_hdri_gain(self, value):
        """Apply one relative gain adjustment, then return the jog to center."""
        adjustment = float(value)
        if self._hdri_gain_jog_in_progress or abs(adjustment) < 1e-9:
            return
        old_log_gain = self._hdri_gain_log.get()
        new_log_gain = max(-2.0, min(3.0, old_log_gain + adjustment * 0.1))
        self._hdri_gain_jog_in_progress = True
        self._hdri_gain_jog.set(0.0)
        self._hdri_gain_jog_in_progress = False
        if new_log_gain == old_log_gain:
            return
        self._hdri_gain_log.set(new_log_gain)
        self._on_hdri_gain()

    def _update_hdri_gain_label(self):
        """Display the current linear HDRI gain beside its relative-adjustment control."""
        if hasattr(self, "_hdri_gain_value_label"):
            gain = 10.0 ** self._hdri_gain_log.get()
            self._hdri_gain_value_label.config(text=f"{gain:.4g}")

    def _update_hdri_alignment_labels(self):
        """Show the current HDRI vertical and horizontal alignment in degrees."""
        if hasattr(self, "_hdri_height_value_label"):
            self._hdri_height_value_label.config(
                text=f"{self._hdri_height.get():+.1f}°")
        if hasattr(self, "_hdri_rotation_value_label"):
            self._hdri_rotation_value_label.config(
                text=f"{self._hdri_rotation.get():+.1f}°")

    def _on_hdri_alignment(self, _=None):
        """Persist HDRI alignment and restart with matching lighting and ground UVs."""
        self._update_hdri_alignment_labels()
        self._update_info()
        self._save_settings()
        if not (self._scene and self._session and not self._render_stopped):
            return
        # AO uses its own neutral domes. The alignment takes effect once HDRI
        # rendering resumes, without needlessly restarting the clay stage.
        if self._ao_mode.get():
            return
        scene_update = None
        if self._hdri_ground.get():
            ground_meshes, ground_text = self._hdri_ground_update()
            deletions = ([] if self._render_hdri_background.get()
                         else [GROUND_MIRROR_NAME])
            scene_update = ([], ground_meshes, [ground_text], deletions)
        self._camera_revision += 1
        self._camera_snapshot = self._capture_camera_snapshot()
        self._camera_restart_pending = True
        self._start_restart_worker(
            self._render_width, self._render_height, "full",
            self._camera_snapshot,
            hdri_gain=10.0 ** self._hdri_gain_log.get(),
            render_hdri_background=self._render_hdri_background.get(),
            ao_mode=False,
            hdri_height=self._hdri_height.get(),
            hdri_rotation=self._hdri_rotation.get(),
            scene_update=scene_update)
    def _on_render_hdri_background_changed(self, *_):
        """Rebuild the environment and ground when HDRI background visibility changes."""
        self._update_info()
        self._save_settings()
        if self._scene and self._session and not self._render_stopped:
            gain = 10.0 ** self._hdri_gain_log.get()
            scene_update = None
            if self._hdri_ground.get() and not self._ao_mode.get():
                # The ground switches between catcher+mirror (HDRI bg on)
                # and a plain gray glossy2 floor (HDRI bg off). Drop the
                # stale mirror object whenever we switch to the gray floor.
                ground_meshes, ground_text = self._hdri_ground_update()
                deletions = ([] if self._render_hdri_background.get()
                             else [GROUND_MIRROR_NAME])
                scene_update = ([], ground_meshes, [ground_text], deletions)
            self._camera_revision += 1
            self._camera_snapshot = self._capture_camera_snapshot()
            self._camera_restart_pending = True
            self._start_restart_worker(
                self._render_width, self._render_height, "full",
                self._camera_snapshot, hdri_gain=gain,
                render_hdri_background=self._render_hdri_background.get(),
                ao_mode=self._ao_mode.get(), scene_update=scene_update)

    def _tracked_scene_bounds(self):
        """Union of streamed mesh bounds, or a stage-sized box at the target."""
        if self._mesh_bounds:
            mins = [min(bounds[0][axis] for bounds in self._mesh_bounds.values())
                    for axis in range(3)]
            maxs = [max(bounds[1][axis] for bounds in self._mesh_bounds.values())
                    for axis in range(3)]
        else:
            mins = [value - 5.0 for value in self._target]
            maxs = [value + 5.0 for value in self._target]
        return mins, maxs

    def _hdri_ground_update(self, source_file=None):
        """Meshes and property text for the ground plane.

        With AO clay mode active: a single plain white matte disk at Z = 0
        that joins the clay look and receives the overhead lamp's
        occlusion shadows.

        With the HDRI background rendered: shadow-catcher disk just above
        Z = 0 plus an archglass mirror at Z = 0 (see block comment above
        GROUND_NAME for details).

        With the HDRI background off: a single plain gray glossy2 disk at
        Z = 0 -- real geometry lit by the HDRI lighting dome, no catcher,
        mirror, or added light source -- that receives shadows and reflects
        the meshes against the white backdrop.
        """
        try:
            import numpy
        except ImportError as ex:
            raise RuntimeError(
                "The HDRI ground plane requires numpy: "
                "python -m pip install numpy") from ex
        mins, maxs = self._tracked_scene_bounds()
        extent = max(maxs[0] - mins[0], maxs[1] - mins[1], maxs[2] - mins[2],
                     0.001)
        center_x = 0.5 * (mins[0] + maxs[0])
        center_y = 0.5 * (mins[1] + maxs[1])
        ground_z = 0.0  # the stage floor is always the world Z = 0 plane
        # Size the disk to always cover the camera's view at Z=0. Compute
        # where the view ray hits the ground plane and how wide the view
        # frustum is at that distance, then use a generous safety margin.
        camera_eye = self._cam_orig()
        camera_height = max(camera_eye[2], 0.001)  # distance above Z=0
        lux_fov = self._camera_fieldofview(
            self._camera_fov, self._render_width, self._render_height)
        half_fov_rad = math.radians(lux_fov) * 0.5
        # The view frustum half-width at the ground plane
        frustum_half_width = camera_height * math.tan(half_fov_rad)
        # Account for the aspect ratio: use the larger dimension
        aspect = self._render_width / max(self._render_height, 1)
        if aspect < 1.0:
            frustum_half_width /= aspect
        # Distance from camera to where the view center hits Z=0
        view_dir = _norm([self._target[i] - camera_eye[i] for i in range(3)])
        if abs(view_dir[2]) > 1e-6:
            # Ray-plane intersection: t = -eye_z / dir_z
            t = -camera_eye[2] / view_dir[2]
            if t > 0:
                hit_x = camera_eye[0] + t * view_dir[0]
                hit_y = camera_eye[1] + t * view_dir[1]
                # Center the disk on the view hit point, but blend with
                # the mesh bounds center to keep it stable
                center_x = 0.5 * (center_x + hit_x)
                center_y = 0.5 * (center_y + hit_y)
        # Radius must cover the frustum plus distance from hit point to
        # disk center, with a 3× safety margin, then enlarged by
        # GROUND_RADIUS_SCALE so the rim stays well out of frame.
        scene_radius = GROUND_RADIUS_FACTOR * extent
        view_radius = (frustum_half_width + camera_height) * 3.0
        disk_radius = GROUND_RADIUS_SCALE * max(scene_radius, view_radius)
        points = []
        uvs = []
        hdri_transform = _hdri_yaw_matrix(self._hdri_rotation.get())
        # Center fan: the nadir maps to the bottom image row. Each segment
        # gets its own center vertex so it continues to interpolate cleanly
        # per wedge after HDRI yaw alignment.
        for j in range(GROUND_SEGMENTS):
            points.append((center_x, center_y, ground_z))
            uvs.append(_hdri_ground_uv(
                2.0 * math.pi * (j + 0.5) / GROUND_SEGMENTS,
                1e-5, hdri_transform))
        # Rings: a hemisphere point at angle alpha from the nadir projects
        # straight up to radius disk_radius * sin(alpha). Its direction is
        # transformed through the same HDRI yaw as the lighting and background
        # map before it is converted to LuxCore lat-long UVs. The texture file
        # itself has already received the matching vertical pixel offset.
        for i in range(GROUND_RINGS):
            alpha = (math.pi / 2.0) * (i + 1) / GROUND_RINGS
            radius = disk_radius * math.sin(alpha)
            previous_u = None
            for j in range(GROUND_SEGMENTS + 1):
                phi = 2.0 * math.pi * j / GROUND_SEGMENTS
                points.append((center_x + radius * math.cos(phi),
                               center_y + radius * math.sin(phi),
                               ground_z))
                u, v = _hdri_ground_uv(phi, alpha, hdri_transform)
                if previous_u is not None:
                    # Keep interpolation continuous across the lat-long seam.
                    # The duplicate last vertex naturally becomes first_u ± 1.
                    while u - previous_u > 0.5:
                        u -= 1.0
                    while u - previous_u < -0.5:
                        u += 1.0
                uvs.append((u, v))
                previous_u = u
        triangles = []
        ring_start = GROUND_SEGMENTS
        for j in range(GROUND_SEGMENTS):
            triangles.append((j, ring_start + j, ring_start + j + 1))
        for i in range(GROUND_RINGS - 1):
            row = ring_start + i * (GROUND_SEGMENTS + 1)
            next_row = row + GROUND_SEGMENTS + 1
            for j in range(GROUND_SEGMENTS):
                a, b = row + j, next_row + j
                triangles.append((a, b, b + 1))
                triangles.append((a, b + 1, a + 1))
        base_points = numpy.array(points, dtype=numpy.float32)
        triangles_array = numpy.array(triangles, dtype=numpy.uint32)
        normals_array = numpy.tile(
            numpy.array([0.0, 0.0, 1.0], dtype=numpy.float32),
            (len(points), 1))

        if self._ao_mode.get():
            # AO clay mode: a plain white matte floor joins the clay stage
            # and receives the overhead lamp's occlusion shadows. No
            # catcher or mirror layers.
            meshes = {GROUND_MESH_NAME: {
                "points": base_points, "triangles": triangles_array,
                "normals": normals_array, "uvs": None,
            }}
            scene_text = "\n".join((
                f"scene.materials.{GROUND_NAME}_mat.type = matte",
                f"scene.materials.{GROUND_NAME}_mat.kd = 1.0 1.0 1.0",
                f"scene.objects.{GROUND_NAME}.shape = {GROUND_MESH_NAME}",
                f"scene.objects.{GROUND_NAME}.material = {GROUND_NAME}_mat"))
            return meshes, scene_text

        if not self._render_hdri_background.get():
            # White-backdrop mode: a plain gray glossy2 floor. Real geometry
            # lit by the HDRI lighting dome -- no catcher, mirror, or light
            # source -- so it receives shadows and shows glossy reflections.
            color = _hex_color_to_rgb(self._ground_color.get())
            reflectivity = self._ground_reflectivity.get()
            roughness = (GROUND_GRAY_MIRROR_ROUGHNESS
                         + (GROUND_GRAY_ROUGHNESS
                            - GROUND_GRAY_MIRROR_ROUGHNESS)
                         * (1.0 - reflectivity))
            meshes = {GROUND_MESH_NAME: {
                "points": base_points, "triangles": triangles_array,
                "normals": normals_array, "uvs": None,
            }}
            # LuxCore consumes this material color in reversed channel order
            # (blue, green, red), so emit the picked RGB swatch as B G R to
            # make the rendered floor match the chosen ground color.
            scene_text = "\n".join((
                f"scene.materials.{GROUND_NAME}_mat.type = glossy2",
                f"scene.materials.{GROUND_NAME}_mat.kd = "
                f"{color[2]} {color[1]} {color[0]}",
                f"scene.materials.{GROUND_NAME}_mat.ks = "
                f"{reflectivity} {reflectivity} {reflectivity}",
                f"scene.materials.{GROUND_NAME}_mat.uroughness = "
                f"{roughness}",
                f"scene.materials.{GROUND_NAME}_mat.vroughness = "
                f"{roughness}",
                f"scene.materials.{GROUND_NAME}_mat.transparency.back = "
                "0.0 0.0 0.0",
                f"scene.objects.{GROUND_NAME}.shape = {GROUND_MESH_NAME}",
                f"scene.objects.{GROUND_NAME}.material = {GROUND_NAME}_mat"))
            return meshes, scene_text

        catcher_points = base_points.copy()
        # The catcher floats a hair above the mirror: far enough apart for
        # float precision at CAD scales, visually coincident.
        catcher_points[:, 2] += GROUND_CATCHER_LIFT * extent
        meshes = {
            GROUND_MESH_NAME: {
                "points": catcher_points, "triangles": triangles_array,
                "normals": normals_array,
                "uvs": numpy.array(uvs, dtype=numpy.float32),
            },
            GROUND_MIRROR_MESH_NAME: {
                "points": base_points, "triangles": triangles_array,
                "normals": normals_array, "uvs": None,
            },
        }
        source = source_file or self._hdr_file or DEFAULT_HDRI_FILE
        kr = self._ground_reflectivity.get()
        # Archglass reflection is Fresnel-limited. Raise its IOR toward a
        # near-perfect reflector only near the slider's right endpoint, while
        # retaining the original transparent ground at ordinary strengths.
        mirror_ior = 2.0 + (GROUND_MIRROR_IOR - 2.0) * kr ** 6
        scene_text = "\n".join((
            f"scene.textures.{GROUND_NAME}_tex.type = imagemap",
            f'scene.textures.{GROUND_NAME}_tex.file = "'
            + HDRI_GROUND_SOURCE_TOKEN + '"',
            f"scene.textures.{GROUND_NAME}_tex.colorspace = nop",
            f"scene.textures.{GROUND_NAME}_tex.storage = "
            + _environment_storage(source),
            f"scene.textures.{GROUND_NAME}_tex.resizepolicy.enable = 0",
            # The clamped copy keeps HDR highlights out of the shadow
            # catcher's diffuse albedo.
            f"scene.textures.{GROUND_NAME}_clamp.type = clamp",
            f"scene.textures.{GROUND_NAME}_clamp.texture = {GROUND_NAME}_tex",
            f"scene.textures.{GROUND_NAME}_clamp.min = 0.0",
            f"scene.textures.{GROUND_NAME}_clamp.max = 1.0",
            # Shadow catcher: transparent wherever the environment is
            # unoccluded; where meshes block the light it shades with the
            # projected texture, so shadows read as darkened ground.
            # Note LuxCore's transparency.front/back value is an opacity:
            # rays pass when passThroughEvent > value, so 0.0 means fully
            # transparent. The transparent back face lets the mirror's
            # reflection rays reach the meshes and keeps the disk invisible
            # from below.
            f"scene.materials.{GROUND_NAME}_mat.type = matte",
            f"scene.materials.{GROUND_NAME}_mat.kd = {GROUND_NAME}_clamp",
            f"scene.materials.{GROUND_NAME}_mat.shadowcatcher.enable = 1",
            f"scene.materials.{GROUND_NAME}_mat"
            ".shadowcatcher.onlyinfinitelights = 1",
            f"scene.materials.{GROUND_NAME}_mat.transparency.back = 0.0 0.0 0.0",
            f"scene.objects.{GROUND_NAME}.shape = {GROUND_MESH_NAME}",
            f"scene.objects.{GROUND_NAME}.material = {GROUND_NAME}_mat",
            # Archglass mirror under the catcher: Fresnel-weighted specular
            # reflection layers the meshes over the transmitted background
            # (full transmission for camera rays, so no brightness seam).
            # Archglass ignores per-side transparency; its own Fresnel
            # pass-through governs both faces. The interior IOR is REQUIRED:
            # without it both IORs default to 1.0 and the Fresnel term -- and
            # therefore the reflection -- is identically zero.
            f"scene.materials.{GROUND_MIRROR_NAME}_mat.type = archglass",
            f"scene.materials.{GROUND_MIRROR_NAME}_mat.kr = "
            f"{kr} {kr} {kr}",
            f"scene.materials.{GROUND_MIRROR_NAME}_mat.kt = 1.0 1.0 1.0",
            f"scene.materials.{GROUND_MIRROR_NAME}_mat.exteriorior = 1.0",
            f"scene.materials.{GROUND_MIRROR_NAME}_mat.interiorior = "
            f"{mirror_ior}",
            f"scene.objects.{GROUND_MIRROR_NAME}.shape = "
            f"{GROUND_MIRROR_MESH_NAME}",
            f"scene.objects.{GROUND_MIRROR_NAME}.material = "
            f"{GROUND_MIRROR_NAME}_mat"))
        return meshes, scene_text

    def _ao_lamp_scene_text(self):
        """A flat, camera-invisible area light above the tracked geometry.

        The quad matches the XY bounding box of the streamed mesh bounds
        (falling back to a stage-sized box around the orbit target) and
        floats directly over it, its normal facing down like a softbox.
        """
        mins, maxs = self._tracked_scene_bounds()
        extent = max(maxs[0] - mins[0], maxs[1] - mins[1], maxs[2] - mins[2],
                     0.001)
        center_x = 0.5 * (mins[0] + maxs[0])
        center_y = 0.5 * (mins[1] + maxs[1])
        half_x = max(0.5 * (maxs[0] - mins[0]),
                     AO_LAMP_MIN_HALF_EXTENT * extent)
        half_y = max(0.5 * (maxs[1] - mins[1]),
                     AO_LAMP_MIN_HALF_EXTENT * extent)
        height = AO_LAMP_HEIGHT_FACTOR * extent
        z = maxs[2] + height
        emission = AO_LAMP_EMISSION
        corners = ((center_x - half_x, center_y - half_y),
                   (center_x - half_x, center_y + half_y),
                   (center_x + half_x, center_y + half_y),
                   (center_x + half_x, center_y - half_y))
        vertices = " ".join(f"{x:.6g} {y:.6g} {z:.6g}" for x, y in corners)
        return "\n".join((
            f"scene.materials.{AO_LAMP_NAME}_mat.type = matte",
            f"scene.materials.{AO_LAMP_NAME}_mat.kd = 0.0 0.0 0.0",
            f"scene.materials.{AO_LAMP_NAME}_mat.emission = "
            f"{emission} {emission} {emission}",
            # Keep the lamp as a direct light while hiding the visible
            # softbox from a glossy or mirror reflection.
            f"scene.materials.{AO_LAMP_NAME}_mat"
            ".visibility.indirect.glossy.enable = 0",
            f"scene.materials.{AO_LAMP_NAME}_mat"
            ".visibility.indirect.specular.enable = 0",
            f"scene.objects.{AO_LAMP_NAME}.material = {AO_LAMP_NAME}_mat",
            f"scene.objects.{AO_LAMP_NAME}.camerainvisible = 1",
            f"scene.objects.{AO_LAMP_NAME}.vertices = {vertices}",
            f"scene.objects.{AO_LAMP_NAME}.faces = 0 1 2 0 2 3"))

    def _on_ao_mode_changed(self, *_):
        """Toggle the ambient-occlusion clay look with one render restart."""
        self._save_settings()
        if not (self._scene and self._session and not self._render_stopped):
            return
        ao = self._ao_mode.get()
        scene_lines = [AO_WHITE_MATERIAL] if ao else []
        for object_name, (shape_name, material_name) in (
                self._uploaded_objects.items()):
            scene_lines.append(
                f"scene.objects.{object_name}.shape = {shape_name}")
            scene_lines.append(
                f"scene.objects.{object_name}.material = "
                f"{'ao_white' if ao else material_name}")
        meshes = {}
        if ao:
            # The clay stage keeps the ground plane as a white matte floor
            # so it participates in the AO look; only the reflective
            # mirror layer is dropped with the environment.
            deletions = []
            if self._hdri_ground.get():
                deletions = [GROUND_MIRROR_NAME]
                ground_meshes, ground_text = self._hdri_ground_update()
                meshes.update(ground_meshes)
                scene_lines.append(ground_text)
            scene_lines.append(self._ao_lamp_scene_text())
            config_text = AO_PATH_DEPTHS + "\n" + AO_TONEMAP
        else:
            deletions = [AO_LAMP_NAME]
            if self._hdri_ground.get():
                ground_meshes, ground_text = self._hdri_ground_update()
                meshes.update(ground_meshes)
                scene_lines.append(ground_text)
            config_text = (DEFAULT_PATH_DEPTHS + "\n"
                           + _reinhard_tonemap(max(0.001, self.exposure.get())))
        update = ([config_text], meshes,
                  ["\n".join(scene_lines)] if scene_lines else [],
                  deletions)
        gain = 10.0 ** self._hdri_gain_log.get()
        self._camera_revision += 1
        self._camera_snapshot = self._capture_camera_snapshot()
        self._camera_restart_pending = True
        self._start_restart_worker(
            self._render_width, self._render_height, "full",
            self._camera_snapshot, hdri_gain=gain,
            render_hdri_background=self._render_hdri_background.get(),
            ao_mode=ao, scene_update=update)

    def _on_hdri_ground_changed(self, *_):
        """Toggle the HDRI-textured ground plane with one render restart."""
        self._save_settings()
        if not (self._scene and self._session and not self._render_stopped):
            return
        if self._hdri_ground.get():
            ground_meshes, ground_text = self._hdri_ground_update()
            # The mirror layer exists only for the non-AO HDRI background
            # stage; drop it in the other modes.
            deletions = ([] if (self._render_hdri_background.get()
                                and not self._ao_mode.get())
                         else [GROUND_MIRROR_NAME])
            update = ([], ground_meshes, [ground_text], deletions)
        else:
            update = ([], {}, [], [GROUND_NAME, GROUND_MIRROR_NAME])
        self._camera_revision += 1
        self._camera_snapshot = self._capture_camera_snapshot()
        self._camera_restart_pending = True
        self._start_restart_worker(
            self._render_width, self._render_height, "full",
            self._camera_snapshot, scene_update=update)

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
        """Accept localhost control connections and assign each to a worker thread."""
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
        """Execute queued protocol commands on the Tk thread and return their replies."""
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
        """Execute one normalized control command.

                Args:
                    header: Decoded command header containing command-specific fields.
                    blobs: Binary payloads keyed by their declared roles.

                Returns:
                    A JSON-serializable protocol reply.

                Raises:
                    ValueError: If the command or its arguments are invalid."""
        cmd = _header_command(header)
        if cmd == "status":
            return self._control_status()
        if cmd == "camera":
            if "az" in header:
                self.az.set(float(header["az"]))
            if "el" in header:
                self.el.set(max(-89.0, min(89.0, float(header["el"]))))
            if "dist" in header:
                self._set_camera_distance(float(header["dist"]))
            # Orbit-parameter commands are turntable-style: level the horizon.
            self._camera_up = [0.0, 0.0, 1.0]
            self._on_camera()
            return {"ok": True, "azimuth": self.az.get(),
                    "elevation": self.el.get(),
                    "distance": self._camera_distance}
        if cmd in ("lookat", "cameraeyetarget"):
            return self._apply_lookat(header)
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
        if cmd == "hdri_alignment":
            height = max(-45.0, min(
                45.0, float(header.get("height", self._hdri_height.get()))))
            rotation = max(-180.0, min(
                180.0, float(header.get("rotation", self._hdri_rotation.get()))))
            changed = (height != self._hdri_height.get()
                       or rotation != self._hdri_rotation.get())
            self._hdri_height.set(height)
            self._hdri_rotation.set(rotation)
            if changed:
                self._on_hdri_alignment()
            return {"ok": True, "hdri_height": height,
                    "hdri_rotation": rotation}
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
        if cmd == "ao":
            enabled = bool(header.get("enabled", True))
            if enabled != self._ao_mode.get():
                self._ao_mode.set(enabled)  # trace restarts render
            return {"ok": True, "ao_mode": enabled}
        if cmd == "ground":
            enabled = bool(header.get("enabled", True))
            if enabled != self._hdri_ground.get():
                self._hdri_ground.set(enabled)  # trace restarts render
            return {"ok": True, "hdri_ground": enabled}
        if cmd == "resolution":
            width, height = int(header["width"]), int(header["height"])
            if not (RENDER_MIN_DIMENSION <= width <= RENDER_MAX_DIMENSION
                    and RENDER_MIN_DIMENSION <= height <= RENDER_MAX_DIMENSION):
                raise ValueError("resolution must be between 16 and 8192 pixels")
            self._set_final_film_resolution(width, height, restart=True)
            return {"ok": True, "width": self._render_width,
                    "height": self._render_height,
                    "viewport_width": self._display_w,
                    "viewport_height": self._display_h,
                    "base_width": self._base_viewport_width,
                    "base_height": self._base_viewport_height,
                    "window_scale": self.window_scale.get(),
                    "film_scale": self.film_scale.get()}
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

    def _apply_lookat(self, header):
        """Set the view from eye/target/up/fov (the C# CameraUpdate header)."""
        def vector3(name, required):
            """Return a validated three-component vector from a lookat command field."""
            value = header.get(name)
            if not isinstance(value, (list, tuple)) or len(value) != 3:
                if required:
                    raise ValueError(f"lookat needs {name}: [x, y, z]")
                return None
            return [float(component) for component in value]

        eye = vector3("eye", True)
        target = vector3("target", True)
        delta = [eye[i] - target[i] for i in range(3)]
        distance = math.sqrt(sum(component * component for component in delta))
        if distance < 1e-6:
            raise ValueError("lookat eye and target must not coincide")
        # Intentional: use half the eye-to-target distance (and derive the
        # elevation from it) to match the sender's viewport framing.
        elevation = math.degrees(
            math.asin(max(-1.0, min(1.0, delta[2] / distance))))
        elevation = max(-89.0, min(89.0, elevation))
        azimuth = math.degrees(math.atan2(delta[0], delta[1]))

        # Zero-length up vectors (a default-constructed C# array) keep the
        # current up; non-positive fov values (orthographic senders) keep
        # the current field of view.
        up = vector3("up", False)
        if up is not None:
            up_length = math.sqrt(sum(component * component for component in up))
            if up_length > 1e-6:
                self._camera_up = [component / up_length for component in up]
        fov = header.get("fov")
        if fov is not None:
            fov = float(fov)
            if fov > 0.0:
                if not 0.5 <= fov <= 179.0:
                    raise ValueError("fov must be between 0.5 and 179 degrees")
                axis = str(header.get("axis", "vertical")).lower()
                if axis not in ("vertical", "horizontal"):
                    raise ValueError("axis must be vertical or horizontal")
                self._camera_fov = (fov, axis)

        # Optional output size: render an exact film while retaining the
        # current viewport and window scale.
        # Non-positive values are ignored, like fov for orthographic senders.
        width = header.get("width")
        height = header.get("height")
        if width is not None and height is not None:
            width, height = int(width), int(height)
            if width > 0 and height > 0:
                if not (RENDER_MIN_DIMENSION <= width <= RENDER_MAX_DIMENSION
                        and RENDER_MIN_DIMENSION <= height <= RENDER_MAX_DIMENSION):
                    raise ValueError(
                        "width and height must be between 16 and 8192 pixels")
                if (width, height) != (
                        self._render_width, self._render_height):
                    self._set_final_film_resolution(width, height, restart=False)

        self._target = list(target)
        distance = distance / 2.0;
        self._set_camera_distance(distance)
        self.az.set(azimuth)
        self.el.set(elevation)
        self._on_camera()
        return {"ok": True, "azimuth": azimuth, "elevation": elevation,
                "distance": distance, "target": list(target),
                "up": list(self._camera_up),
                "fov": self._camera_fov[0] if self._camera_fov else None,
                "fov_axis": self._camera_fov[1] if self._camera_fov else None,
                "width": self._render_width, "height": self._render_height,
                "viewport_width": self._display_w,
                "viewport_height": self._display_h,
                "base_width": self._base_viewport_width,
                "base_height": self._base_viewport_height,
                "window_scale": self.window_scale.get(),
                "film_scale": self.film_scale.get()}

    def _control_status(self):
        """Return current controller, viewport, renderer, and staged-update status."""
        status = {
            "ok": True,
            "azimuth": self.az.get(),
            "elevation": self.el.get(),
            "distance": self._camera_distance,
            "target": list(self._target),
            "eye": self._cam_orig(),
            "up": list(self._camera_up),
            "fov": self._camera_fov[0] if self._camera_fov else None,
            "fov_axis": self._camera_fov[1] if self._camera_fov else None,
            "exposure": self.exposure.get(),
            "hdri_gain": 10.0 ** self._hdri_gain_log.get(),
            "hdri_height": self._hdri_height.get(),
            "hdri_rotation": self._hdri_rotation.get(),
            "render_hdri_background": bool(self._render_hdri_background.get()),
            "ao_mode": bool(self._ao_mode.get()),
            "hdri_ground": bool(self._hdri_ground.get()),
            "hdr_file": self._hdr_file or DEFAULT_HDRI_FILE,
            "active_hdr_file": self._active_hdri_file,
            "width": self._render_width,
            "height": self._render_height,
            "viewport_width": self._display_w,
            "viewport_height": self._display_h,
            "base_width": self._base_viewport_width,
            "base_height": self._base_viewport_height,
            "window_scale": self.window_scale.get(),
            "film_scale": self.film_scale.get(),
            "final_film_resolution": self._film_resolution_override,
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
            """Decode and validate one typed mesh buffer declared by the control protocol."""
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
        self._mesh_bounds[name] = (points.min(axis=0).tolist(),
                                   points.max(axis=0).tolist())
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
            """Infer the component count declared by a C# mesh-header section."""
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
        normal_data = blobs.get("normals");
        uv_data = None;
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
        if normal_data:
            if len(normal_data) % 12:
                raise ValueError("The normals buffer must hold N x 3 float32 values")
            normals = numpy.frombuffer(
                normal_data, dtype=numpy.float32).reshape(-1, 3)
            if len(normals) != len(points):
                raise ValueError("normals must provide one entry per vertex")

        uvs = None
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
        self._mesh_bounds[name] = (points.min(axis=0).tolist(),
                                   points.max(axis=0).tolist())
        create_object = bool(header.get(
            "CreateObject", header.get("create_object", True)))
        if create_object:
            bound_material = f"{name}_mat"
            object_lines = [
                f"scene.materials.{name}_mat.type = glossy2",
                f"scene.materials.{name}_mat.kd = 0.1 0.1 0.5",
            ]
            if self._ao_mode.get():
                # AO clay mode is active: bind to the shared white matte.
                object_lines.append(AO_WHITE_MATERIAL)
                bound_material = "ao_white"
            object_lines.append(f"scene.objects.{name}.shape = {name}")
            object_lines.append(
                f"scene.objects.{name}.material = {bound_material}")
            self._pending_scene_props.append("\n".join(object_lines))
            self._uploaded_objects[name] = (name, f"{name}_mat")
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
        """Apply queued uploaded meshes after the upload debounce interval."""
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
        scene_texts = list(self._pending_scene_props)
        meshes = dict(self._pending_meshes)
        if self._ao_mode.get():
            # Newly streamed geometry changes the scene bounds: rebuild the
            # AO lamp so it still hovers above everything.
            scene_texts.append(self._ao_lamp_scene_text())
        if self._hdri_ground.get():
            # Likewise, keep the ground plane under the new scene bounds.
            ground_meshes, ground_text = self._hdri_ground_update()
            meshes.update(ground_meshes)
            scene_texts.append(ground_text)
        update = (list(self._pending_config_props),
                  meshes,
                  scene_texts,
                  [])
        self._pending_config_props = []
        self._pending_meshes = {}
        self._pending_scene_props = []
        self._camera_revision += 1
        self._camera_snapshot = self._capture_camera_snapshot()
        self._camera_restart_pending = True
        self._start_restart_worker(
            self._render_width, self._render_height, "full",
            self._camera_snapshot, scene_update=update)
        return {"ok": True,
                "meshes": sorted(update[1]),
                "scene_prop_blocks": len(update[2]),
                "config_prop_blocks": len(update[0])}

    # ── Film display ──────────────────────────────────────────────────────────
    def _apply_display_size(self):
        """Size the canvas from the viewport, then derive the final film.

        The window is not resizable by the user, so changing the canvas size
        resizes the whole window around it.
        """
        max_width = max(320, self.winfo_screenwidth() - CONTROL_W - 80)
        max_height = max(240, self.winfo_screenheight() - 120)
        scale = min(1.0, max_width / self._viewport_width,
                    max_height / self._viewport_height)
        self._display_w = max(1, int(round(self._viewport_width * scale)))
        self._display_h = max(1, int(round(self._viewport_height * scale)))
        self._render_width, self._render_height = (
            self._film_resolution_override or _scaled_film_resolution(
                self._display_w, self._display_h, self.film_scale.get()))
        self._render_canvas.config(width=self._display_w,
                                   height=self._display_h)
    def _resize_window_to_content(self):
        """Apply the current content request to the fixed-size top-level."""
        self.update_idletasks()
        control_height = (self._display_h
                          + self._render_footer_frame.winfo_reqheight())
        self._control_viewport.config(height=max(1, control_height))
        self.update_idletasks()
        width = self._main_frame.winfo_reqwidth()
        height = self._main_frame.winfo_reqheight()
        if (width, height) != (self.winfo_width(), self.winfo_height()):
            # Supplying only the size preserves the current virtual-desktop
            # position, including negative coordinates on a left monitor.
            self.geometry(f"{width}x{height}")

    def _update_control_panel_scrollregion(self, _=None):
        """Keep the left control panel accessible inside its viewport."""
        self._control_viewport.configure(
            scrollregion=(0, 0, CONTROL_W, self._control_panel.winfo_reqheight()))

    def _fit_image_to_viewport(self, img):
        """Resize a rendered image to fit the active display canvas without cropping."""
        scale = min(self._display_w / img.width, self._display_h / img.height)
        width = max(1, round(img.width * scale))
        height = max(1, round(img.height * scale))
        if (width, height) == img.size:
            return img
        resample = Image.Resampling.LANCZOS if scale < 1.0 else Image.Resampling.BILINEAR
        return img.resize((width, height), resample)

    def _capture_film_frame(self, fit_for_viewport=True):
        """Copy a stable renderer film without waiting for session replacement."""
        if not self._restart_lock.acquire(blocking=False):
            return None
        try:
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
        finally:
            self._restart_lock.release()

    def _update_film(self):
        """Refresh the displayed Tk image and render statistics on a timed interval."""
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
        if self._session_mode == "preview":
            refresh_ms = PREVIEW_REFRESH_MS
        elif self.pipeline == 1:
            # Each refresh re-runs the OIDN pipeline over the whole film;
            # give the CPU denoiser room to breathe between updates.
            refresh_ms = OIDN_REFRESH_MS
        else:
            refresh_ms = REFRESH_MS
        self.after(refresh_ms, self._update_film)

    # ── Pipeline ──────────────────────────────────────────────────────────────
    def _set_pipeline(self, idx, cancel_auto=True):
        """Select a raw or OIDN image pipeline and optionally cancel automatic switching."""
        self.pipeline = idx
        if hasattr(self, "_pipeline_choice"):
            self._pipeline_choice.set("OIDN" if idx == 1 else "Raw")
        self._update_info()
        if cancel_auto and self._switch_id:
            self.after_cancel(self._switch_id)
            self._switch_id = None
    def _select_pipeline(self, _):
        """Apply the image-pipeline selection from the render-footer dropdown."""
        self._set_pipeline(1 if self._pipeline_choice.get() == "OIDN" else 0)

    def _schedule_switch(self):
        """Schedule automatic switching from raw output to OIDN."""
        if self._switch_id:
            self.after_cancel(self._switch_id)
        self._switch_id = self.after(self.switch_sec.get() * 1000, self._auto_switch)

    def _reset_switch(self):
        """Return to raw output and restart the Auto OIDN countdown."""
        self._set_pipeline(0)
        self._schedule_switch()

    def _auto_switch(self):
        """Switch the active display pipeline to OIDN when the countdown expires."""
        self._switch_id = None
        self._set_pipeline(1, cancel_auto=False)

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
        """Prompt for an image path and save the current rendered film."""
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


    def _update_info(self):
        """Refresh the compact camera, exposure, HDRI, and pipeline status text."""
        az, el = self.az.get(), self.el.get()
        gain = 10.0 ** self._hdri_gain_log.get()
        self._info.config(text=f"  az={az:6.1f}°  el={el:5.1f}°"
                               f"  dist={self._camera_distance:5.2f}"
                               f"  exp={self.exposure.get():.3f}"
                               f"  hdri={gain:.3f}"
                               f"  bg={'HDRI' if self._render_hdri_background.get() else 'white'}"
                               f"  pipe={self.pipeline}")

    # ── Input ─────────────────────────────────────────────────────────────────
    def _drag_start(self, e):
        """Record the pointer position that begins an orbit drag."""
        self._drag_x, self._drag_y = e.x, e.y

    def _drag_move(self, e):
        # Orbit drags are turntable-style: level the horizon.
        """Orbit the camera from a right-drag movement and queue a camera update."""
        self._camera_up = [0.0, 0.0, 1.0]
        self.az.set(self.az.get() + (e.x - self._drag_x) * 0.5)
        self.el.set(max(-89, min(89, self.el.get() + (e.y - self._drag_y) * 0.3)))
        self._drag_x, self._drag_y = e.x, e.y
        self._update_info()
        self._on_camera(refresh_ui=False)


    # ── Render canvas input ───────────────────────────────────────────────────
    def _pan_start(self, e):
        """Record the pointer position that begins a render-viewport pan."""
        self._pan_drag_x, self._pan_drag_y = e.x, e.y

    def _pan_move(self, e):
        """Pan the target in screen space from a render-viewport drag."""
        dx = e.x - self._pan_drag_x
        dy = e.y - self._pan_drag_y
        self._pan_drag_x, self._pan_drag_y = e.x, e.y

        # Exact screen-space pan: the focal-plane point that was under
        # the cursor stays under it. The film fills the display canvas,
        # so one display pixel spans
        # 2 * distance * tan(vertical fov / 2) / display height
        # world units at the orbit-target depth.
        lux_fov = self._camera_fieldofview(
            self._camera_fov, self._render_width, self._render_height)
        half_tan = math.tan(math.radians(lux_fov) * 0.5)
        frame = self._render_width / self._render_height
        tan_half_v = half_tan / frame if frame >= 1.0 else half_tan
        scale = (2.0 * self._camera_distance * tan_half_v
                 / max(1, self._display_h))
        orig   = self._cam_orig()
        cr, cu, _ = cam_axes(orig, self._target)

        # Move the camera opposite to the pointer so the rendered content follows it.
        offset = [(-dx * scale * cr[i]) + (dy * scale * cu[i]) for i in range(3)]

        self._target = [self._target[i] + offset[i] for i in range(3)]
        self._on_camera()

    def _render_scroll(self, e):
        """Zoom while retaining the focal-plane point under the cursor."""
        self._zoom_at_cursor(e, self._display_w, self._display_h)

    def _set_preset(self, az, el):
        # Preset views are canonical: restore the world up vector so a rolled
        # external lookat cannot tilt them.
        """Apply a level canonical orbit preset and queue a camera update."""
        self._camera_up = [0.0, 0.0, 1.0]
        self.az.set(az); self.el.set(el)
        self._on_camera()

    def _select_preset(self, _):
        """Apply the camera preset selected in the controller dropdown."""
        presets = {
            "Front": (0, 10), "Back": (180, 10),
            "Left Side": (-90, 10), "Right Side": (90, 10),
            "Hero Front Left": (-45, 18), "Hero Front Right": (45, 18),
            "Hero Back Left": (-135, 18), "Hero Back Right": (135, 18),
            "Top": (0, 89), "Bottom": (0, -89),
        }
        preset = presets.get(self._preset_choice.get())
        if preset:
            self._set_preset(*preset)

    def _reset(self):
        # Drop external view overrides along with the orbit position.
        """Restore the default orbit and clear external camera orientation overrides."""
        self._camera_up = [0.0, 0.0, 1.0]
        self._camera_fov = None
        self._set_preset(DEFAULT_AZ, DEFAULT_EL)


if __name__ == "__main__":
    app = CameraController()
    app.mainloop()
