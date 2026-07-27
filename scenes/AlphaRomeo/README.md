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

`ModoAlphaRomeo.cfg` uses the Reinhard02 tone mapper for both the raw and OIDN image pipelines. Keep their values aligned:

- `prescale = 1.0`
- `postscale = 0.55`
- `burn = 8.0`

These settings preserve HDRI and clear-coat detail while limiting highlight clipping. `ModoAlphaRomeo.scn` uses `hdre_055.hdr` with an infinite-light gain of `1.0 1.0 1.0`; the explicit side and bulb emissions are reduced to about `4` so they do not overpower the environment.

The controller's Exposure control updates the Reinhard `prescale` for both pipelines and restarts rendering, so its changes apply consistently to raw and denoised output.

## Renderer selection

The controller uses `PATHOCL` with the `OPTIX` accelerator, selecting the CUDA GPU for hardware ray tracing. `opencl.cpu.use = 0`, `opencl.gpu.use = 1`, and `opencl.native.threads.count = 0` ensure it uses the GPU path rather than an OpenCL CPU or native render thread.

The render-window title displays `PATHOCL / OptiX`. Film hardware processing remains disabled because the controller reads raw and OIDN film output on the host. GPU ray tracing was validated with a 320×180 Alpha Romeo render that reached 1,019 passes with every RGB channel populated.
## Changing the HDRI

Drop a `.hdr` file onto the render viewport to replace the infinite-light image. The controller uses `tkinterdnd2`'s native OLE file-drop support, restarts the current render using the dropped HDRI, and preserves the source path in `camera_controller_settings.json`; it does not copy the HDRI into the scene directory.
## Stopping and restarting

Select **Stop Rendering** to halt refinement and retain the displayed film for saving. The same button changes to **Start Rendering**; selecting it creates a fresh session with the current camera, resolution, and exposure settings.
## Saved settings

The controller saves the camera target, orbit, distance, exposure, Auto OIDN delay, and selected render resolution to `camera_controller_settings.json`. These values are restored when the controller starts again.

## Generated files

Film outputs, logs, and files such as `Reinhard_HDRI_Verification.png` and `camera_controller_settings.json` are local artifacts and are not source files to commit.
