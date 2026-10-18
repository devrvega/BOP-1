#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Make the BOP-1 site (a static folder: GitHub Pages or any web server serves it as it is):

  index.html                  what BOP-1 is, and the ways in: try, install, manual, printables, download, source
  img/                        screenshots (docs/img)
  emu/                        BOP-1 in the browser (build/emu, from web/emu/build.sh)
  install/index.html          the web installer (web/bop1_installer.html, with fm1pkg.js, fm1ota.js and
                              the package's metadata inlined; Chrome or Edge, Web MIDI)
  manual/index.html, img/     the manual (tools/manual_html.py, from docs/MANUAL.md)
  print/                      the printables (tools/make_printables.py): the sticker overlay, the lesson cards,
                              the grown-ups' quick guide, the children's picture quick-start
  firmware/bop1-VERSION.fwsc  the package the installer writes; also the download
  source/                     the complete source (GPL-3.0: whoever gets the firmware gets the source),
                              as a tarball of this tree; and BOP1_REPO, if set, the repository's address

  [BOP1_REPO=https://...] tools/make_pages.py build/bop1-0.1.fwsc 0.1 OUT_DIR

The package must be a release build (./build.sh --release X.Y): its identity, FM-1_61XXYYZZ, is what
the installer checks the download against and what the FM-1 reports after the install."""
import html
import json
import os
import re
import shutil
import subprocess
import sys
import tarfile
from pathlib import Path

SRC = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(SRC / "web"))
from make_site import product_of, strip_module  # noqa: E402  (Felucca's: the package format)

REPO = os.environ.get("BOP1_REPO", "")

LANDING = """<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>BOP-1</title>
<meta name="description" content="A music and synthesis toy for children on the M-VAVE FM-1: switch it on and music plays, nothing sounds wrong, every knob changes a friend you can see; then beats, loops and songs; then a real synth by its real names, with lessons and games.">
<link rel="icon" href="data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 16 16'%3E%3Crect width='16' height='16' rx='4' fill='%2314183a'/%3E%3Ccircle cx='8' cy='8.5' r='5.5' fill='%234dafff'/%3E%3Ccircle cx='6' cy='7.5' r='1.3' fill='%23fff6e2'/%3E%3Ccircle cx='10' cy='7.5' r='1.3' fill='%23fff6e2'/%3E%3C/svg%3E">
<link rel="preconnect" href="https://fonts.googleapis.com">
<link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
<link rel="stylesheet" href="https://fonts.googleapis.com/css2?family=Fredoka:wght@400;500;600&display=swap">
<style>
/* BOP-1's colours: the screen's night blue, the friends' eight colours */
:root { --paper: #14183a; --sheet: #24245a; --ink: #fff6e2; --muted: #b4b4e0; --rule: #34347a;
        --f1: #4dafff; --f2: #5ce452; --f3: #ff9020; --f4: #9ccede; --f5: #9938ff; --f6: #ffe060; --f7: #28e8e8; --f8: #ff9acb;
        color-scheme: dark; }
body { background: var(--paper); color: var(--ink); margin: 0; padding: 40px 16px 64px; font: 17px/1.6 Fredoka, "Helvetica Neue", Helvetica, Arial, sans-serif; }
main { max-width: 46rem; margin: 0 auto; }
h1 { font: 600 3.4rem/1 Fredoka, sans-serif; letter-spacing: .02em; margin: 0; }
.dots { display: flex; gap: 8px; margin: .9rem 0 0; } .dots span { width: 22px; height: 22px; border-radius: 50%; }
.sub { color: var(--muted); margin: .8rem 0 1.4rem; font-size: 1.15rem; }
.lede { font-size: 1.12rem; }
.shots { display: flex; flex-wrap: wrap; gap: 12px; margin: 1.6rem 0; }
.shots img { width: calc((100% - 24px) / 3); min-width: 150px; height: auto; border-radius: 10px; }
h2 { font-size: 1.2rem; font-weight: 600; margin: 2.2rem 0 .6rem; padding-top: .6rem; border-top: 1px solid var(--rule); }
.ways { display: grid; grid-template-columns: repeat(auto-fit, minmax(13rem, 1fr)); gap: 12px; margin: 1.6rem 0; }
.ways a { display: block; padding: .8rem 1rem; background: var(--sheet); border: 1px solid var(--rule); border-radius: 14px; text-decoration: none; color: var(--ink); }
.ways a:first-child { background: var(--f6); color: #14183a; }
.ways a strong { display: block; font-weight: 600; } .ways a span { display: block; font-size: .92rem; opacity: .85; }
.ways a:hover, .ways a:focus-visible { border-color: var(--f1); outline: none; }
.v { font-style: normal; font-weight: 400; opacity: .8; }
table.doors { border-collapse: collapse; width: 100%; font-size: .97rem; }
table.doors td { padding: .45rem .5rem; border-bottom: 1px solid var(--rule); vertical-align: top; }
table.doors td:first-child { font-weight: 600; white-space: nowrap; }
.small { color: var(--muted); font-size: .9rem; }
a { color: var(--ink); text-decoration-color: var(--f1); text-decoration-thickness: 2px; text-underline-offset: 3px; }
code { font-size: .88em; }
</style>
</head>
<body>
<main>
<h1>BOP-1</h1>
<div class="dots"><span style="background:var(--f1)"></span><span style="background:var(--f2)"></span><span style="background:var(--f3)"></span><span style="background:var(--f4)"></span><span style="background:var(--f5)"></span><span style="background:var(--f6)"></span><span style="background:var(--f7)"></span><span style="background:var(--f8)"></span></div>
<p class="sub">Music and synthesis for children, on the M-VAVE FM-1</p>
<p class="lede">Switch it on and music plays. Nothing a child can press sounds wrong, and every knob changes something
they can see: a friend whose body is the sound. Its shape is the waveform, its mouth the filter, its hop the
envelope, its wobble the LFO, its size the pitch, its place the echo. The same toy grows with the child: from
pressing things (age 3), to beats, loops and songs (6 to 8), to a real subtractive synth by its real names (9 to 12
and up), with lessons and games along the way.</p>
<div class="shots">
  <img src="img/play.png" width="240" height="240" alt="BOP-1's Play door: Bloop, a round blue friend, singing in a bedroom at night, the band beside it">
  <img src="img/make.png" width="240" height="240" alt="BOP-1's Make door: the beats' sixteen steps, four layers and parts A to D">
  <img src="img/lab.png" width="240" height="240" alt="BOP-1's Lab: the signal flow, a scope showing the filtered wave, and the knobs by their real names">
</div>
<nav class="ways" aria-label="Get BOP-1">
  <a href="emu/"><strong>Try it in the browser</strong><span>The same code the FM-1 runs, with sound. Mouse, touch or keyboard; no FM-1 needed.</span></a>
  <a href="install/"><strong>Install <em class="v">v__VERSION__</em></strong><span>From Chrome or Edge, with the FM-1 connected by USB. Nothing to install on the computer.</span></a>
  <a href="manual/"><strong>Manual</strong><span>For grown-ups: getting started, the four doors, the grown-ups' page, safety.</span></a>
  <a href="print/"><strong>Printables</strong><span>Stickers for the buttons and knobs, the ten lesson cards for a class, a quick guide, a picture quick-start.</span></a>
  <a href="firmware/__PKG__"><strong>Download <em class="v">__PKG__</em></strong><span>For the command-line installer: <code>python3 tools/fm1_install.py __PKG__</code></span></a>
  <a href="source/"><strong>Source</strong><span>GPL-3.0. Built on Felucca by Hügelton Instruments, by way of X0X, AMB-1, IND-1, LCD-4 and CRT-4.</span></a>
</nav>

<h2>The four doors</h2>
<table class="doors">
<tr><td>Play <span class="small">3+</span></td><td>Music already playing. The white keys sing in the scale; the black keys are drums and funny sounds. Turn the knobs and the friend changes shape, opens its mouth, wobbles, and the room grows from a bedroom to outer space. Surprise! makes a new song; magic tricks while a button is held; Loop me records what you play.</td></tr>
<tr><td>Make <span class="small">6–8</span></td><td>Four layers like four friends in a band: Beats, Bass, Tune, Sparkle. Tap steps, record in time, ask for help, build parts A B C D into a song over happy, sad, brave or sleepy chords. Eight songs kept.</td></tr>
<tr><td>Lab <span class="small">9+</span></td><td>The real synth: two oscillators, a filter, an envelope, an LFO, an arpeggiator, effects; a signal-flow picture and a live scope. Each knob shows its real name and the toy's word. Sounds made here become My Friends and play in Play and Make. Ten lessons, each with a try-it.</td></tr>
<tr><td>Games <span class="small">all</span></td><td>Follow the lights (public-domain tunes), Copy me, High or low, Beat copy, Sound match, Name the friend. Stars fill a sticker book; nothing is ever taken away.</td></tr>
</table>
<h2>Made for children</h2>
<p>A grown-up chooses which doors show (Play alone for a three-year-old: nobody gets lost), the volume cap, calm
lights, the screen's brightness, a sleep timer for bedtime, and whether music starts at power-on. An always-on
limiter keeps the output under the cap; resonance and the highest notes are held back where children play;
flashes are limited to three a second (and none in calm mode). Nothing is deleted with a tap.</p>
<p class="small">To go back to the official firmware, use the installer's "Back to the stock firmware", or
M-VAVE's updater, M-UPGRADE, from <a href="https://www.m-vave.com/download">m-vave.com/download</a>.
M-VAVE and FM-1 are trademarks of their owners. BOP-1 is independent: it is not affiliated with M-VAVE, and its
friends, music, sounds and pictures are its own. Children's synths such as Blipblox were an inspiration, in ideas
only.</p>
</main>
</body>
</html>
"""

SOURCE = """<!doctype html>
<html lang="en">
<head><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1"><title>BOP-1 source</title>
<style>body { background: #14183a; color: #fff6e2; font: 17px/1.6 "Helvetica Neue", Helvetica, Arial, sans-serif; margin: 48px 16px; }
main { max-width: 42rem; margin: 0 auto; } a { color: #fff6e2; text-decoration-color: #4dafff; text-decoration-thickness: 2px; }</style></head>
<body><main><h1>BOP-1 source</h1>
<p>BOP-1 is free software under the GNU General Public License, version 3 only. The complete source of v__VERSION__:
<a href="__TAR__">__TAR__</a>.__REPOLINE__</p>
<p><a href="../">Back to BOP-1</a></p></main></body></html>
"""


def source_tarball(dest, version):
    """the tree as it is (no build output, no caches): what the GPL asks to give with the firmware"""
    name = f"bop-1-{re.sub(r'[^A-Za-z0-9.-]', '-', version)}-src"
    skip = {"build", "__pycache__", ".git", ".DS_Store", ".serena"}

    def keep(ti):
        return None if set(Path(ti.name).parts) & skip else ti
    with tarfile.open(dest / f"{name}.tar.gz", "w:gz") as t:
        t.add(SRC, arcname=name, filter=keep)
    return f"{name}.tar.gz"


def main(pkg, version, out):
    pkg, out = Path(pkg), Path(out)
    raw = pkg.read_bytes()
    product = product_of(raw)
    if not re.fullmatch(r"FM-1_61\d{6}", product):
        raise SystemExit(f"{pkg}: identity {product!r}: make a release build (./build.sh --release X.Y)")
    if b"FELUCCA-LOADER-1" not in raw:
        raise SystemExit(f"{pkg}: no update loader in it")
    name = f"bop1-{re.sub(r'[^A-Za-z0-9.-]', '-', version)}.fwsc"
    if out.exists():
        shutil.rmtree(out)
    for d in ("install", "manual", "firmware", "source"):
        (out / d).mkdir(parents=True)
    shutil.copy(pkg, out / "firmware" / name)
    # the installer
    page = (SRC / "web" / "bop1_installer.html").read_text(encoding="utf-8")
    lib = strip_module((SRC / "web" / "fm1pkg.js").read_text(encoding="utf-8")) + "\n" + \
        strip_module((SRC / "web" / "fm1ota.js").read_text(encoding="utf-8"))
    meta = json.dumps({"version": version, "product": product, "pkg": "../firmware/" + name})
    for mark in ("/*LIB*/", "/*META*/"):
        if page.count(mark) != 1:
            raise SystemExit(f"bop1_installer.html must contain {mark} once")
    (out / "install" / "index.html").write_text(page.replace("/*LIB*/", lib).replace("/*META*/", meta), encoding="utf-8")
    # the manual (a complete page; its images beside it)
    subprocess.run([sys.executable, str(SRC / "tools" / "manual_html.py"), str(SRC / "docs" / "MANUAL.md"),
                    str(out / "manual" / "index.html")], check=True, capture_output=True)
    shutil.copytree(SRC / "docs" / "img", out / "manual" / "img")
    # the landing page and its screenshots
    shutil.copytree(SRC / "docs" / "img", out / "img")
    (out / "index.html").write_text(LANDING.replace("__VERSION__", html.escape(version)).replace("__PKG__", name),
                                    encoding="utf-8")
    # the source, for the GPL
    tar = source_tarball(out / "source", version)
    repo = f' The repository: <a href="{html.escape(REPO)}">{html.escape(REPO)}</a>.' if REPO else ""
    (out / "source" / "index.html").write_text(SOURCE.replace("__VERSION__", html.escape(version)).replace("__TAR__", tar)
                                               .replace("__REPOLINE__", repo), encoding="utf-8")
    # the printables
    subprocess.run([sys.executable, str(SRC / "tools" / "make_printables.py"), str(out / "print")], check=True,
                   capture_output=True)
    emu = SRC / "build" / "emu"
    if not (emu / "bop1.wasm").exists():
        raise SystemExit("no build/emu/bop1.wasm: run web/emu/build.sh (needs Emscripten)")
    shutil.copytree(emu, out / "emu")
    (out / ".nojekyll").write_text("")
    print(f"site: {out}: index.html, install/ ({product}), manual/, print/, emu/, firmware/{name} ({len(raw)} B)")


if __name__ == "__main__":
    if len(sys.argv) != 4:
        sys.exit(__doc__)
    main(*sys.argv[1:4])
