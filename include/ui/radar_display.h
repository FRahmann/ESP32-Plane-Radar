#pragma once

namespace ui {

/** Draw the static sonar/radar grid (black disc, green overlay, labels). */
void radarDisplayDraw();

/** Redraw aircraft only (blits cached grid; no full-screen clear). */
void radarDisplayRefreshAircraft();

/**
 * Advance and render the rotating radar sweep. Call frequently from the main
 * loop while the radar is visible; throttles itself to ~30 fps. No-op until a
 * base frame (grid + aircraft) has been drawn. Sweep is S3-only.
 */
void radarDisplaySweepTick();

/** Set radar rotation (0..3 → 0/90/180/270°) from the IMU. S3-only. */
void radarDisplaySetRotation(int rot);

}  // namespace ui
