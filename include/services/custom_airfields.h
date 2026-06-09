#pragma once

#include <cstddef>
#include <cstdint>

namespace services::airfields {

// Airfields are fetched automatically from OpenStreetMap (Overpass) around the
// configured centre — runway lines plus aerodrome labels. Held in RAM and
// refreshed when the location changes; the network fetch runs on core 0.

struct Runway {
  int32_t le_lat_e7;
  int32_t le_lon_e7;
  int32_t he_lat_e7;
  int32_t he_lon_e7;
};

struct Aerodrome {
  char ident[8];
  int32_t lat_e7;
  int32_t lon_e7;
};

constexpr size_t kMaxRunways = 64;
constexpr size_t kMaxAerodromes = 48;

/** Create the data mutex. Call once at boot. */
void init();

/**
 * Fetch runways + aerodromes within radius_km of (lat,lon) from the OSM
 * Overpass API and replace the current set. Blocking HTTPS — call from the
 * core-0 task, not the render loop. Returns true on a successful update.
 */
bool fetchNearby(double lat, double lon, float radius_km);

/** True once after each successful fetch (so the loop can trigger a redraw). */
bool consumeUpdated();

// Thread-safe snapshots for the render core.
size_t snapshotRunways(Runway* out, size_t max);
size_t snapshotAerodromes(Aerodrome* out, size_t max);

}  // namespace services::airfields
