/**
 * Plane Radar — WiFi setup, then radar UI on the round GC9A01 display.
 */

#include <Arduino.h>
#include <WiFi.h>

#include "config.h"
#include "hardware/display.h"
#include "services/adsb_client.h"
#include "services/audio.h"
#include "services/config_server.h"
#include "services/custom_airfields.h"
#include "services/map_contours.h"
#include "services/orientation.h"
#include "services/radar_location.h"
#include "services/wifi_setup.h"
#include "ui/radar_display.h"
#include "ui/radar_range.h"
#include "ui/status_screens.h"

namespace {

bool g_radar_visible = false;
unsigned long g_wifi_down_since = 0;
unsigned long g_last_reconnect_ms = 0;
unsigned long g_last_adsb_fetch_ms = 0;

void showRadarIfConnected() {
  if (WiFi.status() != WL_CONNECTED) {
    g_radar_visible = false;
    return;
  }
  services::config_server::begin();  // idempotent; starts once connected
  ui::radarDisplayDraw();
  g_radar_visible = true;
}

void onRangeTap() {
  ui::radar::rangeNext();
  char range_label[12];
  ui::radar::formatCurrentRing3Label(range_label, sizeof(range_label));
  Serial.printf("Range: %s (outer ~%.0f km)\n", range_label,
                ui::radar::rangeCurrent().outer_km);

  if (g_radar_visible && WiFi.status() == WL_CONNECTED) {
    ui::radarDisplayDraw();
  }
}

void handleBootButton() {
  bootButtonPollLongPress();
  if (bootButtonConsumeTap()) {
    onRangeTap();
  }
}

#if defined(RADAR_BOARD_S3LCD185)
void redrawIfVisible() {
  if (g_radar_visible && WiFi.status() == WL_CONNECTED) {
    ui::radarDisplayDraw();
  }
}

void onPwrSingleClick() {  // toggle map contours
  ui::radar::setContours(!ui::radar::showContours());
  Serial.printf("PWR: contours %s\n", ui::radar::showContours() ? "on" : "off");
  redrawIfVisible();
}

void onPwrDoubleClick() {  // toggle text labels
  ui::radar::setLabels(!ui::radar::showLabels());
  Serial.printf("PWR: labels %s\n", ui::radar::showLabels() ? "on" : "off");
  redrawIfVisible();
}

void onPwrLongPress() {  // cycle range preset (BOOT pin is unreliable here)
  ui::radar::rangeNext();
  char label[12];
  ui::radar::formatCurrentRing3Label(label, sizeof(label));
  Serial.printf("PWR: range %s\n", label);
  redrawIfVisible();
}

// Polled single/double-click + long-press detector for the PWR button
// (active LOW). Long-press fires once on release.
void handlePwrButton() {
  static bool initialized = false;
  static bool prev_high = true;
  static unsigned long press_ms = 0;
  static unsigned long last_click_ms = 0;
  static bool pending_single = false;
  if (!initialized) {
    pinMode(config::kPwrPin, INPUT_PULLUP);
    initialized = true;
  }

  const bool high = digitalRead(config::kPwrPin) != LOW;
  const unsigned long now = millis();

  if (prev_high && !high) {
    press_ms = now;  // pressed
  } else if (!prev_high && high) {
    const unsigned long held = now - press_ms;
    if (held >= config::kPwrLongPressMs) {
      pending_single = false;
      onPwrLongPress();
    } else if (held >= config::kPwrClickMinMs) {
      if (pending_single && (now - last_click_ms) < config::kPwrDoubleClickMs) {
        pending_single = false;
        onPwrDoubleClick();
      } else {
        pending_single = true;
        last_click_ms = now;
      }
    }
  }
  prev_high = high;

  if (pending_single && (now - last_click_ms) >= config::kPwrDoubleClickMs) {
    pending_single = false;
    onPwrSingleClick();
  }
}
#endif  // RADAR_BOARD_S3LCD185

#if defined(RADAR_BOARD_S3LCD185)
// ADS-B polling on core 0: the blocking HTTPS fetch (~100 ms–1 s) must not
// stall the radar sweep, which is rendered by the main loop on core 1. The
// list is shared via a mutex (see services::adsb::snapshotAircraft); the
// render reads positions live every frame, so no explicit redraw is needed.
void adsbTask(void*) {
  double af_lat = 999.0;
  double af_lon = 999.0;
  float af_radius = 0.0f;
  bool last_contours = false;
  unsigned long af_last_attempt = 0;
  constexpr unsigned long kAfMinIntervalMs = 30000;  // be gentle to public Overpass
  for (;;) {
    if (WiFi.status() == WL_CONNECTED) {
      const float fetch_km = ui::radar::fetchRadiusKm();
      services::adsb::fetchUpdate(services::location::lat(),
                                  services::location::lon(), fetch_km);

      // Refresh nearby airfields (OSM Overpass) when the centre moves or the
      // range grows — rare, so a slow query here doesn't matter. Throttled so a
      // failing/retrying fetch never hammers the public server.
      const double clat = services::location::lat();
      const double clon = services::location::lon();
      const double dlat = clat - af_lat;
      const double dlon = clon - af_lon;
      // Cap the airfield query radius — at 100/200 km presets an Overpass
      // query for the full radius would be huge/slow. Nearby fields still show.
      const float af_km = fetch_km > 60.0f ? 60.0f : fetch_km;
      const bool moved = (dlat * dlat + dlon * dlon) > 1e-8;  // ~1 km
      const bool con_on = ui::radar::showContours();
      const bool con_edge = con_on && !last_contours;  // just switched on
      last_contours = con_on;
      const bool due = (af_last_attempt == 0) ||
                       (millis() - af_last_attempt > kAfMinIntervalMs);
      // con_edge (just switched on) is a one-shot, so let it bypass the cooldown.
      if ((moved || af_km > af_radius * 1.05f || con_edge) && (due || con_edge)) {
        af_last_attempt = millis();
        const bool ok = services::airfields::fetchNearby(clat, clon, af_km);
        if (con_on) {
          services::contours::fetchNearby(clat, clon, af_km);
        }
        if (ok) {
          af_lat = clat;
          af_lon = clon;
          af_radius = af_km;
        }
      }
    }
    vTaskDelay(pdMS_TO_TICKS(config::kAdsbFetchIntervalMs));
  }
}
#else
void fetchAndDrawAircraft() {
  const float fetch_km = ui::radar::fetchRadiusKm();
  if (!services::adsb::fetchUpdate(services::location::lat(),
                                   services::location::lon(), fetch_km)) {
    handleBootButton();
    return;
  }
  ui::radarDisplayRefreshAircraft();
  handleBootButton();
}
#endif

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println();
  Serial.println("Plane Radar");

  bootButtonInit();
  displayInit();
  if (wifiShowsSetupScreenOnBoot()) {
    statusScreenPortal();
  }
  services::location::init();
  ui::radar::rangeInit();
  services::adsb::init();
  services::airfields::init();
  services::contours::init();
  services::orientation::init();
  tft.setBrightness(ui::radar::brightness());

#if defined(RADAR_BOARD_S3LCD185)
  // Poll ADS-B on core 0; the main loop keeps the sweep smooth on core 1.
  xTaskCreatePinnedToCore(adsbTask, "adsb", 16384, nullptr, 1, nullptr, 0);
#endif

  if (wifiSetupConnect()) {
    showRadarIfConnected();
  }

#if defined(RADAR_BOARD_S3LCD185) && !defined(RADAR_NO_AUDIO)
  // Init the I2S speaker last, after WiFi + the first radar draw, to avoid a
  // DMA/interrupt-allocation clash during bring-up.
  services::audio::begin();
  services::audio::setVolume(ui::radar::beepVolume());
#endif
}

void loop() {
  handleBootButton();
#if defined(RADAR_BOARD_S3LCD185) && !defined(RADAR_NO_PWR)
  handlePwrButton();
#endif

  if (WiFi.status() != WL_CONNECTED) {
    if (g_radar_visible) {
      Serial.println("WiFi lost — will reconnect");
      g_radar_visible = false;
    }

    if (g_wifi_down_since == 0) {
      g_wifi_down_since = millis();
    }

    const unsigned long down_ms = millis() - g_wifi_down_since;
    if (down_ms >= config::kWifiDownGraceMs &&
        millis() - g_last_reconnect_ms >= config::kWifiReconnectIntervalMs) {
      g_last_reconnect_ms = millis();
      if (wifiReconnect()) {
        g_wifi_down_since = 0;
        showRadarIfConnected();
      }
    }
  } else {
    g_wifi_down_since = 0;
    if (!g_radar_visible) {
      showRadarIfConnected();
    }
#if !defined(RADAR_BOARD_S3LCD185)
    else if (millis() - g_last_adsb_fetch_ms >= config::kAdsbFetchIntervalMs) {
      g_last_adsb_fetch_ms = millis();
      fetchAndDrawAircraft();
    }
#endif
  }

  services::config_server::loop();

  // New airfield/contour data arrived from the core-0 fetch → rebuild base.
  const bool af_upd = services::airfields::consumeUpdated();
  const bool ct_upd = services::contours::consumeUpdated();
  if (g_radar_visible && (af_upd || ct_upd)) {
    ui::radarDisplayDraw();
  }

#if defined(RADAR_BOARD_S3LCD185)
  // IMU auto-rotate: poll the accelerometer a few times a second; the sweep
  // picks up the new rotation on its next frame (no explicit redraw needed).
  static unsigned long s_orient_ms = 0;
  if (g_radar_visible && ui::radar::autoRotate() && millis() - s_orient_ms > 200) {
    s_orient_ms = millis();
    const int r = services::orientation::update();
    if (r >= 0) {
      ui::radarDisplaySetRotation(r);
    }
  }
#endif

  if (g_radar_visible) {
    ui::radarDisplaySweepTick();
  }

  delay(5);
}
