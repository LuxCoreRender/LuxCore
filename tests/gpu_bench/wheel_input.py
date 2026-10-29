"""Build an unrepaired pyluxcore wheel: fork pyd, stock __init__ body, no vendored libs."""
import base64, hashlib, re, sys, zipfile

src_whl, stock_init, out_whl = sys.argv[1:4]
init = open(stock_init, encoding="utf-8").read()
init = re.sub(r"\n# start delvewheel patch.*?# end delvewheel patch\n", "\n", init, flags=re.S)
assert "delvewheel" not in init

with zipfile.ZipFile(src_whl) as zin, zipfile.ZipFile(out_whl, "w", zipfile.ZIP_DEFLATED) as zout:
    record_name = next(n for n in zin.namelist() if n.endswith(".dist-info/RECORD"))
    rows = []
    for info in zin.infolist():
        name = info.filename
        if name.startswith("pyluxcore.libs/") or name == record_name or name.startswith(".load-order") or name.endswith(".dist-info/DELVEWHEEL"):
            continue
        data = init.encode() if name == "pyluxcore/__init__.py" else zin.read(name)
        zout.writestr(name, data)
        digest = base64.urlsafe_b64encode(hashlib.sha256(data).digest()).rstrip(b"=").decode()
        rows.append(f"{name},sha256={digest},{len(data)}")
    rows.append(f"{record_name},,")
    zout.writestr(record_name, "\n".join(rows) + "\n")
print("wrote", out_whl)
