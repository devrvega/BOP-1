#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Make BOP-1's printables (a folder of pages that print on A4 or Letter; the browser's print makes a PDF):

  index.html        the list
  stickers.svg      the sticker overlay: a picture for each button and knob (what it does in Play, where the
  stickers.html     youngest are), and the eleven pads for the black keys; print on sticker paper at 100 %
  lessons.html      the ten lessons as cards for a class: what each teaches, its steps, a try-it on the FM-1,
                    an activity without it, the real words
  guide.html        the grown-ups' quick guide (one page)
  quickstart.html   the children's picture quick-start (one page, no reading needed)

  tools/make_printables.py OUT_DIR

The pictures are drawn here in SVG, after the ones on the screen (firmware/src/app/friends.c)."""
import html
import math
import sys
from pathlib import Path

NAVY, INK, CREAM = "#14183a", "#100c22", "#fff6e2"
FRIENDS = [("Bloop", "#4dafff"), ("Pointy", "#5ce452"), ("Zig", "#ff9020"), ("Bit", "#9ccede"),
           ("Wub", "#9938ff"), ("Twinkle", "#ffe060"), ("Bubbles", "#28e8e8"), ("Fizz", "#ff9acb")]
PADS = [("Kick", "#ff5454"), ("Snare", "#ffaa3c"), ("Clap", "#ffe646"), ("Hat", "#78e65a"), ("Tom", "#3cc8be"),
        ("Cowbell", "#5096ff"), ("Boing", "#aa6eff"), ("Laser", "#ff64dc"), ("Pop", "#ffffff"), ("Whoosh", "#8cdcff"),
        ("Ta-da", "#ffcd78")]


# ------------------------------------------------------------------ pictures (SVG, in a 100 x 100 box)
def friend(col, shape="round", mouth=0.5):
    """a friend: its body by shape, eyes, cheeks, a mouth open by `mouth`"""
    if shape == "round":
        body = f'<ellipse cx="50" cy="54" rx="34" ry="32" fill="{col}" stroke="{INK}" stroke-width="5"/>'
    elif shape == "box":
        body = (f'<line x1="50" y1="22" x2="50" y2="10" stroke="{INK}" stroke-width="5"/><circle cx="50" cy="9" r="6" '
                f'fill="#ff78a0" stroke="{INK}" stroke-width="4"/><rect x="18" y="22" width="64" height="62" rx="14" '
                f'fill="{col}" stroke="{INK}" stroke-width="5"/>')
    elif shape == "spiky":
        pts = []
        for i in range(48):
            a = i / 48
            d = min(a, 1 - a)
            w = max(0.0, min(1.0, 1 - (d - 0.05) * 4.5))
            s = (a * 16) % 1
            r = 32 * (0.94 + w * 0.32 * (1 - s))
            pts.append(f"{50 + r * math.sin(2 * math.pi * a):.1f},{55 - r * math.cos(2 * math.pi * a):.1f}")
        body = f'<polygon points="{" ".join(pts)}" fill="{col}" stroke="{INK}" stroke-width="5" stroke-linejoin="round"/>'
    else:
        body = f'<path d="M50 14 L86 80 Q50 92 14 80 Z" fill="{col}" stroke="{INK}" stroke-width="5" stroke-linejoin="round"/>'
    mh = 3 + 16 * mouth
    m = (f'<ellipse cx="50" cy="{66 + mh / 2:.1f}" rx="{8 + 6 * mouth:.1f}" ry="{mh / 2 + 1:.1f}" fill="{INK}"/>' if mouth > 0.15
         else f'<path d="M40 66 Q50 74 60 66" fill="none" stroke="{INK}" stroke-width="4" stroke-linecap="round"/>')
    return (body + f'<circle cx="38" cy="50" r="7" fill="{CREAM}" stroke="{INK}" stroke-width="3"/>'
            f'<circle cx="62" cy="50" r="7" fill="{CREAM}" stroke="{INK}" stroke-width="3"/>'
            f'<circle cx="39" cy="51" r="3.5" fill="{INK}"/><circle cx="63" cy="51" r="3.5" fill="{INK}"/>'
            f'<ellipse cx="29" cy="62" rx="5" ry="3" fill="#ff8ab0" opacity=".7"/><ellipse cx="71" cy="62" rx="5" ry="3" fill="#ff8ab0" opacity=".7"/>' + m)


def star(cx, cy, r, col, turn=0.0):
    pts = []
    for i in range(10):
        a = 2 * math.pi * (turn + i * 0.1)
        rr = r if i % 2 == 0 else r * 0.45
        pts.append(f"{cx + rr * math.sin(a):.1f},{cy - rr * math.cos(a):.1f}")
    return f'<polygon points="{" ".join(pts)}" fill="{col}"/>'


def pad_pic(i, col):
    c = col
    return [
        f'<circle cx="50" cy="50" r="34" fill="{c}"/><circle cx="50" cy="50" r="19" fill="{INK}"/><circle cx="50" cy="50" r="12" fill="{c}"/>',
        f'<ellipse cx="50" cy="50" rx="36" ry="25" fill="{c}"/><polyline points="25,52 42,40 58,58 75,44" fill="none" stroke="{INK}" stroke-width="7" stroke-linejoin="round"/>',
        star(50, 50, 38, c),
        f'<ellipse cx="50" cy="40" rx="38" ry="12" fill="{c}"/><rect x="46" y="40" width="8" height="42" fill="{c}"/>',
        f'<rect x="20" y="34" width="60" height="46" rx="8" fill="{c}"/><ellipse cx="50" cy="34" rx="30" ry="9" fill="#ffffff" opacity=".5"/>',
        f'<path d="M34 16 H66 L82 84 H18 Z" fill="{c}"/>',
        "".join(f'<line x1="22" y1="{80 - k * 17}" x2="78" y2="{70 - k * 17}" stroke="{c}" stroke-width="9" stroke-linecap="round"/>' for k in range(4)),
        f'<polygon points="60,10 28,54 50,54 38,90 74,44 52,44" fill="{c}"/>',
        f'<circle cx="50" cy="50" r="30" fill="none" stroke="{c}" stroke-width="11"/><circle cx="38" cy="38" r="7" fill="{c}"/>',
        "".join(f'<line x1="{14 + o}" y1="{y}" x2="{70 + o}" y2="{y}" stroke="{c}" stroke-width="9" stroke-linecap="round"/>'
                for o, y in ((0, 32), (16, 52), (0, 72))),
        star(50, 50, 40, c) + star(50, 50, 18, INK, 0.1),
    ][i]


def wave(amp, col, w=7):
    pts = " ".join(f"{10 + x * 0.8:.1f},{50 - amp * math.sin(x / 100 * 4 * math.pi):.1f}" for x in range(0, 101, 4))
    return f'<polyline points="{pts}" fill="none" stroke="{col}" stroke-width="{w}" stroke-linecap="round" stroke-linejoin="round"/>'


def sun(col, rays=9):
    s = "".join(f'<line x1="{50 + 28 * math.sin(a):.1f}" y1="{50 - 28 * math.cos(a):.1f}" x2="{50 + 42 * math.sin(a):.1f}" '
                f'y2="{50 - 42 * math.cos(a):.1f}" stroke="{col}" stroke-width="7" stroke-linecap="round"/>'
                for a in (2 * math.pi * k / rays for k in range(rays)))
    return s + f'<circle cx="50" cy="50" r="19" fill="{col}"/>'


PICS = {
    "play": f'<polygon points="30,18 82,50 30,82" fill="#5ce452"/>',
    "rec": f'<circle cx="50" cy="50" r="30" fill="#ff4646"/><path d="M50 8 A42 42 0 1 1 12 34" fill="none" stroke="#ff7878" stroke-width="6"/>',
    "undo": f'<path d="M30 40 A28 28 0 1 1 30 70" fill="none" stroke="{CREAM}" stroke-width="9" stroke-linecap="round"/><polygon points="12,40 44,40 28,16" fill="{CREAM}"/>',
    "hop": friend("#4dafff", "round", 0.3).replace('cy="54"', 'cy="44"') + f'<line x1="18" y1="94" x2="82" y2="94" stroke="{CREAM}" stroke-width="5"/>',
    "big": friend("#4dafff", "round", 0.5),
    "small": f'<g transform="translate(25 25) scale(.5)">{friend("#4dafff", "round", 0.5)}</g>',
    "doors": "".join(f'<rect x="{12 + 40 * (i % 2)}" y="{12 + 40 * (i // 2)}" width="36" height="36" rx="7" fill="{c}"/>'
                     for i, c in enumerate(["#ff6e5a", "#5ac878", "#5a96ff", "#ffc846"])),
    "drum": pad_pic(0, "#ff5454"),
    "sparkle": star(50, 50, 40, "#ffe060") + star(80, 20, 12, CREAM),
    "wobble": wave(26, "#78dc8c"),
    "dice": f'<rect x="14" y="14" width="72" height="72" rx="16" fill="{CREAM}"/>' +
            "".join(f'<circle cx="{x}" cy="{y}" r="7" fill="{INK}"/>' for x, y in ((32, 32), (68, 32), (50, 50), (32, 68), (68, 68))),
    "heart": f'<path d="M50 84 C10 58 14 22 34 20 C44 19 50 28 50 34 C50 28 56 19 66 20 C86 22 90 58 50 84 Z" fill="#ff5a8c"/>',
    "shape": friend("#ff9020", "spiky", 0.5),
    "sun": sun("#ffdc50"),
    "room": f'<polygon points="12,50 50,14 88,50" fill="#78aaff"/><rect x="22" y="48" width="56" height="40" fill="#78aaff"/>'
            f'<rect x="42" y="62" width="16" height="26" fill="{NAVY}"/>',
    "turtle": f'<ellipse cx="44" cy="58" rx="30" ry="18" fill="#5ac86e"/><circle cx="80" cy="54" r="9" fill="#5ac86e"/>'
              f'<circle cx="84" cy="52" r="2.5" fill="{INK}"/>',
    "styles": f'<circle cx="34" cy="38" r="20" fill="#ff5a5a"/><line x1="14" y1="38" x2="54" y2="38" stroke="{CREAM}" stroke-width="5"/>'
              f'<circle cx="66" cy="68" r="20" fill="#c8d2e6"/>',
    "friend": friend("#4dafff", "round", 0.6),
    "chords": f'<ellipse cx="34" cy="72" rx="12" ry="9" fill="{CREAM}"/><line x1="44" y1="72" x2="44" y2="28" stroke="{CREAM}" stroke-width="5"/>'
              f'<ellipse cx="66" cy="60" rx="12" ry="9" fill="{CREAM}"/><line x1="76" y1="60" x2="76" y2="16" stroke="{CREAM}" stroke-width="5"/>'
              f'<line x1="44" y1="28" x2="76" y2="16" stroke="{CREAM}" stroke-width="8"/>',
    "speaker": f'<polygon points="16,38 34,38 56,18 56,82 34,62 16,62" fill="{CREAM}"/>'
               f'<path d="M66 32 Q80 50 66 68" fill="none" stroke="{CREAM}" stroke-width="6" stroke-linecap="round"/>',
}
# the stickers: (the control, the picture, the word under it); buttons first, then the knobs
BUTTONS = [("PLAY", "play", "music"), ("REC", "rec", "loop me"), ("EDIT", "undo", "oops"), ("ENV", "hop", "short / long"),
           ("OCT-", "big", "bigger"), ("OCT+", "small", "smaller"), ("HOME", "doors", "doors"), ("ARP", "sparkle", "sparkle"),
           ("FX", "dice", "surprise"), ("SEQ", "drum", "drums"), ("LFO", "wobble", "wobble"), ("SAVE", "heart", "keep"),
           ("GLO", "chords", "chords"), ("SEL", "speaker", "quiet")]
KNOBS = [("ALGORITHM", "friend", "friend"), ("PRESETS", "styles", "style"), ("SELECT", "turtle", "slow / fast"),
         ("KNOB 1", "shape", "shape"), ("KNOB 2", "sun", "bright"), ("KNOB 3", "wobble", "wobble"), ("KNOB 4", "room", "echo"),
         ("MASTER", "speaker", "loud")]


def sticker(x, y, d, pic, word, bg=NAVY):
    """a round sticker of diameter d mm at x, y (its corner), the picture and a word"""
    r = d / 2
    s = d / 100 * 0.62
    g = f'<g><circle cx="{x + r:.2f}" cy="{y + r:.2f}" r="{r:.2f}" fill="{bg}"/>'
    g += f'<g transform="translate({x + r - 50 * s:.2f} {y + r - 56 * s:.2f}) scale({s:.4f})">{pic}</g>'
    if word:
        g += (f'<text x="{x + r:.2f}" y="{y + d * 0.86:.2f}" font-family="Fredoka, Helvetica, Arial, sans-serif" '
              f'font-size="{d * 0.13:.2f}" font-weight="600" fill="{CREAM}" text-anchor="middle">{html.escape(word)}</text>')
    g += f'<circle cx="{x + r:.2f}" cy="{y + r:.2f}" r="{r - 0.15:.2f}" fill="none" stroke="#999" stroke-width="0.2" stroke-dasharray="1 1"/></g>'
    return g


def sticker_sheet():
    """A4 (210 x 297 mm): buttons 14 mm, knobs 18 mm, pads 10 mm; two of each"""
    out = [f'<svg xmlns="http://www.w3.org/2000/svg" width="210mm" height="297mm" viewBox="0 0 210 297">',
           '<rect width="210" height="297" fill="#fff"/>',
           '<text x="14" y="16" font-family="Helvetica, Arial, sans-serif" font-size="6" font-weight="700" fill="#14183a">BOP-1 stickers</text>',
           '<text x="14" y="22" font-family="Helvetica, Arial, sans-serif" font-size="3.2" fill="#444">Print at 100 % (no "fit to page") on sticker '
           'paper; cut on the dotted circles. Buttons 14 mm, knobs 18 mm (beside the knob, not on it),</text>',
           '<text x="14" y="26.5" font-family="Helvetica, Arial, sans-serif" font-size="3.2" fill="#444">black keys 10 mm. '
           'Check one against your FM-1 before printing a sheet.</text>']
    y = 32
    for row in range(2):
        for i, (ctl, pic, word) in enumerate(BUTTONS):
            x = 12 + (i % 7) * 27
            yy = y + (i // 7) * 22 + row * 46
            out.append(sticker(x, yy, 14, PICS[pic], word))
            out.append(f'<text x="{x + 7}" y="{yy + 18}" font-family="Helvetica, Arial, sans-serif" font-size="2.6" fill="#888" text-anchor="middle">{ctl}</text>')
    y = 128
    for row in range(2):
        for i, (ctl, pic, word) in enumerate(KNOBS):
            x = 10 + i * 24.5
            yy = y + row * 30
            out.append(sticker(x, yy, 18, PICS[pic], word))
            out.append(f'<text x="{x + 9}" y="{yy + 22}" font-family="Helvetica, Arial, sans-serif" font-size="2.6" fill="#888" text-anchor="middle">{ctl}</text>')
    y = 196
    for row in range(2):
        for i, (name, col) in enumerate(PADS):
            x = 12 + i * 17
            yy = y + row * 18
            out.append(sticker(x, yy, 10, pad_pic(i, col), ""))
            out.append(f'<text x="{x + 5}" y="{yy + 13.5}" font-family="Helvetica, Arial, sans-serif" font-size="2.4" fill="#888" text-anchor="middle">{name}</text>')
    y = 238
    out.append(f'<text x="14" y="{y}" font-family="Helvetica, Arial, sans-serif" font-size="3.4" fill="#444">The eight friends (for a box, a door, a book):</text>')
    shapes = ["round", "triangle", "spiky", "box", "spiky", "round", "spiky", "round"]
    for i, (name, col) in enumerate(FRIENDS):
        x = 12 + i * 24
        out.append(f'<g transform="translate({x} {y + 4}) scale(.2)">{friend(col, shapes[i], 0.5)}</g>')
        out.append(f'<text x="{x + 10}" y="{y + 28}" font-family="Helvetica, Arial, sans-serif" font-size="3" fill="#14183a" text-anchor="middle">{name}</text>')
    out.append("</svg>")
    return "\n".join(out)


PAGE = """<!doctype html>
<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1">
<title>__TITLE__</title>
<style>
@page { size: A4; margin: 12mm; }
body { font: 11.5pt/1.45 "Helvetica Neue", Helvetica, Arial, sans-serif; color: #14183a; margin: 0 auto; max-width: 186mm; padding: 16px; }
h1 { font-size: 22pt; margin: 0 0 .2em; } h2 { font-size: 14pt; margin: 1em 0 .3em; } h3 { font-size: 12pt; margin: .6em 0 .2em; }
.card { border: 2px solid #14183a; border-radius: 6mm; padding: 5mm 7mm; margin: 0 0 6mm; break-inside: avoid; }
.card .n { float: right; font-size: 26pt; font-weight: 700; color: #4dafff; }
.term { color: #7a4cff; font-weight: 600; }
.pics { display: flex; flex-wrap: wrap; gap: 6mm; }
.pic { width: 54mm; text-align: center; font-size: 15pt; font-weight: 700; }
.pic svg { width: 40mm; height: 40mm; background: #14183a; border-radius: 6mm; display: block; margin: 0 auto 2mm; }
.noprint { background: #fff6e2; padding: 8px 12px; border-radius: 8px; }
table { border-collapse: collapse; width: 100%; } td { border-bottom: 1px solid #ccc; padding: 3px 6px; vertical-align: top; }
td:first-child { font-weight: 700; white-space: nowrap; }
@media print { .noprint { display: none; } .break { break-before: page; } }
</style></head><body>
<p class="noprint">Print this page (A4 or Letter); "Save as PDF" in the print window makes a PDF. <a href="./">All printables</a></p>
__BODY__
</body></html>
"""

LESSONS = [
    ("What is sound?", "Sound is something moving: a wiggle in the air that reaches the ear.",
     ["Sound is a wiggle in the air.", "Your ear feels the wiggle, and you hear it."],
     "In the Lab, play a key and watch the scope: the wiggle, slowed down.",
     "Stretch a rubber band over a box and pluck it; put a hand on a speaker playing music.", ["waveform", "vibration"]),
    ("High and low", "Fast wiggles sound high, slow ones low: the pitch is the frequency.",
     ["Fast wiggles sound high; slow wiggles sound low.", "How fast: the frequency, in hertz (Hz), wiggles a second."],
     "Play a high key, then a low one; watch the scope's wave squeeze and stretch. The Lab shows each note's hertz.",
     "Pluck a long and a short rubber band; the shorter one wiggles faster and sounds higher.", ["pitch", "frequency", "hertz", "octave"]),
    ("Shapes of sound", "The same note with a different wave shape sounds different: that is its timbre.",
     ["Same note, other shape: it sounds different.", "Sine, triangle, saw, square, noise."],
     "Turn K1 (Wave) to Square: the friend becomes a box, the sound a robot.",
     "Sing \"oo\" then \"ee\" on the same note: same pitch, different shape.", ["timbre", "waveform", "oscillator"]),
    ("The filter", "A low-pass filter takes the bright, fizzy part out; resonance makes the edge squeak.",
     ["A filter takes the fizz out.", "Cutoff: how much bright gets through.", "Resonance: it squeaks at the edge."],
     "Turn K1 (Cutoff) all the way down, then K2 (Resonance) up. Watch the saw become round on the scope.",
     "Say \"aah\" and slowly close your mouth to \"mmm\": your mouth is a filter.", ["filter", "cutoff", "resonance", "low-pass"]),
    ("Envelopes", "How a sound starts and stops: attack, decay, sustain, release.",
     ["How a sound starts, and how it stops.", "Attack, Decay, Sustain, Release."],
     "Turn K1 (Attack) up: a slow start, like a bowed violin. Down again: a plucked string.",
     "Clap (a fast attack and decay) and hum (a slow attack, a long sustain); draw both as hills.", ["envelope", "ADSR", "attack", "release"]),
    ("The LFO", "A low-frequency oscillator is a hand that keeps turning a knob for you.",
     ["An LFO is a hand that keeps turning a knob.", "Pitch: vibrato. Filter: wah. Volume: tremolo."],
     "Turn K3 (Depth) up and play: the friend wobbles. K4 picks what it moves.",
     "Sing a long note and wobble your hand on your throat; then on your mouth (wah); then near and far from a friend's ear.",
     ["LFO", "vibrato", "tremolo", "modulation"]),
    ("Two oscillators", "Two oscillators a little apart sound fat; a fifth apart, bright.",
     ["Two a little apart make it fat.", "A fifth apart makes it bright."],
     "Turn K2 (Detune) up on the second oscillator page and listen to the slow beating.",
     "Two children hum the same note, then one slides a little higher: hear the wobble between them.", ["detune", "interval", "fifth"]),
    ("Beats and steps", "A bar of music is sixteen steps; the kick on 1, the snare on 5 and 13 makes a beat.",
     ["A bar has 16 steps. The kick on 1...", "...the snare on 5 and on 13."],
     "Press white keys 1, 5, 9 and 13; then in Make, put a kick and a snare on those steps.",
     "Sixteen children in a row, each a step; the 1s and 5s clap while the class counts.", ["step sequencer", "bar", "beat"]),
    ("Scales", "Some notes go together: a scale. The pentatonic has five and no wrong ones.",
     ["Some notes go together: a scale.", "Pentatonic: five notes, no wrong ones.", "Major sounds happy, minor sounds sad."],
     "Play five different notes: in Play every white key is in the scale, so nothing sounds wrong.",
     "Play only the black keys of a piano: a pentatonic scale everyone can improvise on.", ["scale", "pentatonic", "major", "minor"]),
    ("Chords and songs", "Three notes at once make a chord; parts one after another make a song.",
     ["Three notes at once: a chord.", "Parts A and B, one after another: a song."],
     "Hold three keys at once. Then in Make, hold ARP and press A A B A: a song.",
     "Three children sing three notes together; then the class sings a verse (A) and a chorus (B).", ["chord", "triad", "verse", "chorus"]),
]


def lessons_page():
    body = ["<h1>BOP-1: ten lessons</h1>",
            "<p>For a class or a family: each lesson is short, a few cards on the FM-1 (Lab door, key 2) and a try-it the "
            "firmware checks; finishing one puts a sticker in its sticker book. Below, for each one: what it teaches, its "
            "cards, the try-it, an activity without the FM-1, and the real words. Ages 9 and up; younger ones with help.</p>"]
    for i, (title, teaches, cards, try_it, activity, words) in enumerate(LESSONS):
        if i and i % 3 == 0:
            body.append('<div class="break"></div>')
        body.append(f'<div class="card"><span class="n">{i + 1}</span><h2>{html.escape(title)}</h2><p>{html.escape(teaches)}</p>')
        body.append("<h3>On the FM-1</h3><ol>" + "".join(f"<li>{html.escape(c)}</li>" for c in cards) +
                    f"<li><b>Try it:</b> {html.escape(try_it)}</li></ol>")
        body.append(f"<h3>Without it</h3><p>{html.escape(activity)}</p>")
        body.append("<p>The real words: " + ", ".join(f'<span class="term">{html.escape(w)}</span>' for w in words) + "</p></div>")
    return PAGE.replace("__TITLE__", "BOP-1 lesson cards").replace("__BODY__", "\n".join(body))


def guide_page():
    rows = [("Switching on", "Music plays at once (a grown-up can make it quiet). The volume knob is MASTER; the cap below holds it back."),
            ("The doors", "HOME shows them: Play (3+), Make (6-8), Lab (9+), Games. A white key picks, the same key (or PLAY) opens."),
            ("Play", "White keys sing, black keys drum. ALGORITHM a friend, PRESETS a style, SELECT the speed. FX: Surprise!"),
            ("Make", "Four layers (Beats, Bass, Tune, Sparkle), sixteen steps, recording in time, parts A-D into a song."),
            ("Lab", "The real synth by its real names, a live scope, My Friends, and ten lessons (key 2 on its first screen)."),
            ("Games", "Six games and a sticker book. No timers for the youngest; nothing is ever taken away."),
            ("Help", "Hold any button for a second: a card says what it does there."),
            ("Oops", "EDIT undoes; held two seconds it clears (a ring fills first). Nothing is deleted with a tap."),
            ("Keeping songs", "SAVE, then a white key: eight places. Songs, My Friends and stickers stay when it is off."),
            ("Grown-ups' page", "Hold GLO and SAVE together for three seconds. SELECT moves, knob 1 changes, HOME closes."),
            ("For the youngest", "Grown-ups' page: Doors shown: Play only. Then HOME does nothing and nobody gets lost."),
            ("Safety", "A volume cap (default 70 %), a limiter that never lets the output past it, no squeals in Play and Make, "
                       "flashes at most three a second (none with Lights: calm)."),
            ("Bedtime", "Sleep timer: the music stops and the screen dims after 5-30 minutes; pick the Lullaby style first."),
            ("Battery", "Idle: the screen dims after 2 minutes untouched and the music stops after 10."),
            ("Stuck?", "HOME always goes home. Update mode: hold OCT- and OCT+ for five seconds. The web installer can put it back."),
            ("Stickers", "The sticker overlay (on this site) puts a picture on each button for children who don't read yet.")]
    body = ["<h1>BOP-1: the grown-ups' quick guide</h1>",
            "<table>" + "".join(f"<tr><td>{html.escape(a)}</td><td>{html.escape(b)}</td></tr>" for a, b in rows) + "</table>",
            "<p style='font-size:9pt;color:#666'>BOP-1 is free software (GPL-3.0), independent of M-VAVE. The manual, the emulator "
            "and the installer are on its site.</p>"]
    return PAGE.replace("__TITLE__", "BOP-1 quick guide").replace("__BODY__", "\n".join(body))


def quickstart_page():
    pics = [("power", "Switch on: music!", PICS["play"]), ("white", "White keys sing", friend("#4dafff", "round", 0.8)),
            ("black", "Black keys go boom", pad_pic(0, "#ff5454")), ("knobs", "Knobs change your friend", friend("#ff9020", "spiky", 0.4)),
            ("fx", "FX: surprise!", PICS["dice"]), ("rec", "REC: loop me", PICS["rec"]),
            ("oops", "EDIT: oops", PICS["undo"]), ("keep", "SAVE: keep it", PICS["heart"]),
            ("help", "Hold a button: help", f'<rect x="14" y="20" width="72" height="60" rx="12" fill="{CREAM}"/>' + star(50, 50, 16, "#4dafff"))]
    body = ['<h1 style="font-size:30pt">BOP-1</h1><div class="pics">']
    for _, word, pic in pics:
        body.append(f'<div class="pic"><svg viewBox="0 0 100 100">{pic}</svg>{html.escape(word)}</div>')
    body.append("</div>")
    return PAGE.replace("__TITLE__", "BOP-1 picture quick-start").replace("__BODY__", "\n".join(body))


INDEX = """<h1>BOP-1 printables</h1>
<ul style="font-size:13pt;line-height:1.9">
<li><a href="stickers.html">Sticker overlay</a> (<a href="stickers.svg">SVG</a>): a picture for each button and knob, and the pads for the black keys</li>
<li><a href="lessons.html">The ten lessons</a>: cards for a class or a family</li>
<li><a href="guide.html">The grown-ups' quick guide</a> (one page)</li>
<li><a href="quickstart.html">The picture quick-start</a> for children (one page, no reading)</li>
</ul>
<p>Each prints on A4 or Letter; the print window's "Save as PDF" makes a PDF.</p>"""


def main(out):
    out = Path(out)
    out.mkdir(parents=True, exist_ok=True)
    svg = sticker_sheet()
    (out / "stickers.svg").write_text(svg, encoding="utf-8")
    (out / "stickers.html").write_text(
        PAGE.replace("__TITLE__", "BOP-1 stickers").replace(
            "__BODY__", '<style>@page { margin: 0; } body { padding: 0; max-width: none; }</style>' + svg), encoding="utf-8")
    (out / "lessons.html").write_text(lessons_page(), encoding="utf-8")
    (out / "guide.html").write_text(guide_page(), encoding="utf-8")
    (out / "quickstart.html").write_text(quickstart_page(), encoding="utf-8")
    (out / "index.html").write_text(PAGE.replace("__TITLE__", "BOP-1 printables").replace("__BODY__", INDEX), encoding="utf-8")
    print(f"printables: {out}")


if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    main(sys.argv[1])
