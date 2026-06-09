#pragma once

#include <cstdint>

#include <driver/gpio.h>

namespace config {

// --- Default Wi-Fi ---
// Optional: pre-seed a network so the device auto-connects without the captive
// portal. Leave empty ("") to always use the portal for first-time setup.
// NEVER commit real credentials to a public repository.
constexpr char kDefaultWifiSsid[] = "";
constexpr char kDefaultWifiPass[] = "";

// --- Wi-Fi portal ---
constexpr char kPortalApName[] = "PlaneRadar-Setup";
constexpr char kPortalIp[] = "192.168.4.1";
/** mDNS host (no ".local" suffix); browser: http://plane-radar.local */
constexpr char kPortalHostname[] = "plane-radar";
constexpr char kPortalHostUrl[] = "plane-radar.local";

/** Per-attempt STA connect wait (ms); retried kWifiConnectAttempts times. */
constexpr unsigned long kWifiConnectAttemptMs = 15000;
constexpr uint8_t kWifiConnectAttempts = 3;
constexpr unsigned long kWifiPortalTimeoutSec = 0;  // 0 = no timeout while configuring
constexpr unsigned long kWifiConnectingFrameMs = 50;
/** Wait after disconnect before reconnecting (avoids portal on brief drops). */
constexpr unsigned long kWifiDownGraceMs = 4000;
/** Minimum interval between background reconnect tries. */
constexpr unsigned long kWifiReconnectIntervalMs = 15000;

#if defined(RADAR_BOARD_S3LCD185)

// =====================================================================
//  Waveshare ESP32-S3-LCD-1.85 — ST77916 360×360 round (QSPI)
//  Display reset is NOT a direct GPIO; it hangs off a TCA9554 I2C
//  expander (see kExpander* below). Backlight is GPIO 5 (PWM).
// =====================================================================

// --- BOOT button (ESP32-S3, active LOW) ---
constexpr gpio_num_t kBootPin = GPIO_NUM_0;
constexpr unsigned long kBootResetHoldMs = 3000UL;
/** Ignore BOOT taps shorter than this (debounce). */
constexpr unsigned long kBootTapMinMs = 40UL;

// --- PWR button (ESP32-S3-LCD-1.85 side button, active LOW) ---
// Single click toggles map contours, double click toggles labels.
constexpr gpio_num_t kPwrPin = GPIO_NUM_6;
constexpr unsigned long kPwrClickMinMs = 30UL;
constexpr unsigned long kPwrDoubleClickMs = 400UL;
constexpr unsigned long kPwrLongPressMs = 800UL;  // hold = cycle range

// --- Speaker (I2S amp; amp enable is on the TCA9554 expander) ---
constexpr int kSpeakerBclkPin = 48;
constexpr int kSpeakerWsPin = 38;
constexpr int kSpeakerDoutPin = 47;

// --- Display: ST77916 1.85" round 360×360 (QSPI, 4 data lines) ---
constexpr gpio_num_t kDisplayPinSclk = GPIO_NUM_40;  // QSPI clock
constexpr gpio_num_t kDisplayPinD0 = GPIO_NUM_46;    // QSPI IO0 (MOSI)
constexpr gpio_num_t kDisplayPinD1 = GPIO_NUM_45;    // QSPI IO1
constexpr gpio_num_t kDisplayPinD2 = GPIO_NUM_42;    // QSPI IO2
constexpr gpio_num_t kDisplayPinD3 = GPIO_NUM_41;    // QSPI IO3
constexpr gpio_num_t kDisplayPinCs = GPIO_NUM_21;
constexpr gpio_num_t kDisplayPinBacklight = GPIO_NUM_5;  // PWM backlight

// Display reset lives on the TCA9554 expander, not the ESP32 — toggled
// over I2C before tft.init(). LovyanGFX panel pin_rst is therefore -1.
constexpr gpio_num_t kExpanderPinSda = GPIO_NUM_11;
constexpr gpio_num_t kExpanderPinScl = GPIO_NUM_10;
constexpr uint8_t kExpanderI2cAddr = 0x20;  // TCA9554PWR (A0..A2 = 0)
/** Expander output bit wired to ST77916 RST (0-indexed; ESPHome "number: 1"). */
constexpr uint8_t kExpanderLcdResetBit = 1;

constexpr int kDisplayWidth = 360;
constexpr int kDisplayHeight = 360;

constexpr uint32_t kDisplaySpiWriteHz = 40000000;  // QSPI write clock
// This panel needs INVON for correct colours (INVOFF shows the complement:
// yellow→blue). Applied via Panel_ST77916::setInvert() driven by this flag.
constexpr bool kDisplayInvert = true;
constexpr bool kDisplayRgbOrder = true;

#else

// =====================================================================
//  ESP32-C3 Super Mini — GC9A01 1.28" round 240×240 (SPI)  [original]
// =====================================================================

// --- BOOT button (ESP32-C3 Super Mini, active LOW) ---
constexpr gpio_num_t kBootPin = GPIO_NUM_9;
constexpr unsigned long kBootResetHoldMs = 3000UL;
/** Ignore BOOT taps shorter than this (debounce). */
constexpr unsigned long kBootTapMinMs = 40UL;

// --- Display: GC9A01 1.28" round 240×240 (SPI) ---
constexpr gpio_num_t kDisplayPinRst = GPIO_NUM_0;
constexpr gpio_num_t kDisplayPinCs = GPIO_NUM_1;
constexpr gpio_num_t kDisplayPinDc = GPIO_NUM_10;
constexpr gpio_num_t kDisplayPinMosi = GPIO_NUM_3;  // display SDA
constexpr gpio_num_t kDisplayPinSclk = GPIO_NUM_4;  // display SCL

constexpr int kDisplayWidth = 240;
constexpr int kDisplayHeight = 240;

constexpr uint32_t kDisplaySpiWriteHz = 40000000;
// GC9A01 modules often need invert + BGR for correct black/green output
constexpr bool kDisplayInvert = true;
constexpr bool kDisplayRgbOrder = true;

#endif  // RADAR_BOARD_S3LCD185

// --- Radar center defaults (overridden via WiFi setup portal) ---
constexpr double kDefaultRadarLat = 52.3676;
constexpr double kDefaultRadarLon = 4.9041;

/** Poll adsb.fi (API public limit: 1 req/s). */
constexpr unsigned long kAdsbFetchIntervalMs = 3000;
/** Legacy scale unused — fetch uses radar::fetchRadiusKm() to screen edge. */
constexpr float kAdsbFetchRadiusScale = 1.0f;
/** false = hide aircraft with alt_baro "ground"; true = show them too. */
constexpr bool kAdsbShowGroundAircraft = false;

// --- UI colors (RGB565) — status screens ---
constexpr uint16_t kColorBlack = 0x0000;
constexpr uint16_t kColorYellow = 0xFFE0;
constexpr uint16_t kTextOnYellow = kColorBlack;
constexpr uint16_t kTextOnBlack = 0xFFFF;

}  // namespace config
