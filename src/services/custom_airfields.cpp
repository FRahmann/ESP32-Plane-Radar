#include "services/custom_airfields.h"

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include <ArduinoJson.h>

#include <cctype>
#include <cmath>
#include <cstring>

namespace services::airfields {

namespace {

constexpr char kOverpassUrl[] = "https://overpass-api.de/api/interpreter";

Runway s_runways[kMaxRunways];
Aerodrome s_aerodromes[kMaxAerodromes];
size_t s_runway_count = 0;
size_t s_aerodrome_count = 0;
bool s_updated = false;
SemaphoreHandle_t s_mutex = nullptr;

int32_t toE7(double deg) { return static_cast<int32_t>(lround(deg * 1e7)); }

void lock() {
  if (s_mutex) xSemaphoreTake(s_mutex, portMAX_DELAY);
}
void unlock() {
  if (s_mutex) xSemaphoreGive(s_mutex);
}

// Gliding sites ("Segelfluggelände …") are filtered out on request.
bool startsWithSegel(const char* s) {
  static const char kSegel[] = "segel";
  for (int i = 0; i < 5; ++i) {
    if (s[i] == '\0' ||
        tolower(static_cast<unsigned char>(s[i])) != kSegel[i]) {
      return false;
    }
  }
  return true;
}

// Best human label from an aerodrome's OSM tags.
const char* pickLabel(JsonObjectConst tags) {
  for (const char* key : {"icao", "faa", "iata", "ref"}) {
    const char* v = tags[key];
    if (v && v[0]) return v;
  }
  const char* name = tags["name"];
  return (name && name[0]) ? name : "AD";
}

}  // namespace

void init() {
  if (s_mutex == nullptr) {
    s_mutex = xSemaphoreCreateMutex();
  }
}

bool fetchNearby(double lat, double lon, float radius_km) {
  const int r_m = static_cast<int>(radius_km * 1000.0f);

  char q[460];
  snprintf(q, sizeof(q),
           "[out:json][timeout:25];"
           "way[\"aeroway\"=\"runway\"](around:%d,%.6f,%.6f);out geom;"
           "(node[\"aeroway\"=\"aerodrome\"](around:%d,%.6f,%.6f);"
           "way[\"aeroway\"=\"aerodrome\"](around:%d,%.6f,%.6f););out tags center;",
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
    Serial.printf("airfields: Overpass HTTP %d\n", code);
    http.end();
    return false;
  }
  const String payload = http.getString();
  http.end();

  JsonDocument doc;
  if (deserializeJson(doc, payload)) {
    Serial.println("airfields: Overpass JSON parse error");
    return false;
  }
  JsonArrayConst els = doc["elements"].as<JsonArrayConst>();
  if (els.isNull()) {
    return false;
  }

  lock();
  s_runway_count = 0;
  s_aerodrome_count = 0;

  // Pass 1: aerodromes. Skip gliding sites (name starts with "Segel") and
  // remember their centres so we can drop their runways too.
  double ex_lat[24];
  double ex_lon[24];
  size_t ex_count = 0;
  for (JsonObjectConst el : els) {
    JsonObjectConst tags = el["tags"].as<JsonObjectConst>();
    const char* aeroway = tags["aeroway"];
    if (aeroway == nullptr || strcmp(aeroway, "aerodrome") != 0) {
      continue;
    }
    double alat = 0.0, alon = 0.0;
    if (el["lat"].is<double>()) {
      alat = el["lat"].as<double>();
      alon = el["lon"].as<double>();
    } else if (!el["center"].isNull()) {
      alat = el["center"]["lat"].as<double>();
      alon = el["center"]["lon"].as<double>();
    } else {
      continue;
    }
    const char* name = tags["name"];
    if (name != nullptr && startsWithSegel(name)) {
      if (ex_count < 24) {
        ex_lat[ex_count] = alat;
        ex_lon[ex_count] = alon;
        ++ex_count;
      }
      continue;
    }
    if (s_aerodrome_count >= kMaxAerodromes) {
      continue;
    }
    Aerodrome& ad = s_aerodromes[s_aerodrome_count++];
    strncpy(ad.ident, pickLabel(tags), sizeof(ad.ident) - 1);
    ad.ident[sizeof(ad.ident) - 1] = '\0';
    ad.lat_e7 = toE7(alat);
    ad.lon_e7 = toE7(alon);
  }

  // Pass 2: runways. Drop any sitting on an excluded gliding site (~2 km).
  for (JsonObjectConst el : els) {
    JsonObjectConst tags = el["tags"].as<JsonObjectConst>();
    const char* aeroway = tags["aeroway"];
    if (aeroway == nullptr || strcmp(aeroway, "runway") != 0) {
      continue;
    }
    JsonArrayConst geom = el["geometry"].as<JsonArrayConst>();
    if (geom.isNull() || geom.size() < 2) {
      continue;
    }
    if (s_runway_count >= kMaxRunways) {
      continue;
    }
    JsonObjectConst a = geom[0].as<JsonObjectConst>();
    JsonObjectConst b = geom[geom.size() - 1].as<JsonObjectConst>();
    const double alat = a["lat"].as<double>();
    const double alon = a["lon"].as<double>();
    const double blat = b["lat"].as<double>();
    const double blon = b["lon"].as<double>();
    const double mlat = (alat + blat) * 0.5;
    const double mlon = (alon + blon) * 0.5;
    bool excluded = false;
    for (size_t k = 0; k < ex_count; ++k) {
      const double dla = mlat - ex_lat[k];
      const double dlo = mlon - ex_lon[k];
      if (dla * dla + dlo * dlo < 0.0004) {  // ~0.02° ≈ 2 km
        excluded = true;
        break;
      }
    }
    if (excluded) {
      continue;
    }
    Runway& rw = s_runways[s_runway_count++];
    rw.le_lat_e7 = toE7(alat);
    rw.le_lon_e7 = toE7(alon);
    rw.he_lat_e7 = toE7(blat);
    rw.he_lon_e7 = toE7(blon);
  }
  s_updated = true;
  const size_t rn = s_runway_count, an = s_aerodrome_count;
  unlock();

  Serial.printf("airfields: %u runways, %u aerodromes\n",
                static_cast<unsigned>(rn), static_cast<unsigned>(an));
  return true;
}

bool consumeUpdated() {
  lock();
  const bool u = s_updated;
  s_updated = false;
  unlock();
  return u;
}

size_t snapshotRunways(Runway* out, size_t max) {
  lock();
  size_t n = s_runway_count;
  if (n > max) n = max;
  memcpy(out, s_runways, n * sizeof(Runway));
  unlock();
  return n;
}

size_t snapshotAerodromes(Aerodrome* out, size_t max) {
  lock();
  size_t n = s_aerodrome_count;
  if (n > max) n = max;
  memcpy(out, s_aerodromes, n * sizeof(Aerodrome));
  unlock();
  return n;
}

}  // namespace services::airfields
