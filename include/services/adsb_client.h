#pragma once

#include <cstddef>

namespace services::adsb {

struct Aircraft {
  float lat;
  float lon;
  float nose_deg;
  float track_deg;
  float gs_knots;
  char callsign[9];
  char type[5];
  char alt[12];
};

constexpr size_t kMaxAircraft = 64;

/** Create the list mutex. Call once before fetchUpdate/snapshot are used. */
void init();

size_t aircraftCount();
const Aircraft* aircraftList();

/**
 * Copy up to max aircraft into out under the list mutex; returns the count.
 * Safe to call from a different core than fetchUpdate (the render path uses
 * this so a background fetch can't tear the list mid-frame).
 */
size_t snapshotAircraft(Aircraft* out, size_t max);

/** Fetch aircraft within fetch_radius_km of center_lat/lon from adsb.fi. */
bool fetchUpdate(double center_lat, double center_lon, float fetch_radius_km);

}  // namespace services::adsb
