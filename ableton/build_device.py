#!/usr/bin/env python3
"""Build the WhatKey Max for Live device and install it into your Ableton User Library.

The device references this repo by absolute path, so it's generated per machine.
Re-run this if you move the repo or change Python installs.

Usage:
    python3 ableton/build_device.py            # install to the default User Library
    python3 ableton/build_device.py --out X.amxd
"""
import argparse
import json
import os
import shutil
import struct
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
DEFAULT_OUT = Path.home() / "Music/Ableton/User Library/Presets/Audio Effects/Max Audio Effect/WhatKey.amxd"
WIDTH, HEIGHT = 470, 169


def box(id_, maxclass, rect, presentation_rect=None, **kw):
    b = {"id": id_, "maxclass": maxclass, "patching_rect": rect, **kw}
    if presentation_rect:
        b.update(presentation=1, presentation_rect=presentation_rect)
    return {"box": b}


def line(src, so, dst, di):
    return {"patchline": {"source": [src, so], "destination": [dst, di]}}


def patcher():
    node_js = str(HERE / "whatkey-node.js")
    ui_js = str(HERE / "whatkey-ui.js")
    boxes = [
        box("obj-1", "newobj", [20, 20, 60, 22], text="plugin~", numinlets=2, numoutlets=2,
            outlettype=["signal", "signal"]),
        box("obj-2", "newobj", [20, 300, 60, 22], text="plugout~", numinlets=2, numoutlets=2,
            outlettype=["signal", "signal"]),
        box("obj-3", "live.drop", [200, 20, 96, 156], [4, 5, 96, 159], numinlets=1, numoutlets=2,
            outlettype=["", ""], varname="drop"),
        box("obj-4", "newobj", [200, 190, 100, 22], text="prepend analyze", numinlets=1, numoutlets=1,
            outlettype=[""]),
        box("obj-5", "newobj", [200, 220, 300, 22], text=f'node.script "{node_js}" @autostart 1',
            numinlets=1, numoutlets=2, outlettype=["", ""]),
        box("obj-6", "v8ui", [320, 20, 362, 159], [104, 5, WIDTH - 108, 159], filename=ui_js,
            numinlets=1, numoutlets=1, outlettype=[""], parameter_enable=0),
    ]
    lines = [
        line("obj-1", 0, "obj-2", 0), line("obj-1", 1, "obj-2", 1),
        line("obj-3", 0, "obj-4", 0), line("obj-4", 0, "obj-5", 0), line("obj-5", 0, "obj-6", 0),
    ]
    return {"patcher": {
        "fileversion": 1,
        "appversion": {"major": 9, "minor": 0, "revision": 7, "architecture": "x64", "modernui": 1},
        "classnamespace": "box",
        "rect": [100, 100, 720, 400],
        "openrect": [0, 0, 0, HEIGHT],
        "openinpresentation": 1,
        "devicewidth": float(WIDTH),
        "default_fontsize": 10.0, "default_fontface": 0, "default_fontname": "Arial Bold",
        "boxes": boxes, "lines": lines,
        "description": "Drop an audio file to estimate its musical key.",
        "dependency_cache": [], "autosave": 0,
    }}


def write_amxd(path, patch):
    body = json.dumps(patch, indent=1).encode() + b"\n\0"
    header = b"ampf" + struct.pack("<I", 4) + b"aaaa" + b"meta" + struct.pack("<I", 4) + b"\0\0\0\0"
    path.write_bytes(header + b"ptch" + struct.pack("<I", len(body)) + body)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--out", type=Path, default=DEFAULT_OUT, help="where to write the .amxd")
    args = ap.parse_args()

    if subprocess.run([sys.executable, "-c", "import numpy"], capture_output=True).returncode:
        sys.exit(f"numpy isn't installed for {sys.executable}; run: {sys.executable} -m pip install numpy")
    ffmpeg = shutil.which("ffmpeg")
    if not ffmpeg:
        sys.exit("ffmpeg not found on PATH (try: brew install ffmpeg)")

    # Max doesn't inherit your shell PATH, so record exactly what to run.
    path_dirs = [os.path.dirname(ffmpeg), "/usr/local/bin", "/opt/homebrew/bin", "/usr/bin", "/bin"]
    config = {
        "python": sys.executable,
        "script": str(HERE.parent / "whatkey.py"),
        "path": ":".join(dict.fromkeys(path_dirs)),
    }
    (HERE / "config.json").write_text(json.dumps(config, indent=2) + "\n")

    args.out.parent.mkdir(parents=True, exist_ok=True)
    write_amxd(args.out, patcher())
    print(f"Installed device: {args.out}")
    print("In Live: Browser > User Library > Presets > Audio Effects > Max Audio Effect > WhatKey")


if __name__ == "__main__":
    main()
