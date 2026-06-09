#pragma once

#include <cstddef>
#include <cstdint>

namespace services::contours {

// Coastline / large-water outlines fetched from OpenStreetMap (Overpass),
// stored as decimated polylines (a shared point pool + per-line spans). Held in
// RAM, refreshed when the centre/range changes; the network fetch runs on core 0.

struct Point {
  int32_t lat_e7;
  int32_t lon_e7;
};

struct PolySpan {
  uint16_t start;  // index into the point pool
  uint16_t count;  // number of points in this polyline
};

constexpr size_t kMaxPoints = 900;
constexpr size_t kMaxPolys = 96;

void init();

/** Fetch coastline + water polylines within radius_km. Core-0 / blocking. */
bool fetchNearby(double lat, double lon, float radius_km);

/** True once after each successful fetch. */
bool consumeUpdated();

/**
 * Copy the polyline index and point pool into caller buffers under the mutex.
 * Returns the polyline count; *out_points is set to the number of points copied.
 */
size_t snapshot(PolySpan* polys, size_t max_polys, Point* pts, size_t max_pts,
                size_t* out_points);

}  // namespace services::contours
