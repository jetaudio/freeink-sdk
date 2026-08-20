#pragma once

// ED047TC2 vendor waveform, in the LUT form LovyanGFX's Panel_EPD consumes.
//
// The panel on the LilyGo T5 S3 Pro has no on-glass controller: the waveform is
// the firmware's responsibility, and getting it wrong costs contrast and leaves
// ghosts. The tables in ED047TC2Waveform.cpp are generated from the panel
// vendor's own waveform blob by tools/gen_ed047tc2_waveform.py, which refuses to
// emit anything unless the blob still matches the structure it decodes. Do not
// hand-edit the generated file.
//
// The vendor waveform is specified per ambient temperature range, and the drive
// length grows as the panel gets colder. Pick the LUT with tempRangeIndex() from
// a real panel temperature rather than assuming room temperature;
// LilyGoT5S3LgfxConfig.cpp reads one from the TPS65185's thermistor.

#include <Arduino.h>

namespace freeink {
namespace ed047tc2 {

// Temperature ranges the vendor waveform covers, coldest first. The blob carries
// 15..38 C; tempRangeIndex() clamps anything outside that to the nearest end.
constexpr size_t kTempRangeCount = 7;

struct TempRange {
  int8_t minC;
  int8_t maxC;
};

extern const TempRange kTempRanges[kTempRangeCount];

// The vendor's DU (two-level) waveform in each range, for Panel_EPD's
// differential modes (epd_fast / epd_fastest).
extern const uint32_t* const kDuLut[kTempRangeCount];
extern const size_t kDuLutStep[kTempRangeCount];

// The clean refresh, for epd_text / epd_quality. Those modes re-drive every
// non-white pixel whether or not it changed, so this one is charge neutral: it
// drives each level away from its target and back again, which nets zero and
// puts every re-driven pixel through a full rail-to-rail excursion.
extern const uint32_t* const kCleanLut[kTempRangeCount];
extern const size_t kCleanLutStep[kTempRangeCount];

// Frames of drive a full black<->white transition takes in each range. Exposed
// for logging and for sizing refresh timeouts.
extern const uint8_t kDriveFrames[kTempRangeCount];

// Range covering tempC, clamped to the ends of the table.
size_t tempRangeIndex(int tempC);

}  // namespace ed047tc2
}  // namespace freeink
