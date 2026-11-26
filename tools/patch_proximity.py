#!/usr/bin/env python3
"""ITACHI: neutralise ColorOS' proprietary proximity interception so AOSP's DisplayPowerProximityStateController
uses the real proximity sensor.  (c) @yooitachiijod
Works on baksmali output of the ROM's oplus-services.jar.  Always run --scan first.
  python3 patch_proximity.py <smali_dir> --scan        list candidate methods
  python3 patch_proximity.py <smali_dir>               patch (writes .itachi.bak next to each file)"""
import re, sys, pathlib
TARGETS = {  # method name -> forced result
    "registerPSensor": "false", "applyOplusProximitySensorLocked": "false",
    "getUseProximityForceSuspendState": "false", "isUseProximityForceSuspendStateChanged": "false",
    "setUseProximityForceSuspend": "void",
}
CLASSES = ("OplusDisplayPowerControllerExtImpl", "OplusProximitySensorScreenOnOff")
rx = re.compile(r"(\.method [^\n]*? (\w+)\(([^)]*)\)(\S+)\n)(.*?)(\.end method)", re.S)
root = pathlib.Path(sys.argv[1]); scan = "--scan" in sys.argv; changed = 0
for f in root.rglob("*.smali"):
    if not any(c in f.name for c in CLASSES): continue
    src = f.read_text(); out = src; hits = []
    for m in rx.finditer(src):
        name, args, ret = m.group(2), m.group(3), m.group(4)
        if name in TARGETS and "abstract" not in m.group(1) and "native" not in m.group(1):
            hits.append((name, args, ret))
            if scan: continue
            static = " static " in m.group(1)
            if ret == "Z": body = "    .locals 1\n    const/4 v0, 0x0\n    return v0\n"
            elif ret == "V": body = "    .locals 0\n    return-void\n"
            else: continue
            out = out.replace(m.group(0), m.group(1) + body + m.group(6))
    for h in hits: print(f"{'FOUND ' if scan else 'PATCH '} {f.name}: {h[0]}({h[1]}){h[2]}")
    if hits and not scan and out != src:
        pathlib.Path(str(f) + ".itachi.bak").write_text(src); f.write_text(out); changed += 1
print("files changed:", changed)
