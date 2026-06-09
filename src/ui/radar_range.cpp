#include "ui/radar_range.h"

#include "ui/radar_theme.h"

#include <Preferences.h>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace ui::radar {

namespace {

constexpr char kPrefsNamespace[] = "planeradar";
constexpr char kPrefsRangeKey[] = "rangeIdx";
constexpr char kPrefsMilesKey[] = "useMiles";
constexpr char kPrefsRunwaysKey[] = "showRwys";
constexpr char kPrefsContoursKey[] = "showCont";
constexpr char kPrefsLabelsKey[] = "showLbl";
constexpr char kPrefsBeepKey[] = "beep";
constexpr char kPrefsBeepVolKey[] = "beepVol";
constexpr char kPrefsAutoRotKey[] = "autoRot";
constexpr char kPrefsSweepMsKey[] = "sweepMs";
constexpr char kPrefsSweepColKey[] = "sweepCol";
constexpr char kPrefsAcColKey[] = "acCol";
constexpr char kPrefsBrightKey[] = "bright";
constexpr uint8_t kDefaultRangeIndex = 2;  // 15 km ring
constexpr float kKmPerMile = 1.609344f;

constexpr uint32_t kDefaultSweepMs = 3500;
constexpr uint32_t kMinSweepMs = 800;
constexpr uint32_t kMaxSweepMs = 12000;
constexpr uint32_t kDefaultSweepColor = 0x96FFAF;   // light green line
constexpr uint32_t kDefaultAircraftColor = 0xFF0000;  // red
constexpr uint8_t kDefaultBrightness = 255;

Preferences s_prefs;
uint8_t s_range_index = kDefaultRangeIndex;
bool s_use_miles = false;
bool s_show_runways = true;
bool s_show_contours = false;
bool s_show_labels = true;
bool s_beep = false;
uint8_t s_beep_vol = 5;
bool s_auto_rotate = true;
uint32_t s_sweep_ms = kDefaultSweepMs;
uint32_t s_sweep_color = kDefaultSweepColor;
uint32_t s_aircraft_color = kDefaultAircraftColor;
uint8_t s_brightness = kDefaultBrightness;

void saveRangeIndex() {
  if (!s_prefs.begin(kPrefsNamespace, false)) {
    return;
  }
  s_prefs.putUChar(kPrefsRangeKey, s_range_index);
  s_prefs.end();
}

void saveUseMiles() {
  if (!s_prefs.begin(kPrefsNamespace, false)) {
    return;
  }
  s_prefs.putBool(kPrefsMilesKey, s_use_miles);
  s_prefs.end();
}

void saveShowRunways() {
  if (!s_prefs.begin(kPrefsNamespace, false)) {
    return;
  }
  s_prefs.putBool(kPrefsRunwaysKey, s_show_runways);
  s_prefs.end();
}

bool portalCheckboxChecked(const char* value) {
  if (value == nullptr || value[0] == '\0') {
    return false;
  }
  // WiFiManager checkbox submits its value= attribute ("T", or "F" if we prefilled F).
  if ((value[0] == 'T' || value[0] == 't' || value[0] == 'F' || value[0] == 'f') &&
      value[1] == '\0') {
    return true;
  }
  return strcmp(value, "on") == 0;
}

}  // namespace

void rangeInit() {
  if (!s_prefs.begin(kPrefsNamespace, true)) {
    return;
  }
  const uint8_t saved = s_prefs.getUChar(kPrefsRangeKey, kDefaultRangeIndex);
  s_range_index =
      (saved < kRangePresetCount) ? saved : kDefaultRangeIndex;
  s_use_miles = s_prefs.getBool(kPrefsMilesKey, false);
  s_show_runways = s_prefs.getBool(kPrefsRunwaysKey, true);
  s_show_contours = s_prefs.getBool(kPrefsContoursKey, false);
  s_show_labels = s_prefs.getBool(kPrefsLabelsKey, true);
  s_beep = s_prefs.getBool(kPrefsBeepKey, false);
  s_beep_vol = s_prefs.getUChar(kPrefsBeepVolKey, 5);
  if (s_beep_vol > 100) s_beep_vol = 100;
  s_auto_rotate = s_prefs.getBool(kPrefsAutoRotKey, true);
  s_sweep_ms = s_prefs.getUInt(kPrefsSweepMsKey, kDefaultSweepMs);
  if (s_sweep_ms < kMinSweepMs || s_sweep_ms > kMaxSweepMs) {
    s_sweep_ms = kDefaultSweepMs;
  }
  s_sweep_color = s_prefs.getUInt(kPrefsSweepColKey, kDefaultSweepColor) & 0xFFFFFF;
  s_aircraft_color = s_prefs.getUInt(kPrefsAcColKey, kDefaultAircraftColor) & 0xFFFFFF;
  s_brightness = s_prefs.getUChar(kPrefsBrightKey, kDefaultBrightness);
  s_prefs.end();
}

void rangeNext() {
  s_range_index = static_cast<uint8_t>((s_range_index + 1) % kRangePresetCount);
  saveRangeIndex();
}

const RangePreset& rangeCurrent() { return kRangePresets[s_range_index]; }

uint8_t rangeIndex() { return s_range_index; }

float fetchRadiusKm() {
  const float outer_km = rangeCurrent().outer_km;
  const float screen_r_px =
      static_cast<float>(kCenterX - kBeyondRingScreenMarginPx);
  return outer_km * (screen_r_px / static_cast<float>(kGridOuterRadius));
}

bool useMiles() { return s_use_miles; }

bool showRunways() { return s_show_runways; }

void saveMilesFromPortal(const char* checkbox_value) {
  s_use_miles = portalCheckboxChecked(checkbox_value);
  saveUseMiles();
  Serial.printf("Distance units: %s\n", s_use_miles ? "miles" : "km");
}

void saveRunwaysFromPortal(const char* checkbox_value) {
  s_show_runways = portalCheckboxChecked(checkbox_value);
  saveShowRunways();
  Serial.printf("Runway overlay: %s\n", s_show_runways ? "on" : "off");
}

void formatRing3Label(char* buf, size_t len, float ring3_km, bool use_miles) {
  if (use_miles) {
    const int mi = static_cast<int>(lroundf(ring3_km / kKmPerMile));
    snprintf(buf, len, "%dmi", mi);
  } else {
    const int km = static_cast<int>(lroundf(ring3_km));
    snprintf(buf, len, "%dkm", km);
  }
}

void formatCurrentRing3Label(char* buf, size_t len) {
  formatRing3Label(buf, len, rangeCurrent().ring3_km, s_use_miles);
}

void unitsReset() {
  s_use_miles = false;
  s_show_runways = true;
  if (s_prefs.begin(kPrefsNamespace, false)) {
    s_prefs.remove(kPrefsMilesKey);
    s_prefs.remove(kPrefsRunwaysKey);
    s_prefs.end();
  }
}

namespace {

void putUInt(const char* key, uint32_t value) {
  if (s_prefs.begin(kPrefsNamespace, false)) {
    s_prefs.putUInt(key, value);
    s_prefs.end();
  }
}

void putUChar(const char* key, uint8_t value) {
  if (s_prefs.begin(kPrefsNamespace, false)) {
    s_prefs.putUChar(key, value);
    s_prefs.end();
  }
}

}  // namespace

void setRangeIndex(uint8_t index) {
  if (index >= kRangePresetCount) {
    return;
  }
  s_range_index = index;
  putUChar(kPrefsRangeKey, index);
}

void setMiles(bool miles) {
  s_use_miles = miles;
  putUChar(kPrefsMilesKey, miles ? 1 : 0);  // Preferences stores bool as uchar
}

void setRunways(bool on) {
  s_show_runways = on;
  putUChar(kPrefsRunwaysKey, on ? 1 : 0);
}

bool showContours() { return s_show_contours; }

void setContours(bool on) {
  s_show_contours = on;
  putUChar(kPrefsContoursKey, on ? 1 : 0);
}

bool showLabels() { return s_show_labels; }

void setLabels(bool on) {
  s_show_labels = on;
  putUChar(kPrefsLabelsKey, on ? 1 : 0);
}

bool beepEnabled() { return s_beep; }

void setBeep(bool on) {
  s_beep = on;
  putUChar(kPrefsBeepKey, on ? 1 : 0);
}

uint8_t beepVolume() { return s_beep_vol; }

void setBeepVolume(uint8_t pct) {
  s_beep_vol = pct > 100 ? 100 : pct;
  putUChar(kPrefsBeepVolKey, s_beep_vol);
}

bool autoRotate() { return s_auto_rotate; }

void setAutoRotate(bool on) {
  s_auto_rotate = on;
  putUChar(kPrefsAutoRotKey, on ? 1 : 0);
}

uint32_t sweepPeriodMs() { return s_sweep_ms; }

void setSweepPeriodMs(uint32_t ms) {
  if (ms < kMinSweepMs) ms = kMinSweepMs;
  if (ms > kMaxSweepMs) ms = kMaxSweepMs;
  s_sweep_ms = ms;
  putUInt(kPrefsSweepMsKey, ms);
}

uint32_t sweepColorRgb() { return s_sweep_color; }

void setSweepColorRgb(uint32_t rgb) {
  s_sweep_color = rgb & 0xFFFFFF;
  putUInt(kPrefsSweepColKey, s_sweep_color);
}

uint32_t aircraftColorRgb() { return s_aircraft_color; }

void setAircraftColorRgb(uint32_t rgb) {
  s_aircraft_color = rgb & 0xFFFFFF;
  putUInt(kPrefsAcColKey, s_aircraft_color);
}

uint8_t brightness() { return s_brightness; }

void setBrightness(uint8_t value) {
  s_brightness = value;
  putUChar(kPrefsBrightKey, value);
}

}  // namespace ui::radar
