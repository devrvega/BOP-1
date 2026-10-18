# Make it yours

BOP-1 is free software (GPL-3.0). You're welcome to fork it, change it, rename it and release your own
version; no need to ask. You can also send changes back here. Both are described below.

## What the licence asks

If you give someone a firmware built from this code (a release, a `.fwsc` in a chat, a page with an
installer), give them its complete source under the same licence, and keep the copyright notices and
`LICENSE` that are in the files. That's all. The release workflow and the site generator already do it:
every release's tag is its source, and the site carries a tarball of it.

## Build it, try it, test it

Everything runs on GitHub: a fork builds and tests every push (**Actions → Build and test**), and the firmware
is attached to the run as an artifact to download. On your own machine:

| | |
|---|---|
| `tests/run_tests.sh` | the tests (the instrument, the platform, the app in a simulator, the browser build if Emscripten is there) |
| `./build.sh` | the firmware, `build/bop1.fwsc` (needs the JieLi toolchain and three SDK files; see [BUILDING.md](BUILDING.md)) |
| `build/host/bop_host SCRIPT OUT` | the whole app on your computer from a script, with the screen as PNGs and the sound as WAV (`tests/scenarios/*.bop`) |
| `web/emu/build.sh` | the browser emulator (needs Emscripten) |
| `python3 tools/fm1_install.py build/bop1.fwsc` | install it over USB |
| `python3 tools/fm1_status.py load` | the audio load on the FM-1 itself, and where the time goes |

On a Mac the build runs the JieLi tools in Docker; on Linux x86-64 (and on GitHub) it runs them directly.
`tools/get_toolchain.sh` and `tools/get_sdk_files.sh` fetch what it needs.

## Where things are

| | |
|---|---|
| `firmware/src/dsp/bop.c` | the commands, the clock, the sequencer, recording, the mix, the render, the governor (one unit, with the files below) |
| `firmware/src/dsp/bop_voice.c` | the voice: two morphing oscillators, the sub, the filter, the envelope, the LFO, glide, the arpeggiator |
| `firmware/src/dsp/bop_drums.c` | the eleven pads: drums and funny sounds |
| `firmware/src/dsp/bop_fx.c` | the echo, the plate, the tricks, the limiter and the volume cap |
| `firmware/src/dsp/bop_gen.c` | the friends' sounds, the scales, the styles, the progressions, the song generator |
| `firmware/src/app/ui.c` | the controls, the doors, the cards, the lights, sleep and autosave |
| `firmware/src/app/{play,make,lab,lessons,games,grownups}.c` | each door, the lessons, the grown-ups' page |
| `firmware/src/app/friends.c`, `draw.c` | the friends' bodies, the places, the icons; the drawing kit |
| `firmware/src/app/project.c` | saving and loading (the song, eight kept songs, My Friends, the sticker book) |
| `firmware/hal/`, `firmware/loader/`, `firmware/src/{usb,storage,ota}.c` | the FM-1 platform (Felucca's) |
| `host/`, `tests/` | the simulator and the tests |
| `web/`, `tools/make_pages.py`, `tools/make_printables.py` | the emulator, the installer, the site, the printables |

## Your own firmware, from this one

To release it under your own name, so that it and BOP-1 can live side by side (and on the
[FM-1 firmware hub](https://thefirmwarehub.com/)):

1. **The name.** Search for `BOP-1` and change what you want: the start screen (`firmware/src/app/main_fm1.c`),
   the USB names (`STR1` and `STR2` in `firmware/src/usb.c`), the docs and the site.
2. **The identity**, what the FM-1 reports and the installers check: `PRODUCT` and the release prefix in
   `tools/build.py`, and the `FELUCCA_ID` default in `firmware/src/main.c`. Pick a prefix nobody uses (BOP-1
   is `FM-1_61…`); the hub's [catalogue](https://thefirmwarehub.com/catalog.json) lists the ones in use.
3. **The saved data's tags**, `GLOB_MAGIC`, `SONG_MAGIC`, `FRND_MAGIC` and `BOOK_MAGIC` in `firmware/src/app/app.h`, so that
   your firmware and BOP-1 never read each other's saves.
4. **Releases:** push a tag `vX.Y.Z`; the Release workflow builds and publishes it. For the site, set
   **Settings → Pages → Source** to **GitHub Actions**; the Pages workflow then publishes it after each
   release.

## Sending changes back

Pull requests are welcome, small or large; there's no form to fill in. It helps if:

- `tests/run_tests.sh` passes (the pull request's checks run it for you);
- new behaviour has a test or a line in a scenario script, and the manual (`docs/MANUAL.md`) says how to use it;
- you say whether you tried it on an FM-1, and what `tools/fm1_status.py load` showed for anything that
  makes sound.

A new friend: its sound in `EN_FRIEND` (`bop_gen.c`, a patch of the voice's numbers and a colour). A new
style: its line in `STYLE` (tempo, chords, friends, the bass's and the sparkle's figures, its beat). A new
game or lesson: `games.c`, `lessons.c`. Keep it kind: nothing a tap destroys, no timers for the youngest, no
flashes beyond three a second, and the safety in the engine (the cap, the limiter, the safe range) untouched.

Some habits of the code: C99, `float` only (the FM-1 has no double-precision hardware, and the build fails if
a `double` gets in), no libm (`fastmath.h` instead), no divisions in per-sample loops, and loops that keep
few values alive (the FM-1's floats share its sixteen registers). The comments say what a thing is for,
in plain words.
