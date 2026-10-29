"""Copy an addon zip, replacing the pyluxcore wheel."""
import sys, zipfile

src_zip, wheel, out_zip = sys.argv[1:4]
target = "blendluxcore/wheels/pyluxcore-2.11.2-cp311-cp311-win_amd64.whl"
with zipfile.ZipFile(src_zip) as zin, zipfile.ZipFile(out_zip, "w") as zout:
    for info in zin.infolist():
        if info.filename == target:
            continue
        zout.writestr(info, zin.read(info.filename), compress_type=info.compress_type)
    zout.write(wheel, target, compress_type=zipfile.ZIP_STORED)
print("wrote", out_zip)
