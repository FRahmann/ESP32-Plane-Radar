#pragma once

namespace services::orientation {

/** Probe + configure the QMI8658 accelerometer (shared I2C bus). */
void init();

/** True if the IMU was detected. */
bool present();

/**
 * Read the accelerometer and return the desired display rotation (0..3, i.e.
 * 0/90/180/270°) to keep the radar upright, with hysteresis. Returns the last
 * stable value while the device lies flat (no in-plane gravity). -1 if absent.
 */
int update();

}  // namespace services::orientation
