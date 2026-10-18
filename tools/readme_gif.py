#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""docs/img/bop1.gif: the README's animation, from the simulator: the Play door, the friend singing and
changing (its shape, its mouth, the place), then the other doors, a frame every 100 ms.

  tools/readme_gif.py        (builds the simulator first; needs Pillow)"""
import subprocess
import sys
import tempfile
from pathlib import Path

from PIL import Image

SRC = Path(__file__).resolve().parents[1]
STEP_MS = 100
# what happens, and for how long: (commands before, frames)
PLAN = [([], 10), (["tapkey w4", "tapkey w6", "tapkey w8"], 8), (["spin K1 13"], 6), (["spin K1 12"], 6),
        (["spin K1 13", "tapkey b6"], 6), (["spin K1 -38", "spin K2 -30"], 6), (["spin K2 30", "spin K4 30"], 8),
        (["turn ALGO 4"], 8), (["tap FX"], 8), (["spin K4 -30", "tap HOME", "tapkey w1", "tapkey w1"], 10),
        (["tap HOME", "tapkey w2", "tapkey w2", "tapkey w0", "key 12 down"], 10), (["key 12 up", "tap HOME", "tapkey w0", "tapkey w0"], 6)]


def main():
    subprocess.run(["sh", str(SRC / "host" / "build_host.sh")], check=True, capture_output=True)
    with tempfile.TemporaryDirectory() as tmp:
        lines, n = ["wait 2600"], 0
        for cmds, frames in PLAN:
            lines += cmds
            for _ in range(frames):
                lines += [f"wait {STEP_MS}", f"shot f{n:03d}.png"]
                n += 1
        script = Path(tmp) / "gif.bop"
        script.write_text("\n".join(lines) + "\n")
        subprocess.run([str(SRC / "build" / "host" / "bop_host"), str(script), tmp], check=True, capture_output=True)
        frames = [Image.open(Path(tmp) / f"f{i:03d}.png").convert("RGB") for i in range(n)]
    out = [f.quantize(colors=96, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE) for f in frames]
    dest = SRC / "docs" / "img" / "bop1.gif"
    out[0].save(dest, save_all=True, append_images=out[1:], duration=STEP_MS, loop=0, optimize=True)
    print(f"{dest}: {n} frames, {dest.stat().st_size // 1024} KB")


if __name__ == "__main__":
    sys.exit(main())
