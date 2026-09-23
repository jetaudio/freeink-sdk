#pragma once

#include <Arduino.h>

namespace freeink {

// Board-supplied power glue, called from the LovyanGFX bus lifecycle. The board
// implements these (for example PCA9535 expander + TPS65185 PMIC on LilyGo T5 S3)
// and injects them in its LgfxEpdConfig. Any hook may be null.
struct LgfxEpdPowerHooks {
  bool (*prepare)();
  bool (*powerOn)();
  void (*powerOff)();
};

// LovyanGFX parallel-EPD wiring. Geometry is not here; it comes from the active
// BoardProfile, like all drivers. This carries only bus/panel specifics.
struct LgfxEpdConfig {
  int8_t dataPins[8];
  int8_t pinSph;
  int8_t pinSpv;
  int8_t pinOe;
  int8_t pinLe;
  int8_t pinCl;
  int8_t pinCkv;
  int8_t pinPwr;
  uint32_t busHz;
  uint8_t linePadding;
  uint8_t rotation;
  LgfxEpdPowerHooks power;
  const uint32_t* lutQuality = nullptr;
  size_t lutQualityStep = 0;
  const uint32_t* lutText = nullptr;
  size_t lutTextStep = 0;
  const uint32_t* lutFast = nullptr;
  size_t lutFastStep = 0;
  const uint32_t* lutFastest = nullptr;
  size_t lutFastestStep = 0;

  // The 8-bit canvas values the driver writes for the two anti-aliasing greys.
  // Panel_EPD quantises a canvas byte to a 4-bit level as (v + bayer - 8) >> 4,
  // so only v == (level << 4) | 8 lands on one level for every cell of the Bayer
  // matrix; anything else alternates between two levels and shows up as speckle
  // along glyph edges. grayLevelByte() builds a safe value from a level.
  //
  // Which level to ask for is a property of the board's waveform: the LUT drives
  // grey destinations by column, so a canvas level the LUT has no column for
  // simply stays at whatever the B/W base left it on.
  //
  // The defaults are the even thirds this driver has always written. They are not
  // Bayer-exact, which does not matter on a board that leaves
  // grayNudgeInFastBank false: that path dithers the canvas anyway, and these are
  // the densities it has been dithering since the driver was written.
  uint8_t grayDark = 0x55;
  uint8_t grayLight = 0xAA;

  // True when the board's epd_fast LUT carries grey columns as well as the two
  // B/W rails, so the grayscale push can go out through the differential bank:
  // no lut_eraser flash, and the same bank the B/W base used, which is what lets
  // Panel_EPD's per-pixel diff keep skipping everything that did not change.
  //
  // Left false for a board on LovyanGFX's stock LUTs, whose fast bank drives only
  // the rails. Those boards keep the plain epd_fast push they have always had --
  // the greys still come out dithered there, which is a waveform gap on that
  // board, not something this driver can paper over.
  bool grayNudgeInFastBank = false;
  // True when this panel's clean bank only scrubs the whole screen if the refresh
  // BEFORE it did not also use the clean bank.
  //
  // The mechanism is Panel_EPD's, not the board's: its epd_text branch drives a
  // pixel unless it was already REQUESTED WHITE under that same bank and is
  // requested white again (Panel_EPD.cpp, the `white != d1 || d1 != s0` test, in
  // which `white` embeds the bank's own LUT offset). After a push through any
  // other bank every pixel compares unequal and the whole screen is driven; after
  // another clean push the untouched white background is skipped, so only the
  // union of the old and the new ink is driven. On a bank that rail-normalizes
  // before it lands -- black, then white, then down to the level -- that reads on
  // the glass as both pages standing at once, and the old ink then settles beside
  // a background that was never driven, leaving its shape as a faint imprint.
  //
  // Set it on a board whose clean table works that way. The driver's answer is
  // normalizeForCleanBank(), which re-tags the background rather than re-driving
  // it; see LgfxEpdDriver.cpp for why that is nearly free.
  //
  // Left false for a board on LovyanGFX's stock LUTs. Such a board keeps today's
  // behaviour exactly: Half and Full both take the clean bank, with no
  // normalizing pass.
  bool cleanBankNeedsFreshBackground = false;
};

// Canvas byte that quantises to exactly `level` for every Bayer cell.
constexpr uint8_t grayLevelByte(uint8_t level) {
  return static_cast<uint8_t>((level << 4) | 8);
}

}  // namespace freeink
