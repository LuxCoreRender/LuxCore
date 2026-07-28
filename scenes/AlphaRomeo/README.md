# Alpha Romeo interactive renderer

`camera_controller.py` runs the Alpha Romeo scene in a single Tk window. Start it from this directory after building the Release `pyluxcore` target:

```text
python camera_controller.py
```
HDR drag-and-drop requires `tkinterdnd2`:

```text
python -m pip install --user tkinterdnd2
```

## Image pipeline and HDRI

`ModoAlphaRomeo.cfg` uses the LuxLinear tone mapper with gamma correction for both the raw and OIDN image pipelines. Keep their values aligned:

- `sensitivity = 100`
- `exposure = 5.0`
- `fstop = 2.8`
- gamma `2.2`

These settings preserve HDRI and clear-coat detail while limiting highlight clipping. `ModoAlphaRomeo.scn` selects `hdre_055.hdr` and its starting infinite-light gain; the controller's HDRI Gain slider replaces that gain at runtime.

The controller's Exposure control updates the LuxLinear `exposure` for both pipelines and restarts rendering, so its changes apply consistently to raw and denoised output.

## Renderer selection

The controller uses `PATHOCL` with the `OPTIX` accelerator, selecting the CUDA GPU for hardware ray tracing. `opencl.cpu.use = 0`, `opencl.gpu.use = 1`, and `opencl.native.threads.count = 0` ensure it uses the GPU path rather than an OpenCL CPU or native render thread.

The render-window title displays `PATHOCL / OptiX`. Film hardware processing remains disabled because the controller reads raw and OIDN film output on the host. GPU ray tracing was validated with a 320×180 Alpha Romeo render that reached 1,019 passes with every RGB channel populated.

When CUDA 12.4 is installed at its standard Windows location, the controller explicitly selects its NVRTC DLL before importing `pyluxcore`. This prevents a newer installed CUDA toolkit from producing PTX that an older NVIDIA driver cannot load. Set `LUXRAYS_NVRTC_LIBRARY` to an absolute NVRTC DLL path to override that choice.
## Changing the HDRI

Drop a `.hdr` or `.exr` file onto the render viewport to replace the infinite-light image. The controller uses `tkinterdnd2`'s native OLE file-drop support, restarts the current render using the dropped HDRI, and preserves the source path in `camera_controller_settings.json`; it does not copy the HDRI into the scene directory.
Each environment map is loaded twice in memory. The `hdri` light uses a one-eighth-resolution copy for direct illumination and indirect diffuse, glossy, and specular rays; for example, a `16384 × 8192` source becomes `2048 × 1024`. The `hdri_background` light retains the original source pixel dimensions and is visible only to camera rays, so the render background stays sharp without adding illumination or reflections.
Clear **Render HDRI Background** to replace the full-resolution camera map with a camera-only white background. The HDRI remains active at one-eighth resolution for lighting and reflections, but its full-resolution copy is not loaded.

The original HDR/EXR file is never rewritten. Large source maps use half-float storage without changing their dimensions. The two copies coexist only while rendering the HDRI background, so switching maps or using white removes no-longer-used image maps after the new lights are installed.
## Stopping and restarting

Select **Stop Rendering** to halt refinement and retain the displayed film for saving. The same button changes to **Start Rendering**; selecting it creates a fresh session with the current camera, resolution, and exposure settings.
## Saved settings

The controller saves the camera target, orbit, distance, exposure, HDRI gain, background-display preference, Auto OIDN delay, and selected render resolution to `camera_controller_settings.json`. These values are restored when the controller starts again.

## External control

`camera_controller.py` runs a TCP command server for external programs written in any language. The port comes from `control_port` in `camera_controller_settings.json` (default `8765`) or the `LUXCORE_CONTROL_PORT` environment variable; `0` disables it. The server listens on `127.0.0.1` only.

Every message is framed as a 4-byte little-endian header length, a UTF-8 JSON header, then any binary buffers announced by the header's `buffers` list (`role` plus `bytes`, sent in list order). Every message receives one framed JSON reply such as `{"ok": true, ...}` or `{"ok": false, "error": "..."}`.

Immediate commands: `camera` (`az`, `el`, `dist`), `target` (`xyz`), `preset` (`az`, `el`), `reset`, `exposure` (`value`), `hdri_gain` (`value`), `hdri_file` (`path`), `background` (`hdri` true/false), `resolution` (`width`, `height`), `pipeline` (`index` 0 raw, 1 OIDN), `stop`, `start`, `save_film` (`path`), `status`, and `shutdown`.

Geometry and scene content stream through staged commands, and one restart applies everything:

- `define_mesh` stages a named mesh from binary buffers: `points` (float32, N x 3) and `triangles` (uint32 zero-based, M x 3) are required; `normals` (float32, N x 3) and `uvs` (float32, N x 2) are optional. Requires `numpy`.
- `scene_props` and `config_props` stage LuxCore property text; scene objects reference staged meshes with `scene.objects.<name>.shape = <meshName>` plus a material.
- `apply` restarts rendering once with all staged meshes and properties.

This lets a C# client send raw vertex and index arrays (for example with `MemoryMarshal.AsBytes`) instead of writing `.ply` files, while `.scn` and `.cfg` content travels as plain property text.

The `upload_mesh` command accepts the C# `MeshHeader` layout instead of a `buffers` list: `Command`, `MeshName`, and `Vertices`/`Normals`/`UVs`/`Indices` sections whose `ByteLength` fields describe binary buffers sent in that fixed order. Vertices and normals are float32 triples; UVs may be float32 triples (`IwVector3f`, the unused third component is dropped) or pairs; indices are flat 32-bit integers in triangle order, and empty sections are skipped. Mesh names are sanitized for LuxCore property syntax. Each upload also stages a gray matte object for the mesh unless the header sets `CreateObject` to `false`, and an automatic `apply` runs half a second after the last upload, so a client that only streams meshes sees them rendered without further commands; explicit `apply`, `scene_props`, and `config_props` still work and can restyle uploaded meshes.

Run `python control_upload_test.py` in this directory to verify the interface: it checks the frame parsing offline, then launches the controller on a private port and exercises camera control, both mesh protocols, auto-apply, film saving, and shutdown. Pass `--offline` to skip the live render run.

## Generated files

Film outputs, logs, and files such as `Reinhard_HDRI_Verification.png` and `camera_controller_settings.json` are local artifacts and are not source files to commit.
