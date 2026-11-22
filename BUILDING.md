# Building BOP-1

The build makes three files in `build/`:

| File | What |
| --- | --- |
| `bop1.bin` | the firmware app |
| `loader/ota.bin` | the update loader |
| `bop1.fwsc` | the installable package (app + loader) |

## Prerequisites (macOS)

- Python 3 with Pillow: `pip3 install Pillow` (it renders the font, `tools/gen_font.py`)
- A C compiler for the computer itself (`cc`: Xcode's command line tools on a Mac; gcc on Linux) for the
  tests and the simulator.
- Docker Desktop. The JieLi toolchain is Linux x86-64 only; the build runs each tool in a
  `linux/amd64` `debian:bookworm-slim` container (Rosetta on Apple silicon). Keep the source
  tree in a folder Docker can share, e.g. under `/Users`.
- The JieLi Linux toolchain (clang 4.0.1 for pi32v2, from JieLi's package server):

  ```
  tools/get_toolchain.sh            # installs to ~/.jieli/toolchain
  ```

- Three files of the JieLi AC79 SDK (Apache-2.0): `cpu/wl82/tools/uboot.boot`, `cfg_tool.bin` and
  `cfg/eq_cfg_hw.bin`. They are not part of this tree; this fetches just them, from JieLi's repository or a
  GitHub mirror of it, and keeps them only if their SHA-256 is the one the build expects:

  ```
  tools/get_sdk_files.sh            # into ~/fw-AC79_AIoT_SDK
  ```

- Node.js and Emscripten (optional: the browser emulator and its test).

On Linux x86-64 the toolchain runs natively and Docker is not needed.

## Build

```
./build.sh
```

`JIELI_TOOLCHAIN` and `AC79_SDK` override the default locations (`~/.jieli/toolchain`, `~/fw-AC79_AIoT_SDK`).

`./build.sh --release 1.0` makes a release build; the package is `build/bop1-1.0.fwsc`, its identity
`FM-1_61000100` (`FM-1_61` + major, minor, patch, two digits each; a development build is `FM-1_610`).

`./build.sh --bench` makes a development build (`build/bop1-bench.fwsc`) that also plays test songs on request,
to measure the audio load on the FM-1 itself. Every build answers the load query: send `F0 7D 42 31 01 F7`
and the FM-1 replies `F0 7D 42 31 01` with these, each as five 7-bit groups:
- the load (%), the slowest render (µs, of the 5805 µs a 256-frame block has), late renders and renders
- the bench flag
- the time each part of the engine took (the sequencer, the beats, the bass, the tune, the sparkle, the
  sends, the output, the rest)
- the core's clock, the transport, the steps and the MIDI messages in
- the screen's frames, the time they took and the slowest
- the battery divider's ADC reading, and the tune's voices (the governor)

`F0 7D 42 31 03 F7` clears them; `04 s` leaves the screen undrawn for s seconds; `05` reports the store;
`07` what plays. A bench build also takes:

| SysEx | What it does |
|---|---|
| `F0 7D 42 31 02 k F7` | Plays test song k. 0: the heaviest (every pad on every step, chords on every step, every effect up). 1: the first style's song. |
| `06 p` | Plays (1) or stops (0). |
| `0A` | Times each part of a screen frame (ticks of 24 MHz), and the wait for the LCD's transfers. |
| `0B` | The slowest render since `03`: its parts, then its whole. |

`python3 tools/fm1_status.py load|play|store|clock|bench K` asks these for you (needs `mido` and
`python-rtmidi`).

Build option: `EN_CDC=1` adds Felucca's USB serial function (off by default: one plain MIDI interface).

The build also generates `build/gen/felucca_font.h`, the font (five sizes of Fredoka). It fails if a
soft-double routine is linked (a `double` crept in) and checks the image, RAM and pool sizes and that
registers are touched only in `hal/`.

## Tests

```
tests/run_tests.sh
```

Runs, on the build machine: the maths library against libm; the instrument (`tests/host/bop_test.c`: every
style makes a valid song, the same seed the same song, the tune's strong beats on its chords and every note in
the key's scale; the scale lock; the output never above the volume cap at any MASTER, on the heaviest song,
with every trick; resonance held back in the safe range; every friend on every layer and every pad sounds
and goes quiet; the tricks; Loop me and Make's recording, in time and without double hits; MIDI clock in and
out); flash storage; Felucca's update-path tests against `build/bop1.fwsc`; the browser build (if Emscripten
is there); and the whole app in the simulator (`tests/scenarios/*.bop`: Play, Make, Lab and the lessons, the
games, the grown-ups' page, persistence, other firmware's settings, the sleep timer and idle, the volume cap
and the flash rule), with its screenshots in `build/scenarios/`.

The flash rule is measured: the simulator keeps each frame's mean luminance and counts flashes (a rise and a
fall of 10 % or more of full) in any second, and the whole-panel flashes of the LEDs (more than half of them
changing at once); a scenario fails at four or more.

`host/build_host.sh` builds the simulator alone; `build/host/bop_host SCRIPT OUTDIR` runs one script (the
command list is at the top of `host/bop_host.c`).

- `build/host/bop_test DIR` also writes a WAV of every style and every friend into DIR, and times the render.
- `tools/manual_shots.sh`: the manual's screenshots (`tests/manual.bop`) into `docs/img`, and the panel
  drawing (`tools/gen_manual_figures.py`). `tools/readme_gif.py`: the README's animation.
- `tools/make_printables.py DIR`: the sticker overlay, the lesson cards, the quick guides.

## Install

From the command line (needs `pip3 install mido python-rtmidi`):

```
python3 tools/fm1_install.py build/bop1.fwsc
python3 tools/fm1_install.py --info          # identity of the connected FM-1
```

Installing firmware is at your own risk. Hold OCT− and OCT+ for 5 seconds for Felucca's update mode. If the
FM-1 no longer starts but reaches the chip's update mode (4C4A:8057 on USB), `bash tools/fm1_rescue.sh` puts
stock firmware back from a Mac; otherwise recovery needs
[FM-1-transporter](https://github.com/kurogedelic/FM-1-transporter).

BOP-1 keeps its settings at 0xFC000 and its songs, My Friends and sticker book at 0x97000..0xB0FFF; installing
it replaces what another firmware kept there (and theirs, BOP-1's).

## On GitHub: builds, releases, the site

Three workflows in `.github/workflows/` (they work the same in a fork):

- **Build and test** (every push and pull request): the toolchain and the SDK files fetched (and cached), the
  firmware built, `tests/run_tests.sh` run (with the browser build), the package kept as an artifact.
- **Release** (a tag `vX.Y.Z`): the tests, then `./build.sh --release X.Y.Z` (identity `FM-1_61XXYYZZ`, the
  version on the FM-1's screen), and a GitHub release with `bop1-X.Y.Z.fwsc`, its notes taken from
  `CHANGELOG.md`.
- **Pages** (after a release, or by hand): the site for the latest release on GitHub Pages: the landing page,
  the browser emulator, the web installer with the release's package, the manual, the printables, and the
  complete source as a tarball (as the GPL asks). In a fork, set **Settings → Pages → Source** to
  **GitHub Actions** first.

To release: add the version's section to `CHANGELOG.md`, then `git tag vX.Y.Z && git push origin vX.Y.Z`.

The same by hand:

1. `./build.sh --release X.Y.Z`.
2. `web/emu/build.sh` (needs Emscripten) for the browser emulator.
3. `[BOP1_REPO=https://...] python3 tools/make_pages.py build/bop1-X.Y.Z.fwsc X.Y.Z OUT_DIR`: the site in a
   static folder, for any web server. The web installer needs it served over HTTPS (Web MIDI). The manual's
   source is `docs/MANUAL.md` (`tools/manual_html.py` turns it into the page).
