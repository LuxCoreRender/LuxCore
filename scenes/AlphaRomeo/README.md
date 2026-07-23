# Alpha Romeo interactive renderer

`camera_controller.py` runs the Alpha Romeo scene in a single Tk window. Start it from this directory after building the Release `pyluxcore` target:

```text
python camera_controller.py
```

## Image pipeline and HDRI

`ModoAlphaRomeo.cfg` uses the Reinhard02 tone mapper for both the raw and OIDN image pipelines. Keep their values aligned:

- `prescale = 1.0`
- `postscale = 0.55`
- `burn = 8.0`

These settings preserve HDRI and clear-coat detail while limiting highlight clipping. `ModoAlphaRomeo.scn` uses `hdre_055.hdr` with an infinite-light gain of `0.05 0.05 0.05`; the explicit side and bulb emissions are reduced to about `4` so they do not overpower the environment.

The controller's Exposure control updates the Reinhard `prescale` for both pipelines and restarts rendering, so its changes apply consistently to raw and denoised output.

## Renderer selection

The controller starts with `PATHCPU`, the Embree accelerator, and CPU film processing. The current NVIDIA configuration can allow `PATHOCL.Start()` to return successfully, then reject generated PTX asynchronously with `CUDA_ERROR_UNSUPPORTED_PTX_VERSION`; the background render thread stops and would otherwise leave a permanently black viewport.

The render-window title displays `PATHCPU`. CPU rendering is expected to be slower, but it is reliable. Restore `PATHOCL` only after resolving the NVIDIA driver/NVRTC PTX compatibility issue.
## Stopping and restarting

Select **Stop Rendering** to halt refinement and retain the displayed film for saving. The same button changes to **Start Rendering**; selecting it creates a fresh session with the current camera, resolution, and exposure settings.
## Saved settings

The controller saves the camera target, orbit, distance, exposure, Auto OIDN delay, and selected render resolution to `camera_controller_settings.json`. These values are restored when the controller starts again.

## Generated files

Film outputs, logs, and files such as `Reinhard_HDRI_Verification.png` and `camera_controller_settings.json` are local artifacts and are not source files to commit.
