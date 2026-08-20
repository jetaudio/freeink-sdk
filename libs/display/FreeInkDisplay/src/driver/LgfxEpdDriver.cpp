#include "LgfxEpdDriver.h"

#include <BoardConfig.h>

#include <cstring>

#if FREEINK_DRIVER_LGFX_EPD
#include <M5GFX.h>  // pulls LovyanGFX; added to lib_deps only on the LilyGo env
#include <esp_heap_caps.h>
#include <lgfx/v1/platforms/esp32/Bus_EPD.h>
#include <lgfx/v1/platforms/esp32/Panel_EPD.hpp>
#endif

namespace freeink {

#if FREEINK_DRIVER_LGFX_EPD
namespace {

// Set from the active config in begin(), read by the bus subclass below. The
// driver is a singleton (one panel), so a file-scope pointer is fine and mirrors
// how M5GFX/LovyanGFX use global device objects.
const LgfxEpdPowerHooks* g_hooks = nullptr;

// Bus subclass that defers the board's power topology to injected hooks. Matches
// the two override points LovyanGFX exposes: init() (pin setup) and
// powerControl() (rail up/down), guarding the _pwr_on state itself. A board
// whose rails are plain GPIOs (pinOe/pinPwr/pinSpv, e.g. M5Stack PaperS3) leaves
// the corresponding hook null and gets Bus_EPD's stock power sequence instead;
// a board with external power silicon (LilyGo's TPS65185 + PCA9535) hooks it.
class FreeInkBusEPD : public lgfx::Bus_EPD {
 public:
  bool init() override {
    if (g_hooks && g_hooks->prepare && !g_hooks->prepare()) return false;
    return lgfx::Bus_EPD::init();
  }

  bool powerControl(const bool powerOn) override {
    if (_pwr_on == powerOn) return true;
    const bool hooked = g_hooks && (powerOn ? g_hooks->powerOn != nullptr : g_hooks->powerOff != nullptr);
    if (!hooked) return lgfx::Bus_EPD::powerControl(powerOn);
    wait();
    if (powerOn) {
      if (!g_hooks->powerOn()) return false;
      _pwr_on = true;
      return true;
    }
    g_hooks->powerOff();
    _pwr_on = false;
    return true;
  }
};

class FreeInkLgfxEpd : public lgfx::LGFX_Device {
 public:
  void setup(const LgfxEpdConfig& c, uint16_t w, uint16_t h) {
    auto bc = _bus.config();
    bc.bus_speed = c.busHz;
    for (int i = 0; i < 8; ++i) bc.pin_data[i] = c.dataPins[i];
    bc.pin_pwr = c.pinPwr;
    bc.pin_sph = c.pinSph;
    bc.pin_spv = c.pinSpv;
    bc.pin_oe = c.pinOe;
    bc.pin_le = c.pinLe;
    bc.pin_cl = c.pinCl;
    bc.pin_ckv = c.pinCkv;
    bc.bus_width = 8;
    _bus.config(bc);

    _panel.setBus(&_bus);

    auto dc = _panel.config_detail();
    dc.line_padding = c.linePadding;
    if (c.lutQuality && c.lutQualityStep) {
      dc.lut_quality = c.lutQuality;
      dc.lut_quality_step = c.lutQualityStep;
    }
    if (c.lutText && c.lutTextStep) {
      dc.lut_text = c.lutText;
      dc.lut_text_step = c.lutTextStep;
    }
    if (c.lutFast && c.lutFastStep) {
      dc.lut_fast = c.lutFast;
      dc.lut_fast_step = c.lutFastStep;
    }
    if (c.lutFastest && c.lutFastestStep) {
      dc.lut_fastest = c.lutFastest;
      dc.lut_fastest_step = c.lutFastestStep;
    }
    _panel.config_detail(dc);

    auto pc = _panel.config();
    pc.memory_width = pc.panel_width = w;
    pc.memory_height = pc.panel_height = h;
    pc.offset_rotation = 0;
    pc.offset_x = 0;
    pc.offset_y = 0;
    pc.bus_shared = false;
    _panel.config(pc);

    setPanel(&_panel);
  }

 private:
  FreeInkBusEPD _bus;
  lgfx::Panel_EPD _panel;
};

FreeInkLgfxEpd g_dev;

lgfx::epd_mode::epd_mode_t epdModeFor(RefreshMode m, bool twoLevelWaveform) {
  // A board that supplies one two-level panel waveform has nothing to switch to:
  // every mode holds the same LUT, and changing epd_mode would defeat the diff
  // (see LgfxEpdConfig::twoLevelWaveform). Clean refreshes flash instead.
  if (twoLevelWaveform) return lgfx::epd_mode::epd_fast;
  switch (m) {
    case RefreshMode::Full: return lgfx::epd_mode::epd_text;
    case RefreshMode::Half: return lgfx::epd_mode::epd_text;
    default: return lgfx::epd_mode::epd_fast;
  }
}

// 8-bit gray canvas (PSRAM) the panel pushes from, plus the two 1-bpp planes the
// facade streams for grayscale. Allocated once in begin().
lgfx::LGFX_Sprite* g_canvas = nullptr;
uint8_t* g_lsb = nullptr;
uint8_t* g_msb = nullptr;
uint16_t g_w = 0, g_h = 0, g_wb = 0;

constexpr uint8_t kGrayBlack = 0x00, kGrayDark = 0x55, kGrayLight = 0xAA, kGrayWhite = 0xFF;

// (base, lsb, msb) per pixel -> 4 gray levels, matching the reference port: a set
// base bit is white; otherwise msb/lsb pick light/dark; clear is black.
inline uint8_t grayValue(uint8_t base, uint8_t lsb, uint8_t msb, uint8_t mask) {
  if (base & mask) return kGrayWhite;
  const bool l = lsb & mask, m = msb & mask;
  if (m && l) return kGrayDark;
  if (m) return kGrayLight;
  if (l) return kGrayDark;
  return kGrayBlack;
}

void allocCanvas(uint16_t w, uint16_t h) {
  g_w = w;
  g_h = h;
  g_wb = w / 8;
  if (!g_canvas) {
    g_canvas = new lgfx::LGFX_Sprite(&g_dev);
    g_canvas->setPsram(true);
    g_canvas->setColorDepth(lgfx::color_depth_t::grayscale_8bit);
    g_canvas->createSprite(w, h);
  }
  const size_t planeBytes = static_cast<size_t>(g_wb) * h;
  if (!g_lsb) g_lsb = static_cast<uint8_t*>(heap_caps_malloc(planeBytes, MALLOC_CAP_SPIRAM));
  if (!g_msb) g_msb = static_cast<uint8_t*>(heap_caps_malloc(planeBytes, MALLOC_CAP_SPIRAM));
}

// Expand a 1-bpp B/W frame (bit set = white) into the 8-bit gray canvas.
void fillCanvasBW(const uint8_t* fb) {
  if (!g_canvas) return;
  auto* dst = static_cast<uint8_t*>(g_canvas->getBuffer());
  if (!dst) return;
  for (uint16_t y = 0; y < g_h; ++y) {
    const uint8_t* src = fb + static_cast<uint32_t>(y) * g_wb;
    uint8_t* drow = dst + static_cast<uint32_t>(y) * g_w;
    for (uint16_t bx = 0; bx < g_wb; ++bx) {
      const uint8_t b = src[bx];
      for (uint8_t bit = 0; bit < 8; ++bit) drow[bx * 8 + bit] = (b & (0x80 >> bit)) ? kGrayWhite : kGrayBlack;
    }
  }
}

// Combine the B/W base + buffered LSB/MSB planes into the 8-bit gray canvas.
void fillCanvasGray(const uint8_t* base) {
  if (!g_canvas || !g_lsb || !g_msb) return;
  auto* dst = static_cast<uint8_t*>(g_canvas->getBuffer());
  if (!dst) return;
  for (uint16_t y = 0; y < g_h; ++y) {
    const uint8_t* brow = base + static_cast<uint32_t>(y) * g_wb;
    const uint8_t* lrow = g_lsb + static_cast<uint32_t>(y) * g_wb;
    const uint8_t* mrow = g_msb + static_cast<uint32_t>(y) * g_wb;
    uint8_t* drow = dst + static_cast<uint32_t>(y) * g_w;
    for (uint16_t bx = 0; bx < g_wb; ++bx) {
      for (uint8_t bit = 0; bit < 8; ++bit) {
        const uint8_t mask = 0x80 >> bit;
        drow[bx * 8 + bit] = grayValue(brow[bx], lrow[bx], mrow[bx], mask);
      }
    }
  }
}

// Queue a frame and return; Panel_EPD drives the waveform from its own task.
//
// Waiting the refresh out here would park the Arduino loop for its whole
// duration, and that loop is the only thing that samples input: the GT911 is
// polled over I2C with no interrupt, and InputManager::beginAsync (which would
// queue events from a background task) is never started. A button survives the
// gap because it is still physically down when polling resumes, but a tap is a
// transient -- finger down and up inside the gap leaves no trace at all, so the
// press is silently lost and the user has to tap again.
//
// The wait at the top is what keeps this safe. pushSprite() has already copied
// the canvas into Panel_EPD's _buf by the time it returns, so the caller's
// framebuffer and the canvas are both free immediately; only _buf must not be
// rewritten before the panel task has consumed it, which is exactly what the
// leading wait enforces. Anything that drops the rails must waitDisplay() first
// -- see sleepPanel().
void pushCanvas(lgfx::epd_mode::epd_mode_t epdMode) {
  if (!g_canvas) return;
  g_dev.waitDisplay();
  g_dev.setEpdMode(epdMode);
  g_canvas->pushSprite(0, 0);  // commits to _buf; the panel task runs the refresh
}

// Park the panel. The refresh has to finish before the rails drop, or the frame
// is left half-driven on the glass.
void sleepPanel() {
  g_dev.waitDisplay();
  g_dev.sleep();
}

// Ghost-clearing pass for a clean refresh on a two-level waveform: flash the
// whole screen white, then black, and let the caller's frame bring the white
// pixels back.
//
// It takes both flashes, because Panel_EPD only drives pixels whose value
// changes and a single flash can therefore never touch the whole screen. Flash
// to black and every pixel that is already black -- the text -- is skipped and
// left at whatever charge a long run of differential updates put there. Flash to
// white and the background is the part that gets skipped instead. Either way the
// pixels that were skipped keep their residue, which is precisely the ghosting a
// clean refresh exists to clear.
//
// Two flashes leave nothing out. The first drives every dark pixel up, which
// makes the screen uniformly white; the second then changes *every* pixel, so
// every pixel takes a full white->black transition. The caller's frame push
// afterwards drives the white ones back up.
//
// The impulse still balances. Writing L for the frames one transition costs:
// a pixel that ends white took 0/-L/+L if it was already white and +L/-L/+L if it
// was black, and one that ends black took 0/-L/0 or +L/-L/0 -- which is 0 or +L
// for white and -L or 0 for black, exactly what the panel's own waveform asks for
// each of those transitions.
void flashCanvas() {
  if (!g_canvas) return;
  auto* dst = static_cast<uint8_t*>(g_canvas->getBuffer());
  if (!dst) return;
  const size_t pixels = static_cast<size_t>(g_w) * g_h;
  memset(dst, kGrayWhite, pixels);
  pushCanvas(lgfx::epd_mode::epd_fast);
  memset(dst, kGrayBlack, pixels);
  pushCanvas(lgfx::epd_mode::epd_fast);
}

}  // namespace
#endif  // FREEINK_DRIVER_LGFX_EPD

LgfxEpdDriver::LgfxEpdDriver(const LgfxEpdConfig& cfg) : _cfg(cfg) {}

PanelGeometry LgfxEpdDriver::geometry() const {
  const uint16_t w = BoardConfig::ACTIVE.displayWidth;
  const uint16_t h = BoardConfig::ACTIVE.displayHeight;
  const uint16_t wb = w / 8;
  return {w, h, wb, static_cast<uint32_t>(wb) * h};
}

void LgfxEpdDriver::begin(EpdBus& bus) {
  (void)bus;
#if FREEINK_DRIVER_LGFX_EPD
  g_hooks = &_cfg.power;
  g_dev.setup(_cfg, BoardConfig::ACTIVE.displayWidth, BoardConfig::ACTIVE.displayHeight);
  g_dev.init();
  g_dev.setRotation(_cfg.rotation);
  g_dev.setEpdMode(lgfx::epd_mode::epd_fast);
  allocCanvas(BoardConfig::ACTIVE.displayWidth, BoardConfig::ACTIVE.displayHeight);
#endif
}

void LgfxEpdDriver::display(EpdBus& bus, const uint8_t* fb, const uint8_t* prev, RefreshMode mode, bool turnOff) {
  (void)bus;
  (void)prev;
#if FREEINK_DRIVER_LGFX_EPD
  // A clean refresh on a two-level waveform flashes to black first; the frame
  // push below then drives every white pixel back through a full transition.
  if (_cfg.twoLevelWaveform && mode != RefreshMode::Fast) flashCanvas();
  fillCanvasBW(fb);          // expand the 1-bpp frame into the gray canvas
  pushCanvas(epdModeFor(mode, _cfg.twoLevelWaveform));
  if (turnOff) sleepPanel();
#else
  (void)fb;
  (void)mode;
  (void)turnOff;
#endif
}

void LgfxEpdDriver::copyGrayscaleLsb(EpdBus& bus, const uint8_t* lsb) {
  (void)bus;
#if FREEINK_DRIVER_LGFX_EPD
  if (g_lsb && lsb) memcpy(g_lsb, lsb, static_cast<size_t>(g_wb) * g_h);
#else
  (void)lsb;
#endif
}

void LgfxEpdDriver::copyGrayscaleMsb(EpdBus& bus, const uint8_t* msb) {
  (void)bus;
#if FREEINK_DRIVER_LGFX_EPD
  if (g_msb && msb) memcpy(g_msb, msb, static_cast<size_t>(g_wb) * g_h);
#else
  (void)msb;
#endif
}

void LgfxEpdDriver::writeGrayscalePlaneStrip(EpdBus& bus, GrayPlane plane, const uint8_t* rows, uint16_t yStart,
                                             uint16_t numRows) {
  (void)bus;
#if FREEINK_DRIVER_LGFX_EPD
  uint8_t* dstPlane = (plane == GrayPlane::Lsb) ? g_lsb : g_msb;
  if (!dstPlane || !rows) return;
  const uint32_t offset = static_cast<uint32_t>(yStart) * g_wb;
  memcpy(dstPlane + offset, rows, static_cast<size_t>(numRows) * g_wb);
#else
  (void)plane;
  (void)rows;
  (void)yStart;
  (void)numRows;
#endif
}

void LgfxEpdDriver::displayGray(EpdBus& bus, const uint8_t* fb, bool turnOff, const unsigned char* lut,
                                bool factoryMode) {
  (void)bus;
  (void)lut;
  (void)factoryMode;
#if FREEINK_DRIVER_LGFX_EPD
  fillCanvasGray(fb);  // combine base + LSB/MSB planes -> 4-level gray
  // Same mode as the B/W base push: Panel_EPD's per-pixel diff keys on the
  // epd_mode LUT offset, so switching modes here would re-drive every pixel
  // (full-screen inversion flash). The board's fast LUT carries both the B/W
  // drive and the AA gray-nudge columns, so one mode serves both pushes.
  pushCanvas(lgfx::epd_mode::epd_fast);
  if (turnOff) sleepPanel();
#else
  (void)fb;
  (void)turnOff;
#endif
}

void LgfxEpdDriver::cleanupGrayscaleBuffers(EpdBus& bus, const uint8_t* bw) {
  (void)bus;
#if FREEINK_DRIVER_LGFX_EPD
  if (!bw) return;
  fillCanvasBW(bw);
#else
  (void)bw;
#endif
}

void LgfxEpdDriver::deepSleep(EpdBus& bus) {
  (void)bus;
#if FREEINK_DRIVER_LGFX_EPD
  sleepPanel();
#endif
}

// Per-board config injection. This driver has NO universal default — the bus pins
// and power hooks are entirely board-specific — so a LilyGo-class board defines
// `const LgfxEpdConfig& yourConfig();` in namespace freeink and builds with
// -DFREEINK_LGFX_EPD_CONFIG=yourConfig. The SDK's board-support libraries provide
// the default configs for FREEINK_DEVICE_LILYGO (BoardT5S3) and
// FREEINK_DEVICE_PAPERS3 (BoardPaperS3) builds.
#if FREEINK_DEVICE_LILYGO
const LgfxEpdConfig& lilygoT5S3LgfxConfig();
PanelDriver& lgfxEpdDriver() {
  static LgfxEpdDriver instance(lilygoT5S3LgfxConfig());
  return instance;
}
#elif FREEINK_DEVICE_PAPERS3 && !defined(FREEINK_LGFX_EPD_CONFIG)
const LgfxEpdConfig& m5PaperS3LgfxConfig();
PanelDriver& lgfxEpdDriver() {
  static LgfxEpdDriver instance(m5PaperS3LgfxConfig());
  return instance;
}
#elif defined(FREEINK_LGFX_EPD_CONFIG)
const LgfxEpdConfig& FREEINK_LGFX_EPD_CONFIG();
PanelDriver& lgfxEpdDriver() {
  static LgfxEpdDriver instance(FREEINK_LGFX_EPD_CONFIG());
  return instance;
}
#elif FREEINK_DRIVER_LGFX_EPD
#error "FREEINK_DRIVER_LGFX_EPD requires a board config: define `const LgfxEpdConfig& yourConfig();` in namespace freeink and build with -DFREEINK_LGFX_EPD_CONFIG=yourConfig"
#else
// Driver not selected in this build: provide a stub so the accessor still links if
// referenced. Never called (the facade only selects it under FREEINK_DRIVER_LGFX_EPD).
PanelDriver& lgfxEpdDriver() {
  static const LgfxEpdConfig kNone = {};
  static LgfxEpdDriver instance(kNone);
  return instance;
}
#endif

}  // namespace freeink
