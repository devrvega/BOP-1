#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Ask a connected FM-1 running BOP-1 how it's doing, over USB-MIDI (BOP-1's read-only SysEx, F0 7D 42 31 ..
F7, described in firmware/src/app/main_fm1.c). Needs mido with python-rtmidi. Nothing it sends changes the
FM-1's patterns or settings.

  tools/fm1_status.py load [SECONDS]   the audio load: %, the slowest render, late renders, and where the
                                       time goes (sequencer, beats, bass, tune, sparkle, sends, master,
                                       the rest),
                                       and the screen's frames a second, measured over SECONDS (default 5);
                                       play something meanwhile
  tools/fm1_status.py play             what plays: notes counted a layer, the part, the tempo, the step
  tools/fm1_status.py bench K [SECS]   a development build: the heaviest song (0) or a style's (1), its load
  tools/fm1_status.py save             a development build: the song saved while it plays, and what it cost
  tools/fm1_status.py store            the saved objects: their lengths in flash, what is in them
  tools/fm1_status.py clock [SECONDS]  play BOP-1 from here: MIDI start, clock at 120, stop (default 6 s),
                                       and the load while it plays

A render has 5805 us (256 frames at 44.1 kHz); the FM-1 keeps up while the slowest stays under that."""
import sys
import time

import mido

HDR = [0x7D, 0x42, 0x31]
PARTS = ["sequencer", "beats", "bass", "tune", "sparkle", "sends", "master", "the rest"]


def r35(b, i):
    return sum(b[i + k] << (7 * k) for k in range(5))


def port_name():
    name = next((n for n in mido.get_output_names() if "FM-1" in n), None)
    if not name:
        sys.exit(f"fm1_status: no FM-1 MIDI port (ports: {mido.get_output_names()})")
    return name


def ask(i, o, cmd, args=(), reply=True, timeout=2.0, must=True):
    while i.poll():
        pass
    o.send(mido.Message("sysex", data=HDR + [cmd] + list(args)))
    if not reply:
        time.sleep(0.15)                    # the FM-1 takes one SysEx at a time
        return None
    end = time.time() + timeout
    while time.time() < end:
        m = i.poll()
        if m is not None and m.type == "sysex" and list(m.data[:4]) == HDR + [cmd]:
            return list(m.data[4:])
        time.sleep(0.002)
    if not must:
        return None
    sys.exit("fm1_status: no answer (is it BOP-1, and a version that answers this?)")


def load_report(d0, d1, secs):
    renders = max(r35(d1, 15) - r35(d0, 15), 1)
    us = [r35(d1, 25 + 5 * k) / 24.0 / renders for k in range(8)]
    print(f"load {r35(d1, 0)} %, slowest render {r35(d1, 5)} us of 5805, late {r35(d1, 10)}, "
          f"{renders} renders in {secs:.0f} s")
    print("per render: " + ", ".join(f"{p} {u:.0f} us" for p, u in zip(PARTS, us)) + f" = {sum(us):.0f} us")
    if len(d1) >= 100:                                      # the screen
        frames = (r35(d1, 70) - r35(d0, 70)) & 0xFFFFFFFF
        ms = ((r35(d1, 90) - r35(d0, 90)) & 0xFFFFFFFF) / 1000.0
        print(f"screen {frames / secs:.1f} frames/s, {ms / max(frames, 1):.1f} ms a frame, "
              f"the slowest {r35(d1, 95) / 1000.0:.1f} ms")
    if len(d1) >= 110:
        print(f"battery ADC {r35(d1, 100)}, tune voices {r35(d1, 105)}")


def main():
    a = sys.argv[1:] or ["load"]
    with mido.open_input(port_name()) as i, mido.open_output(port_name()) as o:
        time.sleep(0.3)                                     # (on macOS the first message after opening can go
        ask(i, o, 7, timeout=1.0, must=False)               # astray: a question asked for nothing first)
        if a[0] == "save":                                  # a bench build: a save while playing, its cost
            ask(i, o, 3, reply=False)
            d0 = ask(i, o, 1)
            ask(i, o, 8, reply=False)
            time.sleep(1.0)
            d1 = ask(i, o, 1)
            print(f"a save while playing: renders late {r35(d1, 10)}, slowest {r35(d1, 5)} us")
            return
        if a[0] == "bench":
            ask(i, o, 2, [int(a[1]) if len(a) > 1 else 0], reply=False)
            time.sleep(1.0)
            a = ["load"] + a[2:]
        if a[0] in ("load", "clock"):
            secs = float(a[1]) if len(a) > 1 else (5.0 if a[0] == "load" else 6.0)
            ask(i, o, 3, reply=False)                       # the worst and the parts' times from now
            d0 = ask(i, o, 1)
            if a[0] == "clock":
                o.send(mido.Message("start"))
                t0, nxt = time.time(), time.time()
                while time.time() - t0 < secs:
                    if time.time() >= nxt:
                        o.send(mido.Message("clock"))
                        nxt += 60.0 / 120 / 24
                    time.sleep(0.001)
                d1 = ask(i, o, 1)
                o.send(mido.Message("stop"))
            else:
                time.sleep(secs)
                d1 = ask(i, o, 1)
            load_report(d0, d1, secs)
            st = r35(d1, 75)
            print(f"playing {st & 1}, on MIDI clock {st >> 1 & 1}, writing {st >> 2 & 1}; "
                  f"steps {r35(d1, 80)}; MIDI messages in {r35(d1, 85)}")
        elif a[0] == "play":
            d = ask(i, o, 7)
            v = [r35(d, 5 * k) for k in range(11)]
            print(f"notes played (counted mod 256): beats {v[0]}, bass {v[1]}, tune {v[2]}, sparkle {v[3]}")
            print(f"part {'ABCD'[v[4] & 3]}, a chain of {v[5] & 255}, style {v[5] >> 8 & 255}, door {v[5] >> 16 & 255}; "
                  f"its hits {v[6]}; the tune's first note {v[7] & 255}, key {v[7] >> 8}")
            print(f"tempo {v[8] & 0xFFFF}, clock {'internal' if v[8] >> 16 else 'auto'}, trick {v[9] & 255}, "
                  f"step {(v[9] >> 8 & 255) + 1}; volume cap {v[10] & 255} %, safe {v[10] >> 8 & 1}")
        elif a[0] == "store":
            d = ask(i, o, 5)
            names = ["settings", "panel", "the song"] + [f"slot {k + 1}" for k in range(8)] + ["My Friends", "sticker book"]
            for k, name in enumerate(names):
                if 15 * k + 15 > len(d):
                    break
                n, rc, info = r35(d, 15 * k) - 100, r35(d, 15 * k + 5) - 100, r35(d, 15 * k + 10)
                what = ""
                if k == 0 and n > 0:
                    what = f": door {info & 15}, style {info >> 4 & 15}, doors {info >> 8 & 15:04b}, {'valid' if info >> 20 else 'NOT valid'}"
                elif 2 <= k <= 10 and n > 0:
                    what = f": tempo {info & 255}, style {info >> 8 & 15}, {'valid' if info >> 20 else 'NOT valid'}"
                print(f"{name:14} {('%d B' % n) if n >= 0 else 'none':8} last save {'ok' if rc == 0 else rc}{what}")
        else:
            sys.exit(__doc__)


if __name__ == "__main__":
    main()
