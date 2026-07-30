"""Verify the camera_controller TCP control interface and mesh uploads.

Usage (from this directory):
  python control_upload_test.py            # framing checks + live end-to-end run
  python control_upload_test.py --offline  # framing checks only (no window, no GPU)

The live run launches camera_controller.py on a private port selected with the
LUXCORE_CONTROL_PORT environment variable, streams meshes over the control
socket, and shuts the controller down again. camera_controller_settings.json
is backed up and restored, so a full run leaves no state behind.
"""

import json
import os
import shutil
import socket
import struct
import subprocess
import sys
import tempfile
import time

UI_DIR = os.path.dirname(os.path.abspath(__file__))
CONTROLLER = os.path.join(UI_DIR, "camera_controller.py")
SETTINGS = os.path.join(UI_DIR, "camera_controller_settings.json")
TEST_PORT = 8975

sys.path.insert(0, UI_DIR)
import camera_controller as controller  # noqa: E402  (needs UI_DIR on sys.path)


# ── Offline framing checks ────────────────────────────────────────────────────
def frame_tests():
    a, b = socket.socketpair()

    # buffers-list protocol: JSON header plus one declared binary buffer.
    header = {"cmd": "define_mesh", "name": "m",
              "buffers": [{"role": "points", "bytes": 12}]}
    payload = json.dumps(header).encode("utf-8")
    blob = struct.pack("<3f", 1.0, 2.0, 3.0)
    a.sendall(struct.pack("<I", len(payload)) + payload + blob)
    got_header, got_blobs = controller._read_control_message(b)
    assert got_header == header, got_header
    assert got_blobs == {"points": blob}, got_blobs

    # upload_mesh protocol: PascalCase sections read in fixed order, with
    # zero-length sections producing empty buffers.
    header = {"Command": "upload_mesh", "MeshName": "m",
              "Vertices": {"ByteLength": 12}, "Normals": {"ByteLength": 0},
              "UVs": {"ByteLength": 0}, "Indices": {"ByteLength": 12}}
    payload = json.dumps(header).encode("utf-8")
    verts = struct.pack("<3f", 0.0, 0.0, 0.0)
    idx = struct.pack("<3i", 0, 0, 0)
    a.sendall(struct.pack("<I", len(payload)) + payload + verts + idx)
    got_header, got_blobs = controller._read_control_message(b)
    assert got_header == header, got_header
    assert got_blobs == {"vertices": verts, "normals": b"",
                         "uvs": b"", "indices": idx}, got_blobs
    assert controller._header_command(got_header) == "upload_mesh"

    # A plain command with no binary payload.
    header = {"cmd": "status"}
    payload = json.dumps(header).encode("utf-8")
    a.sendall(struct.pack("<I", len(payload)) + payload)
    got_header, got_blobs = controller._read_control_message(b)
    assert got_header == header and got_blobs == {}

    # Reply framing round-trips.
    controller._send_control_message(b, {"ok": True, "value": 7})
    (size,) = struct.unpack("<I", controller._recv_exact(a, 4))
    reply = json.loads(controller._recv_exact(a, size).decode("utf-8"))
    assert reply == {"ok": True, "value": 7}, reply

    # Oversized headers are rejected before any allocation.
    a.sendall(struct.pack("<I", controller.CONTROL_MAX_HEADER + 1))
    try:
        controller._read_control_message(b)
        raise SystemExit("FAILED: oversized header was accepted")
    except ValueError:
        pass

    a.close()
    b.close()
    print("frame tests: OK")


def hdri_alignment_tests():
    """Verify a positive HDRI height moves equirectangular source rows upward."""
    with tempfile.TemporaryDirectory() as directory:
        source_path = os.path.join(directory, "source.hdr")
        shifted_path = os.path.join(directory, "shifted.hdr")
        width, height = 8, 4
        source_rows = [
            bytes((20 + row, 10 + row, 5 + row, 136)) * width
            for row in range(height)
        ]
        with open(source_path, "wb") as source:
            source.write(b"#?RADIANCE\nFORMAT=32-bit_rle_rgbe\n\n-Y 4 +X 8\n")
            for row in source_rows:
                controller._write_radiance_scanline(source, row, width)

        # Height is intentionally calibrated at half the panorama angular rate:
        # four rows span 180°, so +45° shifts the source by half a row.
        assert controller._hdri_vertical_offset_pixels(45.0, height) == 0.5
        controller._remap_radiance_hdr(
            source_path, shifted_path, 1.0)
        with open(shifted_path, "rb") as shifted:
            _, shifted_width, shifted_height = controller._read_radiance_header(
                shifted)
            shifted_rows = [
                controller._read_radiance_scanline(shifted, shifted_width)
                for _ in range(shifted_height)
            ]
        assert (shifted_width, shifted_height) == (width, height)
        assert shifted_rows == [
            source_rows[1], source_rows[2], source_rows[3], source_rows[3]], (
                shifted_rows)
        yaw = controller._hdri_yaw_matrix(37.5)
        assert yaw[2] == yaw[6] == 0.0 and yaw[10] == 1.0, yaw

        import Imath
        import OpenEXR
        import numpy
        exr_source_path = os.path.join(directory, "source.exr")
        exr_shifted_path = os.path.join(directory, "shifted.exr")
        pixel_type = Imath.PixelType(Imath.PixelType.FLOAT)
        header = OpenEXR.Header(width, height)
        header["channels"] = {
            name: Imath.Channel(pixel_type) for name in ("R", "G", "B")}
        source_values = numpy.repeat(
            numpy.arange(height, dtype=numpy.float32)[:, None], width, axis=1)
        output = OpenEXR.OutputFile(exr_source_path, header)
        try:
            output.writePixels({
                "R": source_values.tobytes(),
                "G": (source_values + 10.0).tobytes(),
                "B": (source_values + 20.0).tobytes(),
            })
        finally:
            output.close()
        controller._remap_openexr(exr_source_path, exr_shifted_path, 1.0)
        shifted_exr = OpenEXR.InputFile(exr_shifted_path)
        try:
            shifted_values = numpy.frombuffer(
                shifted_exr.channel("R", pixel_type),
                dtype=numpy.float32).reshape(height, width)
        finally:
            shifted_exr.close()
        assert numpy.array_equal(
            shifted_values[:, 0], numpy.array([1.0, 2.0, 3.0, 3.0])), (
                shifted_values[:, 0])
    print("HDRI vertical remap: OK")


# ── Live end-to-end run ───────────────────────────────────────────────────────
def upload_header(mesh_name, vertex_count, index_count, uv_bytes, normal_bytes):
    """Build a header exactly like the C# MeshHeader serialization."""
    return {
        "Command": "upload_mesh",
        "Version": 1,
        "MeshName": mesh_name,
        "Vertices": {"ElementCount": vertex_count, "ElementSize": 12,
                     "ByteLength": vertex_count * 12, "Format": "float32x3"},
        "Normals": {"ElementCount": normal_bytes // 12, "ElementSize": 12,
                    "ByteLength": normal_bytes, "Format": "float32x3"},
        "UVs": {"ElementCount": uv_bytes // 12, "ElementSize": 12,
                "ByteLength": uv_bytes, "Format": "float32x3"},
        "Indices": {"ElementCount": index_count, "ElementSize": 4,
                    "ByteLength": index_count * 4, "Format": "int32"},
    }


def live_tests():
    settings_backup = SETTINGS + ".test-backup"
    had_settings = os.path.isfile(SETTINGS)
    if had_settings:
        shutil.copyfile(SETTINGS, settings_backup)
        # Launch with clean defaults so the run does not depend on (or
        # disturb) the user's live camera, HDRI, and resolution state.
        os.remove(SETTINGS)

    env = dict(os.environ, LUXCORE_CONTROL_PORT=str(TEST_PORT))
    proc = subprocess.Popen([sys.executable, CONTROLLER],
                            cwd=UI_DIR, env=env)

    def restore_settings():
        if had_settings:
            shutil.copyfile(settings_backup, SETTINGS)
            os.remove(settings_backup)
        else:
            try:
                os.remove(SETTINGS)
            except FileNotFoundError:
                pass

    def fail(message):
        if proc.poll() is None:
            proc.terminate()
            try:
                proc.wait(timeout=15)
            except subprocess.TimeoutExpired:
                proc.kill()
        restore_settings()
        raise SystemExit(f"FAILED: {message}")

    def connect(timeout=90.0):
        deadline = time.time() + timeout
        while time.time() < deadline:
            if proc.poll() is not None:
                fail(f"controller exited early with code {proc.returncode}")
            try:
                conn = socket.create_connection(("127.0.0.1", TEST_PORT),
                                                timeout=2.0)
                conn.settimeout(120.0)
                return conn
            except OSError:
                time.sleep(0.5)
        fail("could not connect to the control port")

    def recv_exact(conn, count):
        data = bytearray()
        while len(data) < count:
            chunk = conn.recv(count - len(data))
            if not chunk:
                fail("control connection closed unexpectedly")
            data.extend(chunk)
        return bytes(data)

    def send(conn, header, blobs=()):
        payload = json.dumps(header).encode("utf-8")
        conn.sendall(struct.pack("<I", len(payload)) + payload)
        for blob in blobs:
            conn.sendall(blob)
        (size,) = struct.unpack("<I", recv_exact(conn, 4))
        return json.loads(recv_exact(conn, size).decode("utf-8"))

    # Shared quad data in the C# layout (IwVector3f attributes, int32 indices).
    verts = struct.pack("<12f",
                        -0.6, 1.2, 0.05,  0.6, 1.2, 0.05,
                         0.6, 2.4, 0.05, -0.6, 2.4, 0.05)
    norms = struct.pack("<12f",
                        0.0, 0.0, 1.0, 0.0, 0.0, 1.0,
                        0.0, 0.0, 1.0, 0.0, 0.0, 1.0)
    uvs = struct.pack("<12f",
                      0.0, 0.0, 0.0, 1.0, 0.0, 0.0,
                      1.0, 1.0, 0.0, 0.0, 1.0, 0.0)
    indices = struct.pack("<6i", 0, 1, 2, 0, 2, 3)

    conn = connect()

    status = send(conn, {"cmd": "status"})
    if not status.get("ok"):
        fail("status returned not ok")
    print("status: OK; render_stopped:", status.get("render_stopped"))

    # apply with nothing staged must fail cleanly.
    reply = send(conn, {"cmd": "apply"})
    if reply.get("ok") or "Nothing staged" not in reply.get("error", ""):
        fail(f"empty apply produced the wrong reply: {reply}")
    print("empty apply rejected: OK")

    reply = send(conn, {"cmd": "camera", "az": 95.0, "el": 12.0, "dist": 18.0})
    if not (reply.get("ok") and abs(reply["azimuth"] - 95.0) < 1e-6
            and abs(reply["distance"] - 9.0) < 1e-6):
        fail(f"camera command failed: {reply}")
    print("camera sender distance half-scale: OK")

    # C#-style CameraUpdate: eye/target/up/fov with derived orbit state.
    # The controller intentionally halves the eye-to-target distance after
    # deriving elevation from the original eye-to-target direction.
    reply = send(conn, {"cmd": "cameraEyeTarget",
                        "eye": [0.0, -10.0, 2.0], "target": [0.0, 0.0, 1.0],
                        "up": [0.0, 0.0, 1.0], "fov": 50.0})
    if not (reply.get("ok")
            and abs(reply["distance"] - 5.0249378) < 1e-3
            and abs(reply["elevation"] - 5.710593) < 0.01
            and abs(reply["azimuth"] - 180.0) < 1e-6
            and reply["fov"] == 50.0 and reply["fov_axis"] == "vertical"):
        fail(f"cameraEyeTarget derived the wrong view: {reply}")
    print("cameraEyeTarget: OK")

    # Orthographic senders use fov 0 and a zero up vector: both are ignored.
    reply = send(conn, {"cmd": "lookat", "eye": [5.0, 0.0, 1.0],
                        "target": [0.0, 0.0, 1.0], "up": [0.0, 0.0, 0.0],
                        "fov": 0.0})
    if not (reply.get("ok") and abs(reply["azimuth"] - 90.0) < 1e-6
            and abs(reply["distance"] - 2.5) < 1e-9
            and reply["fov"] == 50.0 and reply["up"] == [0.0, 0.0, 1.0]):
        fail(f"lookat alias with ortho defaults failed: {reply}")
    status = send(conn, {"cmd": "status"})
    if status.get("fov") != 50.0 or status.get("up") != [0.0, 0.0, 1.0]:
        fail(f"status does not report the lookat overrides: {status}")
    print("lookat alias and ortho defaults: OK")

    # A rolled lookat up vector must not tilt the canonical preset views.
    reply = send(conn, {"cmd": "lookat", "eye": [0.0, -10.0, 2.0],
                        "target": [0.0, 0.0, 0.5], "up": [0.0, 0.7, 0.7],
                        "fov": 45.0})
    if not reply.get("ok") or abs(reply["up"][2] - 0.70710678) > 1e-6:
        fail(f"tilted lookat up was not stored: {reply}")
    reply = send(conn, {"cmd": "preset", "az": 0.0, "el": 10.0})
    if not reply.get("ok"):
        fail(f"preset failed: {reply}")
    status = send(conn, {"cmd": "status"})
    if status.get("up") != [0.0, 0.0, 1.0]:
        fail(f"preset did not restore the world up vector: {status.get('up')}")
    print("preset restores a level horizon: OK")

    # AO clay mode swaps to the white dome and back.
    reply = send(conn, {"cmd": "ao", "enabled": True})
    if not (reply.get("ok") and reply["ao_mode"] is True):
        fail(f"ao enable failed: {reply}")
    status = send(conn, {"cmd": "status"})
    if status.get("ao_mode") is not True:
        fail(f"status does not report ao_mode: {status}")
    reply = send(conn, {"cmd": "ao", "enabled": False})
    if not (reply.get("ok") and reply["ao_mode"] is False):
        fail(f"ao disable failed: {reply}")
    print("ao clay mode toggles: OK")

    # The HDRI ground plane toggles on and off.
    reply = send(conn, {"cmd": "ground", "enabled": True})
    if not (reply.get("ok") and reply["hdri_ground"] is True):
        fail(f"ground enable failed: {reply}")
    status = send(conn, {"cmd": "status"})
    if status.get("hdri_ground") is not True:
        fail(f"status does not report hdri_ground: {status}")
    reply = send(conn, {"cmd": "ground", "enabled": False})
    if not (reply.get("ok") and reply["hdri_ground"] is False):
        fail(f"ground disable failed: {reply}")
    print("hdri ground plane toggles: OK")
    # Keep the HDRI background and ground plane enabled while vertically
    # remapping the shared panorama. The restart must retain both visibility
    # states, create an aligned cache source, and produce a fresh live render.
    reply = send(conn, {"cmd": "background", "hdri": True})
    if not (reply.get("ok") and reply["render_hdri_background"] is True):
        fail(f"HDRI background enable failed: {reply}")
    reply = send(conn, {"cmd": "ground", "enabled": True})
    if not (reply.get("ok") and reply["hdri_ground"] is True):
        fail(f"ground enable for alignment failed: {reply}")
    reply = send(conn, {
        "cmd": "hdri_alignment", "height": 7.5, "rotation": -37.5})
    if not (reply.get("ok")
            and abs(reply["hdri_height"] - 7.5) < 1e-6
            and abs(reply["hdri_rotation"] + 37.5) < 1e-6):
        fail(f"HDRI alignment update failed: {reply}")
    deadline = time.time() + 120
    status = {}
    while time.time() < deadline:
        time.sleep(1.0)
        status = send(conn, {"cmd": "status"})
        if (not status.get("busy") and status.get("passes", 0) >= 2
                and abs(status.get("hdri_height", 0.0) - 7.5) < 1e-6
                and abs(status.get("hdri_rotation", 0.0) + 37.5) < 1e-6):
            break
    if not (status.get("render_hdri_background") is True
            and status.get("hdri_ground") is True
            and status.get("passes", 0) >= 2
            and abs(status.get("hdri_height", 0.0) - 7.5) < 1e-6
            and abs(status.get("hdri_rotation", 0.0) + 37.5) < 1e-6
            and isinstance(status.get("active_hdr_file"), str)
            and os.path.isfile(status["active_hdr_file"])
            and os.path.normcase(os.path.abspath(status["active_hdr_file"])) !=
                os.path.normcase(os.path.abspath(status["hdr_file"]))):
        fail(f"HDRI alignment did not produce a live render: {status}")
    print("HDRI background and vertical alignment restart: OK")

    # Sender dimensions resize the visible viewport and the active film.
    reply = send(conn, {"cmd": "cameraEyeTarget",
                        "eye": [0.0, -10.0, 2.0], "target": [0.0, 0.0, 0.5],
                        "up": [0.0, 0.0, 1.0], "fov": 45.0,
                        "width": 960, "height": 540})
    if not (reply.get("ok") and reply["width"] == 960
            and reply["height"] == 540
            and reply["viewport_width"] == 960
            and reply["viewport_height"] == 540
            and reply["base_width"] == 960 and reply["base_height"] == 540):
        fail(f"lookat viewport size failed: {reply}")
    status = send(conn, {"cmd": "status"})
    if (status.get("width") != 960 or status.get("height") != 540
            or status.get("viewport_width") != 960
            or status.get("viewport_height") != 540
            or status.get("base_width") != 960
            or status.get("base_height") != 540):
        fail(f"viewport size was not applied: {status}")
    reply = send(conn, {"cmd": "resolution", "width": 1280, "height": 720})
    if not reply.get("ok"):
        fail(f"resolution restore failed: {reply}")
    status = send(conn, {"cmd": "status"})
    if (status.get("width") != 1280 or status.get("height") != 720
            or status.get("viewport_width") != 960
            or status.get("viewport_height") != 540):
        fail(f"film-only resolution changed the viewport: {status}")
    print("lookat viewport size and film-only resolution: OK")

    # CAD-scale distances must survive the 1-50 UI distance slider.
    reply = send(conn, {"cmd": "cameraEyeTarget",
                        "eye": [0.0, -400.0, 30.0], "target": [0.0, 0.0, 1.0],
                        "up": [0.0, 0.0, 1.0], "fov": 40.0})
    expected = ((400.0 ** 2 + 29.0 ** 2) ** 0.5) / 2.0
    if not (reply.get("ok") and abs(reply["distance"] - expected) < 1e-6):
        fail(f"large-distance lookat failed: {reply}")
    status = send(conn, {"cmd": "status"})
    if abs(status["distance"] - expected) > 1e-6:
        fail(f"large distance was clamped by the UI: {status['distance']}")
    reply = send(conn, {"cmd": "camera", "az": 95.0, "el": 12.0, "dist": 18.0})
    if not reply.get("ok") or abs(reply["distance"] - 9.0) > 1e-9:
        fail(f"camera restore failed: {reply}")
    print("large-distance lookat: OK")

    # buffers-list protocol mesh with explicit object properties.
    quad_points = struct.pack("<12f",
                              -0.6, 2.6, 0.05,  0.6, 2.6, 0.05,
                               0.6, 3.4, 0.05, -0.6, 3.4, 0.05)
    quad_tris = struct.pack("<6I", 0, 1, 2, 0, 2, 3)
    reply = send(conn, {
        "cmd": "define_mesh", "name": "ctrl_quad",
        "buffers": [{"role": "points", "dtype": "float32",
                     "bytes": len(quad_points)},
                    {"role": "triangles", "dtype": "uint32",
                     "bytes": len(quad_tris)}]},
        (quad_points, quad_tris))
    if not (reply.get("ok") and reply["vertices"] == 4):
        fail(f"define_mesh failed: {reply}")
    reply = send(conn, {"cmd": "scene_props", "text": "\n".join((
        "scene.materials.ctrl_mat.type = matte",
        "scene.materials.ctrl_mat.kd = 0.75 0.15 0.15",
        "scene.objects.ctrl_obj.shape = ctrl_quad",
        "scene.objects.ctrl_obj.material = ctrl_mat",
    ))})
    if not reply.get("ok"):
        fail(f"scene_props failed: {reply}")
    print("define_mesh + scene_props: OK")

    # C#-style uploads: full attributes, then empty normals/uvs sections;
    # connection-per-mesh like SendOneMesh.
    reply = send(conn, upload_header("Body Panel 1", 4, 6, len(uvs), len(norms)),
                 (verts, norms, uvs, indices))
    if not (reply.get("ok") and reply["mesh"] == "Body_Panel_1"
            and reply["vertices"] == 4 and reply["triangles"] == 2
            and reply["object_created"]):
        fail(f"upload_mesh with attributes failed: {reply}")
    print("upload_mesh (normals + IwVector3f uvs): OK")
    conn.close()

    conn = connect(timeout=10.0)
    reply = send(conn, upload_header("Trim.Strip", 4, 6, 0, 0),
                 (verts, indices))
    if not (reply.get("ok") and reply["mesh"] == "Trim_Strip"):
        fail(f"upload_mesh with empty sections failed: {reply}")
    print("upload_mesh (empty normals/uvs): OK")

    # No explicit apply: the debounced auto-apply must consume everything.
    passes = 0
    staged = ["pending"]
    deadline = time.time() + 120
    while time.time() < deadline:
        time.sleep(2.0)
        status = send(conn, {"cmd": "status"})
        passes = status.get("passes", 0)
        staged = status.get("staged_meshes", [])
        if passes >= 2 and not staged and not status.get("busy"):
            break
    if passes < 2 or staged:
        fail(f"auto-apply did not render uploads: passes={passes} staged={staged}")
    print(f"auto-apply rendered the streamed meshes: OK ({passes} passes)")

    film_path = os.path.join(tempfile.gettempdir(), "luxcore_control_test.png")
    if os.path.exists(film_path):
        os.remove(film_path)
    reply = send(conn, {"cmd": "save_film", "path": film_path})
    if not (reply.get("ok") and os.path.getsize(film_path) > 0):
        fail(f"save_film failed: {reply}")
    os.remove(film_path)
    print("save_film: OK")

    reply = send(conn, {"cmd": "shutdown"})
    if not reply.get("ok"):
        fail(f"shutdown failed: {reply}")
    conn.close()
    try:
        proc.wait(timeout=30)
    except subprocess.TimeoutExpired:
        fail("controller did not exit after shutdown")
    print("shutdown: OK; controller exit code:", proc.returncode)

    restore_settings()


if __name__ == "__main__":
    frame_tests()
    hdri_alignment_tests()
    if "--offline" in sys.argv[1:]:
        print("offline mode: skipping the live controller run")
    else:
        live_tests()
    print("control upload test: OK")
