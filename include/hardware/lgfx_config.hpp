#pragma once

#define LGFX_USE_V1
#include <LovyanGFX.hpp>

#include "config.h"

#if defined(RADAR_BOARD_S3LCD185)

// =====================================================================
//  Waveshare ESP32-S3-LCD-1.85 — ST77916 360×360 round, QSPI bus.
//
//  LovyanGFX has no built-in Panel_ST77916, so we vendor one here as a
//  plain Panel_LCD subclass (the QSPI command framing is handled by
//  Bus_SPI once pin_io0..io3 are configured — same pattern as the
//  bundled Panel_ST77961). The init sequence is the ST77916 vendor
//  sequence taken from moononournation/Arduino_GFX (Arduino_ST77916),
//  translated into LovyanGFX's {cmd, count[, data...]} list format.
//
//  NOTE: QSPI support lives in the LovyanGFX *develop* branch — see
//  platformio.ini (lib_deps pins the git ref).
//
//  Display RST is not on a GPIO; it is driven over the TCA9554 I2C
//  expander before tft.init() (see expanderResetDisplay() in display.cpp),
//  so the panel's pin_rst is left at -1.
// =====================================================================

namespace lgfx_user {

// Based on LovyanGFX's Panel_SH8601Z, which implements the QSPI-DBI wire
// protocol the ST77916 uses: commands framed as {0x02,0x00,cmd,0x00} on a
// single line, pixel RAM writes framed as {0x32,0x00,0x2C,0x00} + quad data.
// (The plain Panel_LCD path sends raw bytes with a D/C pin and does NOT frame
// QSPI commands — that produced garbage on this panel.) We inherit all the
// framing / setWindow / writeImage machinery and only swap in the ST77916
// init sequence, 360×360 geometry, COLMOD, and PWM backlight.
struct Panel_ST77916 : public lgfx::Panel_SH8601Z {
  Panel_ST77916(void) {
    _cfg.panel_width = _cfg.memory_width = config::kDisplayWidth;
    _cfg.panel_height = _cfg.memory_height = config::kDisplayHeight;
    _cfg.dummy_read_pixel = 8;
  }

  // ST77916 wants COLMOD (0x3A) = 0x05 for 16 bpp (per the Arduino_GFX init);
  // the SH8601Z base would send 0x55, so force the ST77916 value here.
  lgfx::v1::color_depth_t setColorDepth(lgfx::v1::color_depth_t) override {
    _write_depth = lgfx::v1::rgb565_2Byte;
    _read_depth = lgfx::v1::rgb565_2Byte;
    startWrite();
    cs_control(false);
    write_cmd(0x3A);
    _bus->writeCommand(0x05, 8);
    _bus->wait();
    cs_control(true);
    endWrite();
    return _write_depth;
  }

  // Backlight is a PWM GPIO (Light_PWM), not a DCS brightness command.
  void setBrightness(uint8_t brightness) override {
    if (_light) {
      _light->setBrightness(brightness);
    }
  }

  // SH8601Z::setInvert runs outside a transaction (no startWrite) and is a
  // no-op on this wiring; drive INVON/INVOFF transactionally so cfg.invert
  // actually controls the panel.
  void setInvert(bool invert) override {
    startWrite();
    cs_control(false);
    write_cmd(invert ? 0x21 : 0x20);  // 0x21 INVON / 0x20 INVOFF
    _bus->wait();
    cs_control(true);
    endWrite();
  }

  bool init(bool use_reset) override {
    // Bypass Panel_SH8601Z's AMOLED init; run the ST77916 vendor sequence
    // through the inherited QSPI-DBI command_list().
    if (!lgfx::v1::Panel_Device::init(use_reset)) {
      return false;
    }
    // Format: command, param_count, params...
    //   param_count | CMD_INIT_DELAY (0x80)  ⇒ one trailing delay-ms byte.
    // Terminated by 0xFF, 0xFF.
    static constexpr uint8_t list0[] = {
        0xF0, 1, 0x28,
        0xF2, 1, 0x28,
        0x73, 1, 0xF0,
        0x7C, 1, 0xD1,
        0x83, 1, 0xE0,
        0x84, 1, 0x61,
        0xF2, 1, 0x82,
        0xF0, 1, 0x00,
        0xF0, 1, 0x01,
        0xF1, 1, 0x01,
        0xB0, 1, 0x69,
        0xB1, 1, 0x4A,
        0xB2, 1, 0x2F,
        0xB3, 1, 0x01,
        0xB4, 1, 0x69,
        0xB5, 1, 0x45,
        0xB6, 1, 0xAB,
        0xB7, 1, 0x41,
        0xB8, 1, 0x86,
        0xB9, 1, 0x15,
        0xBA, 1, 0x00,
        0xBB, 1, 0x08,
        0xBC, 1, 0x08,
        0xBD, 1, 0x00,
        0xBE, 1, 0x00,
        0xBF, 1, 0x07,
        0xC0, 1, 0x80,
        0xC1, 1, 0x10,
        0xC2, 1, 0x37,
        0xC3, 1, 0x80,
        0xC4, 1, 0x10,
        0xC5, 1, 0x37,
        0xC6, 1, 0xA9,
        0xC7, 1, 0x41,
        0xC8, 1, 0x01,
        0xC9, 1, 0xA9,
        0xCA, 1, 0x41,
        0xCB, 1, 0x01,
        0xCC, 1, 0x7F,
        0xCD, 1, 0x7F,
        0xCE, 1, 0xFF,
        0xD0, 1, 0x91,
        0xD1, 1, 0x68,
        0xD2, 1, 0x68,
        0xF5, 2, 0x00, 0xA5,
        0xF1, 1, 0x10,
        0xF0, 1, 0x00,
        0xF0, 1, 0x02,
        0xE0, 14, 0xF0, 0x10, 0x18, 0x0D, 0x0C, 0x38, 0x3E, 0x44, 0x51, 0x39,
        0x15, 0x15, 0x30, 0x34,
        0xE1, 14, 0xF0, 0x0F, 0x17, 0x0D, 0x0B, 0x07, 0x3E, 0x33, 0x51, 0x39,
        0x15, 0x15, 0x30, 0x34,
        0xF0, 1, 0x10,
        0xF3, 1, 0x10,
        0xE0, 1, 0x08,
        0xE1, 1, 0x00,
        0xE2, 1, 0x00,
        0xE3, 1, 0x00,
        0xE4, 1, 0xE0,
        0xE5, 1, 0x06,
        0xE6, 1, 0x21,
        0xE7, 1, 0x03,
        0xE8, 1, 0x05,
        0xE9, 1, 0x02,
        0xEA, 1, 0xE9,
        0xEB, 1, 0x00,
        0xEC, 1, 0x00,
        0xED, 1, 0x14,
        0xEE, 1, 0xFF,
        0xEF, 1, 0x00,
        0xF8, 1, 0xFF,
        0xF9, 1, 0x00,
        0xFA, 1, 0x00,
        0xFB, 1, 0x30,
        0xFC, 1, 0x00,
        0xFD, 1, 0x00,
        0xFE, 1, 0x00,
        0xFF, 1, 0x00,
        0x60, 1, 0x40,
        0x61, 1, 0x05,
        0x62, 1, 0x00,
        0x63, 1, 0x42,
        0x64, 1, 0xDA,
        0x65, 1, 0x00,
        0x66, 1, 0x00,
        0x67, 1, 0x00,
        0x68, 1, 0x00,
        0x69, 1, 0x00,
        0x6A, 1, 0x00,
        0x6B, 1, 0x00,
        0x70, 1, 0x40,
        0x71, 1, 0x04,
        0x72, 1, 0x00,
        0x73, 1, 0x42,
        0x74, 1, 0xD9,
        0x75, 1, 0x00,
        0x76, 1, 0x00,
        0x77, 1, 0x00,
        0x78, 1, 0x00,
        0x79, 1, 0x00,
        0x7A, 1, 0x00,
        0x7B, 1, 0x00,
        0x80, 1, 0x48,
        0x81, 1, 0x00,
        0x82, 1, 0x07,
        0x83, 1, 0x02,
        0x84, 1, 0xD7,
        0x85, 1, 0x04,
        0x86, 1, 0x00,
        0x87, 1, 0x00,
        0x88, 1, 0x48,
        0x89, 1, 0x00,
        0x8A, 1, 0x09,
        0x8B, 1, 0x02,
        0x8C, 1, 0xD9,
        0x8D, 1, 0x04,
        0x8E, 1, 0x00,
        0x8F, 1, 0x00,
        0x90, 1, 0x48,
        0x91, 1, 0x00,
        0x92, 1, 0x0B,
        0x93, 1, 0x02,
        0x94, 1, 0xDB,
        0x95, 1, 0x04,
        0x96, 1, 0x00,
        0x97, 1, 0x00,
        0x98, 1, 0x48,
        0x99, 1, 0x00,
        0x9A, 1, 0x0D,
        0x9B, 1, 0x02,
        0x9C, 1, 0xDD,
        0x9D, 1, 0x04,
        0x9E, 1, 0x00,
        0x9F, 1, 0x00,
        0xA0, 1, 0x48,
        0xA1, 1, 0x00,
        0xA2, 1, 0x06,
        0xA3, 1, 0x02,
        0xA4, 1, 0xD6,
        0xA5, 1, 0x04,
        0xA6, 1, 0x00,
        0xA7, 1, 0x00,
        0xA8, 1, 0x48,
        0xA9, 1, 0x00,
        0xAA, 1, 0x08,
        0xAB, 1, 0x02,
        0xAC, 1, 0xD8,
        0xAD, 1, 0x04,
        0xAE, 1, 0x00,
        0xAF, 1, 0x00,
        0xB0, 1, 0x48,
        0xB1, 1, 0x00,
        0xB2, 1, 0x0A,
        0xB3, 1, 0x02,
        0xB4, 1, 0xDA,
        0xB5, 1, 0x04,
        0xB6, 1, 0x00,
        0xB7, 1, 0x00,
        0xB8, 1, 0x48,
        0xB9, 1, 0x00,
        0xBA, 1, 0x0C,
        0xBB, 1, 0x02,
        0xBC, 1, 0xDC,
        0xBD, 1, 0x04,
        0xBE, 1, 0x00,
        0xBF, 1, 0x00,
        0xC0, 1, 0x10,
        0xC1, 1, 0x47,
        0xC2, 1, 0x56,
        0xC3, 1, 0x65,
        0xC4, 1, 0x74,
        0xC5, 1, 0x88,
        0xC6, 1, 0x99,
        0xC7, 1, 0x01,
        0xC8, 1, 0xBB,
        0xC9, 1, 0xAA,
        0xD0, 1, 0x10,
        0xD1, 1, 0x47,
        0xD2, 1, 0x56,
        0xD3, 1, 0x65,
        0xD4, 1, 0x74,
        0xD5, 1, 0x88,
        0xD6, 1, 0x99,
        0xD7, 1, 0x01,
        0xD8, 1, 0xBB,
        0xD9, 1, 0xAA,
        0xF3, 1, 0x01,
        0xF0, 1, 0x00,
        0x3A, 1, 0x05,  // COLMOD = 16bpp (RGB565)
        0x35, 1, 0x00,  // TEON
        // Inversion is set by setInvert() (driven by cfg.invert), not here.
        0x11, CMD_INIT_DELAY, 120,  // SLPOUT + 120 ms
        0x29, CMD_INIT_DELAY, 10,   // DISPON + 10 ms
        0xFF, 0xFF,
    };
    command_list(list0);
    // Inversion is applied at runtime in displayInit() via invertDisplay();
    // setting it here (during LGFX_Device::init) does not stick on this panel.
    return true;
  }
};

}  // namespace lgfx_user

/** LovyanGFX device: ST77916 over QSPI + PWM backlight. Pins from config.h. */
class LGFX : public lgfx::LGFX_Device {
  lgfx::Bus_SPI _bus;
  lgfx_user::Panel_ST77916 _panel;
  lgfx::Light_PWM _light;

 public:
  LGFX() {
    {
      auto cfg = _bus.config();
      cfg.spi_host = SPI2_HOST;
      cfg.spi_mode = 0;
      cfg.freq_write = config::kDisplaySpiWriteHz;
      cfg.freq_read = 16000000;
      cfg.spi_3wire = false;
      cfg.use_lock = true;
      cfg.dma_channel = SPI_DMA_CH_AUTO;
      cfg.pin_sclk = static_cast<int>(config::kDisplayPinSclk);
      cfg.pin_dc = -1;  // QSPI has no separate D/C line
      // Quad data lines (enables QSPI mode in develop-branch Bus_SPI).
      cfg.pin_io0 = static_cast<int>(config::kDisplayPinD0);
      cfg.pin_io1 = static_cast<int>(config::kDisplayPinD1);
      cfg.pin_io2 = static_cast<int>(config::kDisplayPinD2);
      cfg.pin_io3 = static_cast<int>(config::kDisplayPinD3);
      _bus.config(cfg);
      _panel.setBus(&_bus);
    }
    {
      auto cfg = _panel.config();
      cfg.pin_cs = static_cast<int>(config::kDisplayPinCs);
      cfg.pin_rst = -1;  // reset via TCA9554 expander, done before init()
      cfg.panel_width = config::kDisplayWidth;
      cfg.panel_height = config::kDisplayHeight;
      cfg.offset_x = 0;
      cfg.offset_y = 0;
      cfg.readable = false;
      cfg.invert = config::kDisplayInvert;
      cfg.rgb_order = config::kDisplayRgbOrder;
      cfg.dlen_16bit = false;
      cfg.bus_shared = false;
      _panel.config(cfg);
    }
    {
      auto cfg = _light.config();
      cfg.pin_bl = static_cast<int>(config::kDisplayPinBacklight);
      cfg.invert = false;
      cfg.freq = 12000;
      cfg.pwm_channel = 7;
      _light.config(cfg);
      _panel.setLight(&_light);
    }
    setPanel(&_panel);
  }
};

#else  // ---------------- ESP32-C3 + GC9A01 (original) ----------------

/** LovyanGFX device: GC9A01 on SPI. Pin values come from config.h. */
class LGFX : public lgfx::LGFX_Device {
  lgfx::Bus_SPI _bus;
  lgfx::Panel_GC9A01 _panel;

 public:
  LGFX() {
    {
      auto cfg = _bus.config();
      cfg.spi_host = SPI2_HOST;
      cfg.freq_write = config::kDisplaySpiWriteHz;
      cfg.pin_sclk = static_cast<int>(config::kDisplayPinSclk);
      cfg.pin_mosi = static_cast<int>(config::kDisplayPinMosi);
      cfg.pin_miso = -1;
      cfg.pin_dc = static_cast<int>(config::kDisplayPinDc);
      _bus.config(cfg);
      _panel.setBus(&_bus);
    }
    {
      auto cfg = _panel.config();
      cfg.pin_cs = static_cast<int>(config::kDisplayPinCs);
      cfg.pin_rst = static_cast<int>(config::kDisplayPinRst);
      cfg.invert = config::kDisplayInvert;
      cfg.rgb_order = config::kDisplayRgbOrder;
      _panel.config(cfg);
    }
    setPanel(&_panel);
  }
};

#endif  // RADAR_BOARD_S3LCD185
