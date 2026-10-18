#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# The manual's screenshots, from the simulator (tests/manual.bop), into docs/img; then the panel figure
# (tools/gen_manual_figures.py) with the Play door's screen in it; the site's three (play, make, lab).
#   tools/manual_shots.sh
set -e
cd "$(dirname "$0")/.."
sh host/build_host.sh >/dev/null
rm -rf build/manual
mkdir -p build/manual docs/img
build/host/bop_host tests/manual.bop build/manual >/dev/null
cp build/manual/*.png docs/img/
cp docs/img/screen-play.png docs/img/play.png
cp docs/img/screen-make-beats.png docs/img/make.png
cp docs/img/screen-lab-filter.png docs/img/lab.png
python3 -c 'import sys; from PIL import Image; [Image.open(f).save(f, optimize=True) for f in sys.argv[1:]]' docs/img/*.png
python3 tools/gen_manual_figures.py docs/img/screen-play.png
ls docs/img
