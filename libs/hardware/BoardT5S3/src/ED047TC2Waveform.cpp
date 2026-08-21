// GENERATED FILE -- DO NOT EDIT BY HAND.
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
//   15..18 C  L = [ 0,  2,  3,  4,  4,  5,  6,  7,  7,  8,  9, 10, 15, 16, 17, 24]
//   18..21 C  L = [ 0,  3,  5,  6,  6,  7,  8,  9,  9,  9, 11, 12, 13, 17, 19, 21]
//   21..24 C  L = [ 0,  3,  3,  5,  5,  6,  8,  8,  9,  9, 10, 12, 12, 17, 17, 21]
//   24..27 C  L = [ 0,  3,  3,  5,  6,  6,  7,  8,  9,  9,  9,  9, 12, 16, 19, 21]
//   27..30 C  L = [ 0,  3,  5,  5,  5,  6,  6,  7,  8,  9,  9,  9, 12, 13, 15, 17]
//   30..33 C  L = [ 0,  4,  4,  4,  5,  6,  7,  7,  8,  8,  9, 10, 12, 12, 14, 16]
//   33..38 C  L = [ 0,  1,  2,  2,  3,  6,  7,  7,  9,  9, 10, 10, 14, 14, 14, 14]
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
//   15..18 C  dark  = level 10 at  9 frames ( 38% to white)   light = level 12 at 15 frames ( 62% to white)
//   18..21 C  dark  = level  6 at  8 frames ( 38% to white)   light = level 12 at 13 frames ( 62% to white)
//   21..24 C  dark  = level  6 at  8 frames ( 38% to white)   light = level 11 at 12 frames ( 57% to white)
//   24..27 C  dark  = level  7 at  8 frames ( 38% to white)   light = level 12 at 12 frames ( 57% to white)
//   27..30 C  dark  = level  5 at  6 frames ( 35% to white)   light = level 12 at 12 frames ( 71% to white)
//   30..33 C  dark  = level  5 at  6 frames ( 38% to white)   light = level 11 at 10 frames ( 62% to white)
//   33..38 C  dark  = level  5 at  6 frames ( 43% to white)   light = level  8 at  9 frames ( 64% to white)
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

namespace freeink {
namespace ed047tc2 {

namespace {

// One frame of a LovyanGFX Panel_EPD LUT: sixteen 2-bit drive codes, indexed by
// the destination level. 0 ends the sequence, 1 drives toward black, 2 drives
// toward white, 3 is a no-op that keeps the sequence running.
#define LUT_MAKE(d0, d1, d2, d3, d4, d5, d6, d7, d8, d9, da, db, dc, dd, de, df)         \
  (uint32_t)((d0 << 0) | (d1 << 2) | (d2 << 4) | (d3 << 6) | (d4 << 8) | (d5 << 10) |    \
             (d6 << 12) | (d7 << 14) | (d8 << 16) | (d9 << 18) | (da << 20) |            \
             (db << 22) | (dc << 24) | (dd << 26) | (de << 28) | (df << 30))

// Differential update + AA grey nudge, 24 drive frames.
// Level 10 parks after 9 frames, level 12 after 15.
constexpr uint32_t kFast24_10at9_12at15[] = {
    LUT_MAKE(1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2, 3, 2, 3, 3, 2),
    LUT_MAKE(1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2, 3, 2, 3, 3, 2),
    LUT_MAKE(1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2, 3, 2, 3, 3, 2),
    LUT_MAKE(1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2, 3, 2, 3, 3, 2),
    LUT_MAKE(1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2, 3, 2, 3, 3, 2),
    LUT_MAKE(1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2, 3, 2, 3, 3, 2),
    LUT_MAKE(1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2, 3, 2, 3, 3, 2),
    LUT_MAKE(1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2, 3, 2, 3, 3, 2),
    LUT_MAKE(1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2, 3, 2, 3, 3, 2),
    LUT_MAKE(1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2, 3, 3, 2),
    LUT_MAKE(1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2, 3, 3, 2),
    LUT_MAKE(1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2, 3, 3, 2),
    LUT_MAKE(1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2, 3, 3, 2),
    LUT_MAKE(1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2, 3, 3, 2),
    LUT_MAKE(1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2, 3, 3, 2),
    LUT_MAKE(1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2),
    LUT_MAKE(1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2),
    LUT_MAKE(1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2),
    LUT_MAKE(1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2),
    LUT_MAKE(1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2),
    LUT_MAKE(1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2),
    LUT_MAKE(1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2),
    LUT_MAKE(1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2),
    LUT_MAKE(1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2),
    0u,
};

// Differential update + AA grey nudge, 21 drive frames.
// Level 6 parks after 8 frames, level 12 after 13.
constexpr uint32_t kFast21_6at8_12at13[] = {
    LUT_MAKE(1, 3, 3, 3, 3, 3, 2, 3, 3, 3, 3, 3, 2, 3, 3, 2),
    LUT_MAKE(1, 3, 3, 3, 3, 3, 2, 3, 3, 3, 3, 3, 2, 3, 3, 2),
    LUT_MAKE(1, 3, 3, 3, 3, 3, 2, 3, 3, 3, 3, 3, 2, 3, 3, 2),
    LUT_MAKE(1, 3, 3, 3, 3, 3, 2, 3, 3, 3, 3, 3, 2, 3, 3, 2),
    LUT_MAKE(1, 3, 3, 3, 3, 3, 2, 3, 3, 3, 3, 3, 2, 3, 3, 2),
    LUT_MAKE(1, 3, 3, 3, 3, 3, 2, 3, 3, 3, 3, 3, 2, 3, 3, 2),
    LUT_MAKE(1, 3, 3, 3, 3, 3, 2, 3, 3, 3, 3, 3, 2, 3, 3, 2),
    LUT_MAKE(1, 3, 3, 3, 3, 3, 2, 3, 3, 3, 3, 3, 2, 3, 3, 2),
    LUT_MAKE(1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2, 3, 3, 2),
    LUT_MAKE(1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2, 3, 3, 2),
    LUT_MAKE(1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2, 3, 3, 2),
    LUT_MAKE(1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2, 3, 3, 2),
    LUT_MAKE(1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2, 3, 3, 2),
    LUT_MAKE(1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2),
    LUT_MAKE(1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2),
    LUT_MAKE(1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2),
    LUT_MAKE(1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2),
    LUT_MAKE(1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2),
    LUT_MAKE(1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2),
    LUT_MAKE(1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2),
    LUT_MAKE(1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2),
    0u,
};

// Differential update + AA grey nudge, 21 drive frames.
// Level 6 parks after 8 frames, level 11 after 12.
constexpr uint32_t kFast21_6at8_11at12[] = {
    LUT_MAKE(1, 3, 3, 3, 3, 3, 2, 3, 3, 3, 3, 2, 3, 3, 3, 2),
    LUT_MAKE(1, 3, 3, 3, 3, 3, 2, 3, 3, 3, 3, 2, 3, 3, 3, 2),
    LUT_MAKE(1, 3, 3, 3, 3, 3, 2, 3, 3, 3, 3, 2, 3, 3, 3, 2),
    LUT_MAKE(1, 3, 3, 3, 3, 3, 2, 3, 3, 3, 3, 2, 3, 3, 3, 2),
    LUT_MAKE(1, 3, 3, 3, 3, 3, 2, 3, 3, 3, 3, 2, 3, 3, 3, 2),
    LUT_MAKE(1, 3, 3, 3, 3, 3, 2, 3, 3, 3, 3, 2, 3, 3, 3, 2),
    LUT_MAKE(1, 3, 3, 3, 3, 3, 2, 3, 3, 3, 3, 2, 3, 3, 3, 2),
    LUT_MAKE(1, 3, 3, 3, 3, 3, 2, 3, 3, 3, 3, 2, 3, 3, 3, 2),
    LUT_MAKE(1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2, 3, 3, 3, 2),
    LUT_MAKE(1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2, 3, 3, 3, 2),
    LUT_MAKE(1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2, 3, 3, 3, 2),
    LUT_MAKE(1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2, 3, 3, 3, 2),
    LUT_MAKE(1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2),
    LUT_MAKE(1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2),
    LUT_MAKE(1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2),
    LUT_MAKE(1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2),
    LUT_MAKE(1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2),
    LUT_MAKE(1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2),
    LUT_MAKE(1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2),
    LUT_MAKE(1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2),
    LUT_MAKE(1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2),
    0u,
};

// Differential update + AA grey nudge, 21 drive frames.
// Level 7 parks after 8 frames, level 12 after 12.
constexpr uint32_t kFast21_7at8_12at12[] = {
    LUT_MAKE(1, 3, 3, 3, 3, 3, 3, 2, 3, 3, 3, 3, 2, 3, 3, 2),
    LUT_MAKE(1, 3, 3, 3, 3, 3, 3, 2, 3, 3, 3, 3, 2, 3, 3, 2),
    LUT_MAKE(1, 3, 3, 3, 3, 3, 3, 2, 3, 3, 3, 3, 2, 3, 3, 2),
    LUT_MAKE(1, 3, 3, 3, 3, 3, 3, 2, 3, 3, 3, 3, 2, 3, 3, 2),
    LUT_MAKE(1, 3, 3, 3, 3, 3, 3, 2, 3, 3, 3, 3, 2, 3, 3, 2),
    LUT_MAKE(1, 3, 3, 3, 3, 3, 3, 2, 3, 3, 3, 3, 2, 3, 3, 2),
    LUT_MAKE(1, 3, 3, 3, 3, 3, 3, 2, 3, 3, 3, 3, 2, 3, 3, 2),
    LUT_MAKE(1, 3, 3, 3, 3, 3, 3, 2, 3, 3, 3, 3, 2, 3, 3, 2),
    LUT_MAKE(1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2, 3, 3, 2),
    LUT_MAKE(1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2, 3, 3, 2),
    LUT_MAKE(1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2, 3, 3, 2),
    LUT_MAKE(1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2, 3, 3, 2),
    LUT_MAKE(1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2),
    LUT_MAKE(1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2),
    LUT_MAKE(1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2),
    LUT_MAKE(1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2),
    LUT_MAKE(1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2),
    LUT_MAKE(1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2),
    LUT_MAKE(1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2),
    LUT_MAKE(1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2),
    LUT_MAKE(1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2),
    0u,
};

// Differential update + AA grey nudge, 17 drive frames.
// Level 5 parks after 6 frames, level 12 after 12.
constexpr uint32_t kFast17_5at6_12at12[] = {
    LUT_MAKE(1, 3, 3, 3, 3, 2, 3, 3, 3, 3, 3, 3, 2, 3, 3, 2),
    LUT_MAKE(1, 3, 3, 3, 3, 2, 3, 3, 3, 3, 3, 3, 2, 3, 3, 2),
    LUT_MAKE(1, 3, 3, 3, 3, 2, 3, 3, 3, 3, 3, 3, 2, 3, 3, 2),
    LUT_MAKE(1, 3, 3, 3, 3, 2, 3, 3, 3, 3, 3, 3, 2, 3, 3, 2),
    LUT_MAKE(1, 3, 3, 3, 3, 2, 3, 3, 3, 3, 3, 3, 2, 3, 3, 2),
    LUT_MAKE(1, 3, 3, 3, 3, 2, 3, 3, 3, 3, 3, 3, 2, 3, 3, 2),
    LUT_MAKE(1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2, 3, 3, 2),
    LUT_MAKE(1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2, 3, 3, 2),
    LUT_MAKE(1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2, 3, 3, 2),
    LUT_MAKE(1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2, 3, 3, 2),
    LUT_MAKE(1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2, 3, 3, 2),
    LUT_MAKE(1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2, 3, 3, 2),
    LUT_MAKE(1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2),
    LUT_MAKE(1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2),
    LUT_MAKE(1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2),
    LUT_MAKE(1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2),
    LUT_MAKE(1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2),
    0u,
};

// Differential update + AA grey nudge, 16 drive frames.
// Level 5 parks after 6 frames, level 11 after 10.
constexpr uint32_t kFast16_5at6_11at10[] = {
    LUT_MAKE(1, 3, 3, 3, 3, 2, 3, 3, 3, 3, 3, 2, 3, 3, 3, 2),
    LUT_MAKE(1, 3, 3, 3, 3, 2, 3, 3, 3, 3, 3, 2, 3, 3, 3, 2),
    LUT_MAKE(1, 3, 3, 3, 3, 2, 3, 3, 3, 3, 3, 2, 3, 3, 3, 2),
    LUT_MAKE(1, 3, 3, 3, 3, 2, 3, 3, 3, 3, 3, 2, 3, 3, 3, 2),
    LUT_MAKE(1, 3, 3, 3, 3, 2, 3, 3, 3, 3, 3, 2, 3, 3, 3, 2),
    LUT_MAKE(1, 3, 3, 3, 3, 2, 3, 3, 3, 3, 3, 2, 3, 3, 3, 2),
    LUT_MAKE(1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2, 3, 3, 3, 2),
    LUT_MAKE(1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2, 3, 3, 3, 2),
    LUT_MAKE(1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2, 3, 3, 3, 2),
    LUT_MAKE(1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2, 3, 3, 3, 2),
    LUT_MAKE(1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2),
    LUT_MAKE(1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2),
    LUT_MAKE(1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2),
    LUT_MAKE(1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2),
    LUT_MAKE(1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2),
    LUT_MAKE(1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2),
    0u,
};

// Differential update + AA grey nudge, 14 drive frames.
// Level 5 parks after 6 frames, level 8 after 9.
constexpr uint32_t kFast14_5at6_8at9[] = {
    LUT_MAKE(1, 3, 3, 3, 3, 2, 3, 3, 2, 3, 3, 3, 3, 3, 3, 2),
    LUT_MAKE(1, 3, 3, 3, 3, 2, 3, 3, 2, 3, 3, 3, 3, 3, 3, 2),
    LUT_MAKE(1, 3, 3, 3, 3, 2, 3, 3, 2, 3, 3, 3, 3, 3, 3, 2),
    LUT_MAKE(1, 3, 3, 3, 3, 2, 3, 3, 2, 3, 3, 3, 3, 3, 3, 2),
    LUT_MAKE(1, 3, 3, 3, 3, 2, 3, 3, 2, 3, 3, 3, 3, 3, 3, 2),
    LUT_MAKE(1, 3, 3, 3, 3, 2, 3, 3, 2, 3, 3, 3, 3, 3, 3, 2),
    LUT_MAKE(1, 3, 3, 3, 3, 3, 3, 3, 2, 3, 3, 3, 3, 3, 3, 2),
    LUT_MAKE(1, 3, 3, 3, 3, 3, 3, 3, 2, 3, 3, 3, 3, 3, 3, 2),
    LUT_MAKE(1, 3, 3, 3, 3, 3, 3, 3, 2, 3, 3, 3, 3, 3, 3, 2),
    LUT_MAKE(1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2),
    LUT_MAKE(1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2),
    LUT_MAKE(1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2),
    LUT_MAKE(1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2),
    LUT_MAKE(1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2),
    0u,
};

// Clean refresh, 24 frames away from the target then 24 back.
constexpr uint32_t kClean24[] = {
    LUT_MAKE(2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1),
    LUT_MAKE(2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1),
    LUT_MAKE(2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1),
    LUT_MAKE(2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1),
    LUT_MAKE(2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1),
    LUT_MAKE(2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1),
    LUT_MAKE(2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1),
    LUT_MAKE(2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1),
    LUT_MAKE(2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1),
    LUT_MAKE(2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1),
    LUT_MAKE(2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1),
    LUT_MAKE(2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1),
    LUT_MAKE(2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1),
    LUT_MAKE(2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1),
    LUT_MAKE(2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1),
    LUT_MAKE(2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1),
    LUT_MAKE(2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1),
    LUT_MAKE(2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1),
    LUT_MAKE(2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1),
    LUT_MAKE(2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1),
    LUT_MAKE(2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1),
    LUT_MAKE(2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1),
    LUT_MAKE(2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1),
    LUT_MAKE(2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1),
    LUT_MAKE(1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2),
    LUT_MAKE(1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2),
    LUT_MAKE(1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2),
    LUT_MAKE(1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2),
    LUT_MAKE(1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2),
    LUT_MAKE(1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2),
    LUT_MAKE(1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2),
    LUT_MAKE(1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2),
    LUT_MAKE(1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2),
    LUT_MAKE(1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2),
    LUT_MAKE(1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2),
    LUT_MAKE(1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2),
    LUT_MAKE(1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2),
    LUT_MAKE(1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2),
    LUT_MAKE(1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2),
    LUT_MAKE(1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2),
    LUT_MAKE(1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2),
    LUT_MAKE(1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2),
    LUT_MAKE(1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2),
    LUT_MAKE(1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2),
    LUT_MAKE(1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2),
    LUT_MAKE(1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2),
    LUT_MAKE(1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2),
    LUT_MAKE(1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2),
    0u,
};

// Clean refresh, 21 frames away from the target then 21 back.
constexpr uint32_t kClean21[] = {
    LUT_MAKE(2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1),
    LUT_MAKE(2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1),
    LUT_MAKE(2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1),
    LUT_MAKE(2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1),
    LUT_MAKE(2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1),
    LUT_MAKE(2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1),
    LUT_MAKE(2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1),
    LUT_MAKE(2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1),
    LUT_MAKE(2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1),
    LUT_MAKE(2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1),
    LUT_MAKE(2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1),
    LUT_MAKE(2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1),
    LUT_MAKE(2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1),
    LUT_MAKE(2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1),
    LUT_MAKE(2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1),
    LUT_MAKE(2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1),
    LUT_MAKE(2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1),
    LUT_MAKE(2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1),
    LUT_MAKE(2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1),
    LUT_MAKE(2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1),
    LUT_MAKE(2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1),
    LUT_MAKE(1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2),
    LUT_MAKE(1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2),
    LUT_MAKE(1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2),
    LUT_MAKE(1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2),
    LUT_MAKE(1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2),
    LUT_MAKE(1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2),
    LUT_MAKE(1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2),
    LUT_MAKE(1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2),
    LUT_MAKE(1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2),
    LUT_MAKE(1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2),
    LUT_MAKE(1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2),
    LUT_MAKE(1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2),
    LUT_MAKE(1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2),
    LUT_MAKE(1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2),
    LUT_MAKE(1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2),
    LUT_MAKE(1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2),
    LUT_MAKE(1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2),
    LUT_MAKE(1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2),
    LUT_MAKE(1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2),
    LUT_MAKE(1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2),
    LUT_MAKE(1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2),
    0u,
};

// Clean refresh, 17 frames away from the target then 17 back.
constexpr uint32_t kClean17[] = {
    LUT_MAKE(2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1),
    LUT_MAKE(2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1),
    LUT_MAKE(2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1),
    LUT_MAKE(2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1),
    LUT_MAKE(2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1),
    LUT_MAKE(2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1),
    LUT_MAKE(2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1),
    LUT_MAKE(2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1),
    LUT_MAKE(2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1),
    LUT_MAKE(2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1),
    LUT_MAKE(2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1),
    LUT_MAKE(2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1),
    LUT_MAKE(2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1),
    LUT_MAKE(2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1),
    LUT_MAKE(2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1),
    LUT_MAKE(2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1),
    LUT_MAKE(2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1),
    LUT_MAKE(1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2),
    LUT_MAKE(1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2),
    LUT_MAKE(1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2),
    LUT_MAKE(1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2),
    LUT_MAKE(1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2),
    LUT_MAKE(1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2),
    LUT_MAKE(1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2),
    LUT_MAKE(1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2),
    LUT_MAKE(1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2),
    LUT_MAKE(1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2),
    LUT_MAKE(1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2),
    LUT_MAKE(1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2),
    LUT_MAKE(1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2),
    LUT_MAKE(1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2),
    LUT_MAKE(1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2),
    LUT_MAKE(1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2),
    LUT_MAKE(1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2),
    0u,
};

// Clean refresh, 16 frames away from the target then 16 back.
constexpr uint32_t kClean16[] = {
    LUT_MAKE(2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1),
    LUT_MAKE(2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1),
    LUT_MAKE(2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1),
    LUT_MAKE(2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1),
    LUT_MAKE(2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1),
    LUT_MAKE(2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1),
    LUT_MAKE(2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1),
    LUT_MAKE(2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1),
    LUT_MAKE(2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1),
    LUT_MAKE(2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1),
    LUT_MAKE(2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1),
    LUT_MAKE(2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1),
    LUT_MAKE(2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1),
    LUT_MAKE(2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1),
    LUT_MAKE(2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1),
    LUT_MAKE(2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1),
    LUT_MAKE(1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2),
    LUT_MAKE(1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2),
    LUT_MAKE(1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2),
    LUT_MAKE(1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2),
    LUT_MAKE(1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2),
    LUT_MAKE(1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2),
    LUT_MAKE(1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2),
    LUT_MAKE(1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2),
    LUT_MAKE(1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2),
    LUT_MAKE(1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2),
    LUT_MAKE(1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2),
    LUT_MAKE(1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2),
    LUT_MAKE(1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2),
    LUT_MAKE(1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2),
    LUT_MAKE(1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2),
    LUT_MAKE(1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2),
    0u,
};

// Clean refresh, 14 frames away from the target then 14 back.
constexpr uint32_t kClean14[] = {
    LUT_MAKE(2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1),
    LUT_MAKE(2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1),
    LUT_MAKE(2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1),
    LUT_MAKE(2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1),
    LUT_MAKE(2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1),
    LUT_MAKE(2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1),
    LUT_MAKE(2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1),
    LUT_MAKE(2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1),
    LUT_MAKE(2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1),
    LUT_MAKE(2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1),
    LUT_MAKE(2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1),
    LUT_MAKE(2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1),
    LUT_MAKE(2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1),
    LUT_MAKE(2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1),
    LUT_MAKE(1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2),
    LUT_MAKE(1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2),
    LUT_MAKE(1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2),
    LUT_MAKE(1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2),
    LUT_MAKE(1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2),
    LUT_MAKE(1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2),
    LUT_MAKE(1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2),
    LUT_MAKE(1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2),
    LUT_MAKE(1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2),
    LUT_MAKE(1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2),
    LUT_MAKE(1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2),
    LUT_MAKE(1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2),
    LUT_MAKE(1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2),
    LUT_MAKE(1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2),
    0u,
};

#undef LUT_MAKE

}  // namespace

const TempRange kTempRanges[kTempRangeCount] = {
    {15, 18},
    {18, 21},
    {21, 24},
    {24, 27},
    {27, 30},
    {30, 33},
    {33, 38},
};

// The canvas levels the AA grey columns are cut for, per temperature range. A
// board config turns these into the grey bytes its canvas writes; they are not
// independently tunable -- retarget them in tools/gen_ed047tc2_waveform.py and
// regenerate, or the canvas will address a column the LUT does not drive.
const uint8_t kGrayLevelDark[kTempRangeCount] = {10, 6, 6, 7, 5, 5, 5};

const uint8_t kGrayLevelLight[kTempRangeCount] = {12, 12, 11, 12, 12, 11, 8};

const uint32_t* const kFastLut[kTempRangeCount] = {
    kFast24_10at9_12at15,
    kFast21_6at8_12at13,
    kFast21_6at8_11at12,
    kFast21_7at8_12at12,
    kFast17_5at6_12at12,
    kFast16_5at6_11at10,
    kFast14_5at6_8at9,
};

const size_t kFastLutStep[kTempRangeCount] = {
    sizeof(kFast24_10at9_12at15) / sizeof(kFast24_10at9_12at15[0]),
    sizeof(kFast21_6at8_12at13) / sizeof(kFast21_6at8_12at13[0]),
    sizeof(kFast21_6at8_11at12) / sizeof(kFast21_6at8_11at12[0]),
    sizeof(kFast21_7at8_12at12) / sizeof(kFast21_7at8_12at12[0]),
    sizeof(kFast17_5at6_12at12) / sizeof(kFast17_5at6_12at12[0]),
    sizeof(kFast16_5at6_11at10) / sizeof(kFast16_5at6_11at10[0]),
    sizeof(kFast14_5at6_8at9) / sizeof(kFast14_5at6_8at9[0]),
};

const uint32_t* const kCleanLut[kTempRangeCount] = {
    kClean24,
    kClean21,
    kClean21,
    kClean21,
    kClean17,
    kClean16,
    kClean14,
};

const size_t kCleanLutStep[kTempRangeCount] = {
    sizeof(kClean24) / sizeof(kClean24[0]),
    sizeof(kClean21) / sizeof(kClean21[0]),
    sizeof(kClean21) / sizeof(kClean21[0]),
    sizeof(kClean21) / sizeof(kClean21[0]),
    sizeof(kClean17) / sizeof(kClean17[0]),
    sizeof(kClean16) / sizeof(kClean16[0]),
    sizeof(kClean14) / sizeof(kClean14[0]),
};

const uint8_t kDriveFrames[kTempRangeCount] = {24, 21, 21, 21, 17, 16, 14};

size_t tempRangeIndex(int tempC) {
  for (size_t i = 0; i < kTempRangeCount; ++i) {
    if (tempC < kTempRanges[i].maxC) return i;
  }
  return kTempRangeCount - 1;
}

}  // namespace ed047tc2
}  // namespace freeink
