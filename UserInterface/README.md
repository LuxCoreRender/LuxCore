# LuxCore interactive render controller

`camera_controller.py` renders an empty stage (`scene.scn`, camera plus the default `hdre_055.hdr` environment) in a single Tk window; geometry streams in through the external control interface described below. Start it from this directory with `StartLuxCore.vbs` (no console window at all) or `StartLuxCore.bat`; both use the windowless `pythonw` interpreter and append Python errors to `render.log`. Alternatively, after building the Release `pyluxcore` target:

```text
python camera_controller.py
```
HDR drag-and-drop requires `tkinterdnd2`:

```text
python -m pip install --user tkinterdnd2
```

## Image pipeline and HDRI

`render.cfg` uses the Reinhard02 tone mapper with gamma correction for both the raw and OIDN image pipelines. Its luminance roll-off preserves color separation in bright HDRI highlights instead of clipping channels independently. Keep the two pipelines aligned:

- reference exposure `5.0` (`postscale = 1.2`)
- `prescale = 1.0`
- `burn = 3.75`
- gamma `2.2`

These settings preserve HDRI detail while smoothly compressing highlights. `scene.scn` contains the camera, the default `hdre_055.hdr` environment, and a camera-invisible millimetre placeholder triangle (defined inline, no `.ply`) that keeps the GPU accelerator from ever seeing a zero-triangle scene, which crashes intermittently. The controller's HDRI Gain jog below the viewport shows the active numeric value, applies a relative adjustment to the left or right, then returns to center; the Alpha Romeo demo assets remain untouched in `scenes/AlphaRomeo`.

The `exposure` control command updates Reinhard02 `postscale` for both pipelines and restarts rendering, so external changes apply consistently to raw and denoised output.

## Renderer selection

The controller uses `PATHOCL` with the `OPTIX` accelerator, selecting the CUDA GPU for hardware ray tracing. `opencl.cpu.use = 0`, `opencl.gpu.use = 1`, and `opencl.native.threads.count = 0` ensure it uses the GPU path rather than an OpenCL CPU or native render thread.

The render-window title displays `PATHOCL / OptiX`. Film hardware processing remains disabled because the controller reads raw and OIDN film output on the host.

When CUDA 12.4 is installed at its standard Windows location, the controller explicitly selects its NVRTC DLL before importing `pyluxcore`. This prevents a newer installed CUDA toolkit from producing PTX that an older NVIDIA driver cannot load. Set `LUXRAYS_NVRTC_LIBRARY` to an absolute NVRTC DLL path to override that choice.
## Changing the HDRI

Drop a `.hdr` or `.exr` file onto the render viewport to replace the infinite-light image. The controller uses `tkinterdnd2`'s native OLE file-drop support, restarts the current render using the dropped HDRI, and preserves the source path in `camera_controller_settings.json`; it does not copy the HDRI into the scene directory.
**HDRI Height** vertically shifts the equirectangular image by up to 45° in either direction; a positive value moves image features upward. It is calibrated at half the panorama angular rate, so a 45° control value shifts the source image by 22.5°. The shift clamps at the poles rather than wrapping the sky around the bottom or ground around the top. **HDRI Rotation** yaws the dome by up to 180° in either direction. The controls are below **Stop Rendering** in the left panel, restart rendering, and are saved with the controller settings. Height generates a keyed temporary remap while rotation remains a directional transform, so the sharp camera background, reduced-resolution lighting dome, and HDRI ground texture all use the same shifted source.
Each environment map is loaded twice in memory. The `hdri` light uses a one-eighth-resolution copy for direct illumination and indirect diffuse and glossy rays; for example, a `16384 × 8192` source becomes `2048 × 1024`. The `hdri_background` light retains the original source pixel dimensions and is visible to camera rays and specular ray continuations, so the render background, mirror-like reflections, and the shadow-catcher ground's see-through areas stay sharp without adding diffuse or glossy illumination.
Clear **Render HDRI Background** to replace the full-resolution camera map with a camera-only white background. The HDRI remains active at one-eighth resolution for lighting and reflections, but its full-resolution copy is not loaded.

The original HDR/EXR file is never rewritten. A nonzero height value writes a reusable remapped copy under the system temporary directory; it can be discarded safely because it is regenerated from the source file and saved alignment value. Environment and HDRI-ground image maps always use full floating-point storage, preserving high-intensity source values without changing their dimensions. The two copies coexist only while rendering the HDRI background, so switching maps or using white removes no-longer-used image maps after the new lights are installed.
## Viewport and film size

**Viewport Base** selects the unscaled display resolution. **Window Scale** scales that viewport by `1/20`, `1/10`, `1/5`, `1/3`, `1/2`, `1`, `2`, `3`, `4`, or `5`; the render canvas and outer window follow the scaled viewport, subject to screen bounds. When the window is shorter than the controls, the left control panel scrolls instead of preventing the whole window from shrinking.

**Final Film Scale** uses the same factors independently for the LuxCore output film. It is calculated from the actual display canvas after screen fitting, retains the viewport aspect ratio as closely as integer pixels allow, and proportionally caps the longest film axis at 8192 pixels. The film-size label beside the selector shows the current renderer output. For example, a displayed `512 × 288` viewport at a final-film scale of `2` renders a `1024 × 576` film which is downsampled for display. Saved films retain the native final-film dimensions.

The base resolution and both scale choices are saved. Selecting a final-film scale clears any exact external output-size override.
## Stopping and restarting

Select **Stop Rendering** to halt refinement and retain the displayed film for saving. The same button changes to **Start Rendering**; selecting it creates a fresh session with the current camera, resolution, and exposure settings.
## Saved settings

The controller saves the camera target, orbit, distance, exposure, HDRI gain, HDRI height and rotation, background-display, ambient-occlusion, and ground-plane preferences, Auto OIDN delay, viewport base and scale settings, final-film scale or exact external film size, and window size and screen position to `camera_controller_settings.json`. These values are restored when the controller starts again.

## External control

`camera_controller.py` runs a TCP command server for external programs written in any language. The port comes from `control_port` in `camera_controller_settings.json` (default `8765`) or the `LUXCORE_CONTROL_PORT` environment variable; `0` disables it. The server listens on `127.0.0.1` only.

Every message is framed as a 4-byte little-endian header length, a UTF-8 JSON header, then any binary buffers announced by the header's `buffers` list (`role` plus `bytes`, sent in list order). Every message receives one framed JSON reply such as `{"ok": true, ...}` or `{"ok": false, "error": "..."}`.

Immediate commands: `camera` (`az`, `el`, `dist`), `lookat` (below), `target` (`xyz`), `preset` (`az`, `el`), `reset`, `exposure` (`value`), `hdri_gain` (`value`), `hdri_alignment` (`height` and `rotation` in degrees), `hdri_file` (`path`), `background` (`hdri` true/false), `ao` (`enabled` true/false), `ground` (`enabled` true/false), `resolution` (`width`, `height`), `pipeline` (`index` 0 raw, 1 OIDN), `stop`, `start`, `save_film`, `status`, and `shutdown`.

`resolution` sets the LuxCore film to its exact requested `width` and `height` (16 to 8192 pixels each) without changing the viewport base or window scale. The renderer output is resampled to the current display canvas. The `status` reply exposes both output `width`/`height` and `viewport_width`/`viewport_height`; `hdr_file` is the original HDRI while `active_hdr_file` is the generated shifted source when height is nonzero.

`lookat` (alias `cameraEyeTarget`) sets the view directly from `eye`, `target`, optional `up`, optional `fov` in degrees with an `axis` of `vertical` (default) or `horizontal`, and an optional exact output-film size as `width` and `height` in pixels (16 to 8192; non-positive values are ignored). Its size fields have the same display-independent behavior as `resolution`. The controller derives its orbit state from the vectors (intentionally using half the eye-to-target distance and the corresponding elevation, to match the sender's viewport framing), preserves a supplied FOV in the protocol response, and ignores it for LuxCore rendering so the scene camera's 90° field of view keeps the HDRI background wide and stable. It ignores zero-length `up` vectors and non-positive `fov` values (as sent for orthographic viewports), and `reset` restores the scene's original up vector and field of view. A rolled `up` vector applies only until the next turntable-style interaction: the preset selector, orbit drags, and the `camera` command restore the world-Z up vector so canonical views stay level. Camera distance has no UI slider or range limit, so CAD-scale coordinates pass through exactly; no axis conversion is applied anywhere, meaning uploaded meshes and the camera share the sender's coordinate system.

Geometry and scene content stream through staged commands, and one restart applies everything:

- `define_mesh` stages a named mesh from binary buffers: `points` (float32, N x 3) and `triangles` (uint32 zero-based, M x 3) are required; `normals` (float32, N x 3) and `uvs` (float32, N x 2) are optional. Requires `numpy`.
- `scene_props` and `config_props` stage LuxCore property text; scene objects reference staged meshes with `scene.objects.<name>.shape = <meshName>` plus a material.
- `apply` restarts rendering once with all staged meshes and properties.

This lets a C# client send raw vertex and index arrays (for example with `MemoryMarshal.AsBytes`) instead of writing `.ply` files, while `.scn` and `.cfg` content travels as plain property text.

The `upload_mesh` command accepts the C# `MeshHeader` layout instead of a `buffers` list: `Command`, `MeshName`, and `Vertices`/`Normals`/`UVs`/`Indices` sections whose `ByteLength` fields describe binary buffers sent in that fixed order. Vertices and normals are float32 triples; UVs may be float32 triples (`IwVector3f`, the unused third component is dropped) or pairs; indices are flat 32-bit integers in triangle order, and empty sections are skipped. Mesh names are sanitized for LuxCore property syntax. Each upload also stages a gray matte object for the mesh unless the header sets `CreateObject` to `false`, and an automatic `apply` runs half a second after the last upload, so a client that only streams meshes sees them rendered without further commands; explicit `apply`, `scene_props`, and `config_props` still work and can restyle uploaded meshes.

The **Ambient Occlusion (clay)** checkbox — or the `ao` command with `enabled` — swaps the environment for a flat, camera-invisible area light hovering above the geometry (sized from the union of the streamed mesh bounds and rebuilt on each apply), plus a weak white dome fill over a medium-gray camera-only backdrop. The softbox remains the dominant illumination while the fill prevents curved self-occluded regions from collapsing to pure black. AO switches both image pipelines to a linear tonemap so lit surfaces map straight to white, re-binds every `upload_mesh`-created object to a shared white matte material (`kd 1.0`), and shortens the path depths (`path.pathdepth.total = 3`) so the image reads as ambient occlusion with a soft top-down studio look: upward surfaces converge to white, sides fall off gently, and creases and contact areas shade gray. Turning it off removes the lamp and restores the HDRI environment, the per-mesh materials, the Reinhard02 tonemapper with the current exposure, and the default depths. Objects defined manually through `scene_props` keep their own materials, and the HDRI Gain and exposure controls are inert while the mode is active.

The **Ground Plane** checkbox — or the `ground` command with `enabled` — adds a ground disk at `Z = 0` whose construction depends on the **Render HDRI Background** checkbox. With the background rendered: a shadow-catcher disk just above `Z = 0` with an archglass mirror beneath it. The catcher is transparent wherever the environment is unoccluded, revealing the mirror's Fresnel-weighted mesh and HDRI reflections over the transmitted full-resolution HDRI ground. With the background off (white backdrop): a colored `glossy2` floor, real geometry lit by the HDRI lighting dome — no catcher, mirror, or added light source — that receives shadows and reflects its surroundings. **Ground Appearance** provides a color chooser for the gray-floor mode plus one shared **Reflection strength** slider used by both HDRI-mirror and gray-floor modes; settings are saved and changing a control rebuilds the active ground. At full strength, the gray floor is nearly perfectly smooth and the archglass Fresnel reflectance approaches a mirror; lower strengths retain a gradual glossy/transmissive appearance. The disk size adapts to the camera view: it computes where the view ray hits Z=0 and sizes the radius to cover the full view frustum with a 3× safety margin, ensuring the disk edge is never visible. HDRI height selects the same vertically shifted source for the ground texture, while rotation rebuilds its lat-long UVs with the lighting-dome yaw. The disk is invisible from below, centered between the streamed mesh bounds and the view intersection point, rebuilt on each apply and on HDRI file, background, and alignment changes; in AO clay mode it is a white matte floor.

Run `python control_upload_test.py` in this directory to verify the interface: it checks the frame parsing offline, then launches the controller on a private port and exercises camera control, both mesh protocols, auto-apply, film saving, and shutdown. Pass `--offline` to skip the live render run.

## Generated files

Film outputs, `render.log`, and `camera_controller_settings.json` are local artifacts and are not source files to commit.
