#!/usr/bin/env python3
"""Generate the ED047TC2 LovyanGFX waveform tables from the vendor waveform blob.

Input is the epdiy-format vendor waveform header for the ED047TC2 panel used by
the LilyGo T5 S3 Pro (`epdiy_ED047TC2.h` / `ED047TC2.h` -- the two are the same
data under different symbol names).  Output is
`libs/hardware/BoardT5S3/src/ED047TC2Waveform.cpp`.

    python tools/gen_ed047tc2_waveform.py path/to/epdiy_ED047TC2.h

Vendor blob layout
------------------
Each `epd_wp_ED047TC2_<mode>_<range>_data[phases][16][4]` table holds, for one
draw mode and one temperature range, a 2-bit drive code per (destination level,
source level) pair per phase:

    code 0 = no drive,  1 = drive toward black,  2 = drive toward white

The outer index of the 16 rows is the DESTINATION level; the 4 bytes of each row
hold the 16 SOURCE levels, four per byte, most significant pair first.  Level 0
is black, level 15 is white.

Why the tables below are only two columns wide
----------------------------------------------
Decoding the blob shows every mode/range is a separable impulse waveform:

    net_frames(to, from) == L[to] - L[from]

with a zero diagonal, for one impulse vector L per temperature range shared by
all three modes (DU, GC16, GL16).  The DU tables are exactly `L[15] + 1` phases
long and drive black->white as `L[15]` consecutive white frames (and white->black
as `L[15]` consecutive black frames), so DU reduces to a single number per
temperature range: the drive length.  This script asserts all of that against the
blob before emitting anything, so a different or corrupted input fails loudly
rather than producing a plausible-looking wrong waveform.

The emitted `epd_fast` bank is the DU table plus two grey columns.  DU itself only
defines the two rails, but the impulse vector gives the cost of every other
level, so a destination-indexed nudge for the AA greys drops straight out of it:
level `g` reached from black costs `L[g]` frames of drive toward white.  That is
only valid because the B/W base push leaves every AA fringe pixel at black before
the grey push runs -- see ED047TC2Waveform.cpp for the full argument.

Both greys live in the *same* bank as the B/W drive on purpose.  Panel_EPD stores
the LUT bank offset inside its per-pixel progress value, so a base push and a grey
push in different epd_modes make every pixel compare unequal and re-drive the whole
screen.  One bank, two pushes, diff intact.
"""

import re
import sys
from pathlib import Path

BLOB_RE = re.compile(
    r"const uint8_t epd_wp_ED047TC2_(\d+)_(\d+)_data\[(\d+)\]\[16\]\[4\]\s*=\s*(.*?);",
    re.S | re.I,
)
INTERVAL_RE = re.compile(
    r"const EpdWaveformTempInterval ed047tc2_intervals\[(\d+)\]\s*=\s*(.*?);", re.S
)

MODE_DU, MODE_GC16, MODE_GL16 = 1, 2, 5

# The vendor blob carries ranges 5..11 of the 14-entry interval table.
FIRST_RANGE = 5
RANGE_COUNT = 7


def parse(path):
    text = Path(path).read_text(encoding="utf-8", errors="replace")

    tables = {}
    for m in BLOB_RE.finditer(text):
        mode, rng, phases = int(m.group(1)), int(m.group(2)), int(m.group(3))
        vals = [int(x, 16) for x in re.findall(r"0x([0-9a-fA-F]{2})", m.group(4))]
        if len(vals) != phases * 16 * 4:
            raise SystemExit(f"mode {mode} range {rng}: expected {phases*16*4} bytes, got {len(vals)}")
        table = []
        for p in range(phases):
            rows = []
            for r in range(16):
                base = (p * 16 + r) * 4
                row = []
                for byte in vals[base : base + 4]:
                    row += [(byte >> (6 - 2 * k)) & 3 for k in range(4)]
                rows.append(row)
            table.append(rows)
        tables[(mode, rng)] = table

    im = INTERVAL_RE.search(text)
    if not im:
        raise SystemExit("no ed047tc2_intervals table found")
    nums = [int(x) for x in re.findall(r"\.(?:min|max)\s*=\s*(-?\d+)", im.group(2))]
    intervals = list(zip(nums[0::2], nums[1::2]))

    return tables, intervals


def impulse_vector(table):
    """Return L with L[0] == 0 such that net(to, from) == L[to] - L[from]."""
    net = [[0] * 16 for _ in range(16)]
    for phase in table:
        for to in range(16):
            for src in range(16):
                code = phase[to][src]
                if code == 1:
                    net[to][src] -= 1
                elif code == 2:
                    net[to][src] += 1
                elif code == 3:
                    raise SystemExit("unexpected drive code 3 in vendor blob")
    return net, [net[to][0] - net[0][0] for to in range(16)]


def check(tables, intervals):
    """Validate every structural assumption the generated tables rely on."""
    ranges = list(range(FIRST_RANGE, FIRST_RANGE + RANGE_COUNT))
    for mode in (MODE_DU, MODE_GC16, MODE_GL16):
        for rng in ranges:
            if (mode, rng) not in tables:
                raise SystemExit(f"vendor blob is missing mode {mode} range {rng}")
    if len(intervals) < FIRST_RANGE + RANGE_COUNT:
        raise SystemExit("interval table too short for ranges 5..11")

    impulses = {}
    for rng in ranges:
        net_gc, l_gc = impulse_vector(tables[(MODE_GC16, rng)])
        # GC16 must be exactly separable with a zero diagonal.
        for to in range(16):
            for src in range(16):
                if net_gc[to][src] != l_gc[to] - l_gc[src]:
                    raise SystemExit(f"range {rng}: GC16 is not separable at ({to},{src})")
            if net_gc[to][to] != 0:
                raise SystemExit(f"range {rng}: GC16 diagonal is not zero at {to}")
        if l_gc != sorted(l_gc):
            raise SystemExit(f"range {rng}: GC16 impulse vector is not monotonic: {l_gc}")

        # GL16 must agree with GC16 on the impulse vector.
        _, l_gl = impulse_vector(tables[(MODE_GL16, rng)])
        if l_gl != l_gc:
            raise SystemExit(f"range {rng}: GL16 impulse {l_gl} != GC16 impulse {l_gc}")

        # DU must be the two-level restriction of the same vector, run as one
        # contiguous burst starting at phase 0, and nothing else.
        du = tables[(MODE_DU, rng)]
        drive = l_gc[15]
        if len(du) != drive + 1:
            raise SystemExit(f"range {rng}: DU has {len(du)} phases, expected {drive + 1}")
        for to in range(16):
            for src in range(16):
                seq = [phase[to][src] for phase in du]
                if to == 15 and src == 0:
                    want = [2] * drive + [0]
                elif to == 0 and src == 15:
                    want = [1] * drive + [0]
                elif to in (0, 15):
                    continue  # intermediate sources: unused by a binarised canvas
                else:
                    want = [0] * len(du)
                if seq != want:
                    raise SystemExit(f"range {rng}: DU[{to}][{src}] = {seq}, expected {want}")
        impulses[rng] = l_gc
    return impulses


# Optical targets for the two AA greys, as a fraction of the way from black to
# white. The 2-bit font quantises a glyph edge to 25-50% ink (its light grey) and
# 50-75% ink (its dark grey), so the midpoints of those buckets are what the panel
# should land on.
GRAY_TARGET_DARK = 0.375
GRAY_TARGET_LIGHT = 0.625

# A grey is only usable if it stays clear of white and clear of the other grey.
GRAY_MAX_FRACTION = 0.95
GRAY_MIN_SEPARATION = 0.08


def pick_gray_levels(impulse):
    """Pick the (dark, light) canvas levels that best hit the targets in one range.

    The vendor impulse vector is not evenly spaced and its spacing changes with
    temperature -- at 33..38 C levels 12..15 are all the same optical white, so a
    level that reads as a good light grey when cold is no grey at all when warm.
    Choosing per range is what keeps both greys real across the whole table.
    """
    white = impulse[15]
    frac = [impulse[lv] / white for lv in range(16)]

    best = None
    for dark in range(1, 15):
        for light in range(dark + 1, 15):
            if frac[light] > GRAY_MAX_FRACTION:
                continue
            if frac[light] - frac[dark] < GRAY_MIN_SEPARATION:
                continue
            err = (frac[dark] - GRAY_TARGET_DARK) ** 2 + (frac[light] - GRAY_TARGET_LIGHT) ** 2
            if best is None or err < best[0]:
                best = (err, dark, light)
    if best is None:
        raise SystemExit("no usable grey pair for impulse %s" % impulse)
    return best[1], best[2]


def fast_row(frame, l_dark, l_light, dark, light):
    """One frame of the epd_fast bank: the two B/W rails plus the grey nudges."""
    codes = [3] * 16
    codes[0] = 1  # headed for black: driven black for the whole bank
    codes[15] = 2  # headed for white: driven white for the whole bank
    # A grey destination is driven toward white for exactly L[level] frames and
    # then parked. That lands it on `level` only if it started at black, which is
    # what the B/W base push guarantees.
    if frame < l_dark:
        codes[dark] = 2
    if frame < l_light:
        codes[light] = 2
    return "    LUT_MAKE(%s)," % ", ".join(str(c) for c in codes)


# Away from the target: dark levels get driven white, light levels black.
CLEAN_AWAY_ROW = "    LUT_MAKE(2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1),"
# ...then back onto it, the same number of frames, so the net is zero.
CLEAN_ONTO_ROW = "    LUT_MAKE(1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2),"

HEADER = '''// GENERATED FILE -- DO NOT EDIT BY HAND.
//
// Regenerate with:
//     python tools/gen_ed047tc2_waveform.py <vendor ED047TC2 waveform header>
//
// Source: the ED047TC2 vendor waveform in epdiy form -- LilyGo ships it as
// Waveform_header/ED047TC2.h in Xinyuan-LilyGO/LilyGo-EPD47, epdiy as
// epdiy_ED047TC2.h; the two are the same data under different symbol names.
// See tools/gen_ed047tc2_waveform.py for the blob layout and for the structural
// checks the generator runs against it.
//
// The vendor waveform is a separable impulse waveform: a transition from source
// level `f` to destination level `t` needs `L[t] - L[f]` frames of drive, toward
// white when positive and toward black when negative, where L is the per
// temperature range impulse vector below (level 0 is black, 15 is white):
//
{impulse_comment}
//
// kFastLut, for the differential modes (epd_fast / epd_fastest), is the vendor DU
// table plus the two grey columns the reader's anti-aliasing needs. The DU part is
// verbatim: `L[15]` frames pushing level 0 toward black and level 15 toward white,
// then a terminating all-zero frame.
//
// The grey part is the same impulse vector read at two more levels. A grey
// destination is driven toward white for `L[level]` frames and then parked on
// a no-op, which lands it on that level *provided it started at black*. That
// proviso is the whole design: the reader always pushes the B/W base first, and
// that base paints every partially covered glyph pixel solid black, so by the time
// the grey push runs the fringe is at a known rail. A LovyanGFX LUT column is
// indexed by destination alone and cannot see where a pixel came from, so a
// from-black nudge is the only correct shape available here.
//
// Both live in one bank deliberately. Panel_EPD folds the LUT bank offset into its
// per-pixel progress value, so pushing the base and the greys under different
// epd_modes makes every pixel compare unequal and re-drives the whole screen.
//
// Which levels carry the greys is decided per temperature range, because the
// vendor vector is not evenly spaced and its spacing moves with temperature: at
// 33..38 C levels 12 through 15 are all the same optical white, so a level that
// reads as a good light grey when cold is no grey at all when warm. The board
// config reads kGrayLevelDark/kGrayLevelLight for the range it selected the LUT
// for and writes the matching canvas bytes, so the two always agree.
//
{gray_comment}
//
// kCleanLut, for epd_text / epd_quality, is the clean refresh. Those modes re-drive
// every non-white pixel whether or not it changed, so this LUT has to be charge
// neutral or a run of clean refreshes would pump a bias into the text. It spends
// `L[15]` frames driving each level *away* from where it is headed and the same
// number driving it back, which nets exactly zero and puts every re-driven pixel
// through a full rail-to-rail excursion -- which is what shakes out the residue a
// long run of differential updates leaves behind.
//
// Keeping the drive length matched to the panel temperature IS the temperature
// compensation: e-ink particles move more slowly when cold, so a cold panel needs
// a longer push for the same optical result. Driving a warm panel with a cold
// waveform over-drives it and driving a cold panel with a warm one leaves the
// image grey and ghosted.

#include <ED047TC2Waveform.h>

namespace freeink {{
namespace ed047tc2 {{

namespace {{

// One frame of a LovyanGFX Panel_EPD LUT: sixteen 2-bit drive codes, indexed by
// the destination level. 0 ends the sequence, 1 drives toward black, 2 drives
// toward white, 3 is a no-op that keeps the sequence running.
#define LUT_MAKE(d0, d1, d2, d3, d4, d5, d6, d7, d8, d9, da, db, dc, dd, de, df)         \\
  (uint32_t)((d0 << 0) | (d1 << 2) | (d2 << 4) | (d3 << 6) | (d4 << 8) | (d5 << 10) |    \\
             (d6 << 12) | (d7 << 14) | (d8 << 16) | (d9 << 18) | (da << 20) |            \\
             (db << 22) | (dc << 24) | (dd << 26) | (de << 28) | (df << 30))

'''

FOOTER = '''#undef LUT_MAKE

}}  // namespace

const TempRange kTempRanges[kTempRangeCount] = {{
{ranges}
}};

// The canvas levels the AA grey columns are cut for, per temperature range. A
// board config turns these into the grey bytes its canvas writes; they are not
// independently tunable -- retarget them in tools/gen_ed047tc2_waveform.py and
// regenerate, or the canvas will address a column the LUT does not drive.
const uint8_t kGrayLevelDark[kTempRangeCount] = {{{gray_dark_levels}}};

const uint8_t kGrayLevelLight[kTempRangeCount] = {{{gray_light_levels}}};

const uint32_t* const kFastLut[kTempRangeCount] = {{
{fast_ptrs}
}};

const size_t kFastLutStep[kTempRangeCount] = {{
{fast_steps}
}};

const uint32_t* const kCleanLut[kTempRangeCount] = {{
{clean_ptrs}
}};

const size_t kCleanLutStep[kTempRangeCount] = {{
{clean_steps}
}};

const uint8_t kDriveFrames[kTempRangeCount] = {{{drive_frames}}};

size_t tempRangeIndex(int tempC) {{
  for (size_t i = 0; i < kTempRangeCount; ++i) {{
    if (tempC < kTempRanges[i].maxC) return i;
  }}
  return kTempRangeCount - 1;
}}

}}  // namespace ed047tc2
}}  // namespace freeink
'''


def main():
    if len(sys.argv) != 2:
        raise SystemExit(__doc__)
    tables, intervals = parse(sys.argv[1])
    impulses = check(tables, intervals)

    ranges = list(range(FIRST_RANGE, FIRST_RANGE + RANGE_COUNT))
    drives = [impulses[r][15] for r in ranges]

    impulse_comment = "\n".join(
        "//   {:>2}..{:<2} C  L = [{}]".format(
            intervals[r][0], intervals[r][1], ", ".join("%2d" % v for v in impulses[r])
        )
        for r in ranges
    )

    # Grey levels are chosen per range, so the epd_fast bank is keyed on the two
    # levels and the three frame counts that cut its columns. Ranges that agree on
    # all five share one table.
    grays = {r: pick_gray_levels(impulses[r]) for r in ranges}
    bank_keys = [
        (
            impulses[r][15],
            grays[r][0],
            impulses[r][grays[r][0]],
            grays[r][1],
            impulses[r][grays[r][1]],
        )
        for r in ranges
    ]
    bank_name = {k: "kFast%d_%dat%d_%dat%d" % k for k in bank_keys}

    gray_comment = "\n".join(
        "//   {:>2}..{:<2} C  dark  = level {:>2} at {:>2} frames ({:>3.0f}% to white)"
        "   light = level {:>2} at {:>2} frames ({:>3.0f}% to white)".format(
            intervals[r][0],
            intervals[r][1],
            grays[r][0],
            impulses[r][grays[r][0]],
            100.0 * impulses[r][grays[r][0]] / impulses[r][15],
            grays[r][1],
            impulses[r][grays[r][1]],
            100.0 * impulses[r][grays[r][1]] / impulses[r][15],
        )
        for r in ranges
    )

    body = HEADER.format(impulse_comment=impulse_comment, gray_comment=gray_comment)

    for key in dict.fromkeys(bank_keys):
        drive, dark, l_dark, light, l_light = key
        body += (
            "// Differential update + AA grey nudge, %d drive frames.\n"
            "// Level %d parks after %d frames, level %d after %d.\n"
            % (drive, dark, l_dark, light, l_light)
        )
        body += "constexpr uint32_t %s[] = {\n" % bank_name[key]
        body += "".join(
            fast_row(f, l_dark, l_light, dark, light) + "\n" for f in range(drive)
        )
        body += "    0u,\n};\n\n"

    for drive in dict.fromkeys(drives):
        body += "// Clean refresh, %d frames away from the target then %d back.\n" % (drive, drive)
        body += "constexpr uint32_t kClean%d[] = {\n" % drive
        body += (CLEAN_AWAY_ROW + "\n") * drive
        body += (CLEAN_ONTO_ROW + "\n") * drive
        body += "    0u,\n};\n\n"

    body += FOOTER.format(
        ranges="\n".join(
            "    {%d, %d}," % (intervals[r][0], intervals[r][1]) for r in ranges
        ),
        gray_dark_levels=", ".join(str(grays[r][0]) for r in ranges),
        gray_light_levels=", ".join(str(grays[r][1]) for r in ranges),
        fast_ptrs="\n".join("    %s," % bank_name[k] for k in bank_keys),
        fast_steps="\n".join(
            "    sizeof(%s) / sizeof(%s[0])," % (bank_name[k], bank_name[k])
            for k in bank_keys
        ),
        clean_ptrs="\n".join("    kClean%d," % d for d in drives),
        clean_steps="\n".join(
            "    sizeof(kClean%d) / sizeof(kClean%d[0])," % (d, d) for d in drives
        ),
        drive_frames=", ".join(str(d) for d in drives),
    )

    out = (
        Path(__file__).resolve().parent.parent
        / "libs/hardware/BoardT5S3/src/ED047TC2Waveform.cpp"
    )
    out.write_text(body, encoding="utf-8")
    print("wrote", out)
    for r, d in zip(ranges, drives):
        print("  range %2d  %2d..%-2d C  drive=%2d frames  greys=level %d/%d"
              % (r, intervals[r][0], intervals[r][1], d, grays[r][0], grays[r][1]))


if __name__ == "__main__":
    main()
