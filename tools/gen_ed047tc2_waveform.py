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

CrossPoint only ever puts two levels on this panel (LovyanGFX's epd_fast path
binarises the canvas and dithers), so the DU tables are the whole waveform it
needs.  The full impulse vectors are emitted as a comment for future 16-level
work.
"""

import re
import sys
from pathlib import Path

BLOB_RE = re.compile(
    r"const uint8_t epd_wp_ED047TC2_(\d+)_(\d+)_data\[(\d+)\]\[16\]\[4\]\s*=\s*(.*?);",
    re.S,
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


LUT_ROW = "    LUT_MAKE(1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2),"

HEADER = '''// GENERATED FILE -- DO NOT EDIT BY HAND.
//
// Regenerate with:
//     python tools/gen_ed047tc2_waveform.py <vendor ED047TC2 waveform header>
//
// Source: the ED047TC2 vendor waveform shipped in epdiy form (epdiy_ED047TC2.h).
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
// CrossPoint drives this panel with two levels only, so every transition it can
// ask for is +/- L[15] frames -- which is exactly what the vendor DU tables do,
// as `L[15]` consecutive frames from phase 0. That reduces to the LUTs here: one
// per distinct drive length, holding `drive` identical frames that push level 0
// toward black and level 15 toward white, then a terminating all-zero frame.
// Levels 1..14 are no-ops, matching the vendor DU tables, which leave every
// destination other than black and white undriven.
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

const uint32_t* const kDuLut[kTempRangeCount] = {{
{lut_ptrs}
}};

const size_t kDuLutStep[kTempRangeCount] = {{
{lut_steps}
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

    body = HEADER.format(impulse_comment=impulse_comment)
    for drive in sorted(set(drives)):
        body += "// %d drive frames\n" % drive
        body += "constexpr uint32_t kLut%d[] = {\n" % drive
        body += (LUT_ROW + "\n") * drive
        body += "    0u,\n};\n\n"

    body += FOOTER.format(
        ranges="\n".join(
            "    {%d, %d}," % (intervals[r][0], intervals[r][1]) for r in ranges
        ),
        lut_ptrs="\n".join("    kLut%d," % d for d in drives),
        lut_steps="\n".join(
            "    sizeof(kLut%d) / sizeof(kLut%d[0])," % (d, d) for d in drives
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
        print("  range %2d  %2d..%-2d C  drive=%2d frames" % (r, intervals[r][0], intervals[r][1], d))


if __name__ == "__main__":
    main()
