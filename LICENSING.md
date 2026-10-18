# BOP-1 licensing

BOP-1 is free software under the GNU General Public License, version 3 only (`GPL-3.0-only`, full text in
`LICENSE`). It is built on CRT-4 and LCD-4, which are built on IND-1, AMB-1 and Felucca's platform layer
(through X0X and FoMni), and keeps their licence. If you distribute BOP-1, or firmware derived from it, you
must give your recipients its complete corresponding source under the same licence (`tools/make_pages.py`
puts a tarball of the source on the site it makes, next to the firmware).

## Where the code comes from

| What | Origin | Licence |
| --- | --- | --- |
| Platform: `firmware/hal/`, `firmware/loader/`, `firmware/src/{libc,lcd,gfx,usb,storage,ota,midi_uart}.c`, `tools/` (build, package, install, rescue), `web/fm1*.js` | [Felucca](https://github.com/hugelton/Felucca), Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments, as changed for [X0X](https://github.com/charlesvestal/fm1-x0x), FoMni and [AMB-1](https://github.com/charlesvestal/fm1-amb), then IND-1, LCD-4, CRT-4 and BOP-1 | GPL-3.0-only |
| `firmware/src/app/{panel,plat_fm1,main_fm1}.c`, the input and knob code in `ui.c`, `host/bop_host.c`, `web/emu/`, `web/bop1_installer.html`, `tools/{make_pages,manual_html,gen_manual_figures,fm1_status}.py` | adapted from CRT-4, LCD-4, IND-1 and AMB-1 (themselves from X0X, from Felucca's `panel.c`, `main.c`, `audio.c`) | GPL-3.0-only |
| In `firmware/src/dsp/`: the command queue, the clock and transport, MIDI clock, the sine table, the noise, polyBLEP and the state-variable filter (`bop_dsp.h`), the drum voices of `bop_drums.c` (kick, snare, clap, hat, tom, cowbell) and the plate reverb and ping-pong echo of `bop_fx.c` | from CRT-4 and LCD-4 (the plate after J. Dattorro, "Effect Design, Part 1", JAES 1997) | GPL-3.0-only |
| The idea of a browser emulator for FM-1 firmware (`web/emu/`) | [Groove OS](https://www.groove-os.com/) by Peter Gombos; no code from it | credit only |
| Everything else in `firmware/src/{app,dsp}` (the voice, the funny sounds, the tricks, the limiter, the generator, the friends and their sounds, the styles, the doors, the friends' drawings, the places, the games, the lessons, the grown-ups' page), `tools/make_printables.py`, `tests/` | BOP-1 | GPL-3.0-only |

**Inspiration.** Children's synthesizers such as Blipblox (Playtime Engineering) inspired BOP-1's ideas: music
at power-on, generated melodies, nothing that sounds wrong, lights in time with the music, a toy that grows
into a synth. Only ideas: BOP-1 contains no code, sounds, names, characters or artwork of theirs.

**The tunes.** The games' tunes (Twinkle Twinkle Little Star, Mary Had a Little Lamb, Hot Cross Buns, Frère
Jacques, Row Row Row Your Boat, Old MacDonald Had a Farm, Ode to Joy, Happy Birthday to You) are traditional
or in the public domain; "Happy Birthday to You" has been in the public domain in the United States since 2016.

## Third-party material

| What | Licence | Where |
| --- | --- | --- |
| Fredoka (The Fredoka Project Authors), the screen's rounded face | SIL OFL 1.1 | `assets/fonts/Fredoka[wdth,wght].ttf`, `assets/fonts/Fredoka-OFL.txt` |
| JieLi AC79 SDK: `uboot.boot`, `cfg_tool.bin`, `eq_cfg_hw.bin` are read from your SDK checkout at build time and placed in the package; no SDK files are in this tree | Apache-2.0 | <https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK> |

## Trademarks

"Felucca" and "Hügelton Instruments" are names of Hügelton Instruments. "M-VAVE" and "FM-1" are trademarks
of their respective owners, as is "Blipblox" of Playtime Engineering. BOP-1 is independent firmware; it is
not affiliated with, endorsed by or supported by any of them.

## Radio

BOP-1 never enables the Bluetooth / Wi-Fi radio of the hardware.
