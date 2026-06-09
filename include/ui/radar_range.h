#pragma once

#include <cstddef>
#include <cstdint>

namespace ui::radar {

/**
 * Range presets (label on ring 3 = ¾ of outer radius).
 *
 * Recommended for ADS-B on a 1.28″ display:
 *   5 km  — pattern / very local (airfield vicinity)
 *  10 km  — default; neighborhood spotting
 *  15 km  — wider local area
 *  25 km  — metro / regional picture
 *
 * Outer radius (for aircraft math) is ring-3 distance ÷ 0.75.
 */
struct RangePreset {
  /** Distance shown on ring 3 (¾ of outer radius), always stored in km. */
  float ring3_km;
  float outer_km;
};

constexpr float kRing3ToOuterKm = 4.0f / 3.0f;

constexpr RangePreset kRangePresets[] = {
    {5.0f, 5.0f * kRing3ToOuterKm},
    {10.0f, 10.0f * kRing3ToOuterKm},
    {15.0f, 15.0f * kRing3ToOuterKm},
    {25.0f, 25.0f * kRing3ToOuterKm},
    {50.0f, 50.0f * kRing3ToOuterKm},
    {100.0f, 100.0f * kRing3ToOuterKm},
    {200.0f, 200.0f * kRing3ToOuterKm},
};

constexpr size_t kRangePresetCount =
    sizeof(kRangePresets) / sizeof(kRangePresets[0]);

/** Load saved range and distance units from flash. Call once after boot. */
void rangeInit();
/** Cycle preset and save to flash. */
void rangeNext();
const RangePreset& rangeCurrent();
uint8_t rangeIndex();
/** ADSB fetch radius (km): scaled to screen edge so beyond-ring dots have data. */
float fetchRadiusKm();

bool useMiles();
bool showRunways();
/** WiFi portal checkbox: "T" = miles, otherwise km. */
void saveMilesFromPortal(const char* checkbox_value);
void saveRunwaysFromPortal(const char* checkbox_value);
void formatRing3Label(char* buf, size_t len, float ring3_km, bool use_miles);
void formatCurrentRing3Label(char* buf, size_t len);
/** Reset distance units to km (e.g. with WiFi credential wipe). */
void unitsReset();

// --- Direct setters (used by the config web page; persist to flash) ---
void setRangeIndex(uint8_t index);
void setMiles(bool miles);
void setRunways(bool on);

/** Coastline/water contour underlay (OSM); off by default. */
bool showContours();
void setContours(bool on);

/** Text labels (airport idents, aircraft tags); on by default. */
bool showLabels();
void setLabels(bool on);

/** Short beep when the sweep paints an aircraft; off by default. */
bool beepEnabled();
void setBeep(bool on);

/** Beep volume 0–100 %. */
uint8_t beepVolume();
void setBeepVolume(uint8_t pct);

/** Auto-rotate the radar from the IMU (90° steps); on by default. */
bool autoRotate();
void setAutoRotate(bool on);

// --- Sweep + appearance settings (persisted) ---
/** Sweep revolution period in ms (clamped to a sane range). */
uint32_t sweepPeriodMs();
void setSweepPeriodMs(uint32_t ms);
/** Sweep line colour as 0xRRGGBB. */
uint32_t sweepColorRgb();
void setSweepColorRgb(uint32_t rgb);
/** Aircraft/blip colour as 0xRRGGBB. */
uint32_t aircraftColorRgb();
void setAircraftColorRgb(uint32_t rgb);
/** Display backlight brightness 0–255. */
uint8_t brightness();
void setBrightness(uint8_t value);

}  // namespace ui::radar
