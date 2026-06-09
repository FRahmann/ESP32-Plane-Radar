#include "services/map_contours.h"

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include <ArduinoJson.h>

#include <cmath>
#include <cstring>

namespace services::contours {

namespace {

constexpr char kOverpassUrl[] = "https://overpass-api.de/api/interpreter";
// Minimum spacing between kept points (deg²); ~0.004° ≈ 0.45 km — decimates
// dense coastlines/rivers so a few large features fit the pools.
constexpr double kMinSpacingSq = 0.000016;

Point s_pts[kMaxPoints];
PolySpan s_polys[kMaxPolys];
size_t s_point_count = 0;
size_t s_poly_count = 0;
bool s_updated = false;
SemaphoreHandle_t s_mutex = nullptr;

int32_t toE7(double deg) { return static_cast<int32_t>(lround(deg * 1e7)); }

void lock() {
  if (s_mutex) xSemaphoreTake(s_mutex, portMAX_DELAY);
}
void unlock() {
  if (s_mutex) xSemaphoreGive(s_mutex);
}

}  // namespace

void init() {
  if (s_mutex == nullptr) {
    s_mutex = xSemaphoreCreateMutex();
  }
}

bool fetchNearby(double lat, double lon, float radius_km) {
  const int r_m = static_cast<int>(radius_km * 1000.0f);

  char q[420];
  snprintf(q, sizeof(q),
           "[out:json][timeout:25];"
           "(way[\"natural\"=\"coastline\"](around:%d,%.6f,%.6f);"
           "way[\"natural\"=\"water\"](around:%d,%.6f,%.6f);"
           "way[\"waterway\"=\"river\"](around:%d,%.6f,%.6f););out geom;",
           r_m, lat, lon, r_m, lat, lon, r_m, lat, lon);

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  if (!http.begin(client, kOverpassUrl)) {
    return false;
  }
  http.addHeader("Content-Type", "text/plain");
  http.setTimeout(30000);
  const int code = http.POST(reinterpret_cast<uint8_t*>(q), strlen(q));
  if (code != HTTP_CODE_OK) {
    Serial.printf("contours: Overpass HTTP %d\n", code);
    http.end();
    return false;
  }
  const String payload = http.getString();
  http.end();

  JsonDocument doc;
  if (deserializeJson(doc, payload)) {
    Serial.println("contours: Overpass JSON parse error");
    return false;
  }
  JsonArrayConst els = doc["elements"].as<JsonArrayConst>();
  if (els.isNull()) {
    return false;
  }

  lock();
  s_point_count = 0;
  s_poly_count = 0;
  for (JsonObjectConst el : els) {
    if (s_poly_count >= kMaxPolys || s_point_count >= kMaxPoints) {
      break;
    }
    JsonArrayConst geom = el["geometry"].as<JsonArrayConst>();
    if (geom.isNull() || geom.size() < 2) {
      continue;
    }

    const uint16_t start = static_cast<uint16_t>(s_point_count);
    uint16_t kept = 0;
    double last_lat = 0.0, last_lon = 0.0;
    for (JsonObjectConst p : geom) {
      if (s_point_count >= kMaxPoints) {
        break;
      }
      const double plat = p["lat"].as<double>();
      const double plon = p["lon"].as<double>();
      if (kept > 0) {
        const double dla = plat - last_lat;
        const double dlo = plon - last_lon;
        if (dla * dla + dlo * dlo < kMinSpacingSq) {
          continue;  // too close to the last kept point — decimate
        }
      }
      s_pts[s_point_count].lat_e7 = toE7(plat);
      s_pts[s_point_count].lon_e7 = toE7(plon);
      ++s_point_count;
      ++kept;
      last_lat = plat;
      last_lon = plon;
    }
    if (kept >= 2) {
      s_polys[s_poly_count].start = start;
      s_polys[s_poly_count].count = kept;
      ++s_poly_count;
    } else {
      s_point_count = start;  // discard a single-point line
    }
  }
  s_updated = true;
  const size_t pc = s_poly_count, ptc = s_point_count;
  unlock();

  Serial.printf("contours: %u lines, %u points\n", static_cast<unsigned>(pc),
                static_cast<unsigned>(ptc));
  return true;
}

bool consumeUpdated() {
  lock();
  const bool u = s_updated;
  s_updated = false;
  unlock();
  return u;
}

size_t snapshot(PolySpan* polys, size_t max_polys, Point* pts, size_t max_pts,
                size_t* out_points) {
  lock();
  size_t np = s_poly_count;
  if (np > max_polys) np = max_polys;
  size_t npt = s_point_count;
  if (npt > max_pts) npt = max_pts;
  memcpy(polys, s_polys, np * sizeof(PolySpan));
  memcpy(pts, s_pts, npt * sizeof(Point));
  unlock();
  *out_points = npt;
  return np;
}

}  // namespace services::contours
