# Alpha Romeo interactive renderer

`camera_controller.py` runs the Alpha Romeo scene in a single Tk window. Start it from this directory after building the Release `pyluxcore` target:

```text
python camera_controller.py
```

## Image pipeline and HDRI

`ModoAlphaRomeo.cfg` uses the Reinhard02 tone mapper for both the raw and OIDN image pipelines. Keep their values aligned:

- `prescale = 1.0`
- `postscale = 0.4`
- `burn = 8.0`

These settings preserve HDRI and clear-coat detail while limiting highlight clipping. `ModoAlphaRomeo.scn` uses `hdre_055.hdr` with an infinite-light gain of `0.05 0.05 0.05`.

The controller's Exposure control updates the Reinhard `prescale` for both pipelines and restarts rendering, so its changes apply consistently to raw and denoised output.

## CUDA PATHOCL fallback

The controller starts with `PATHOCL`. On systems where the NVIDIA driver rejects the generated PTX with `CUDA_ERROR_UNSUPPORTED_PTX_VERSION`, it automatically restarts using `PATHCPU`, the Embree accelerator, and CPU film processing. This prevents the hidden startup exception from leaving a black viewport.

The render-window title displays `PATHOCL` during GPU rendering or `PATHCPU fallback` when this recovery path is active. The CPU fallback is expected to be slower; resolve the NVIDIA driver/NVRTC PTX compatibility issue to restore GPU rendering.

## Generated files

Film outputs, logs, and files such as `Reinhard_HDRI_Verification.png` are local verification artifacts and are not source files to commit.
