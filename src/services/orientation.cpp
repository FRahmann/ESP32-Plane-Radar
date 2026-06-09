#include "services/orientation.h"

#include "config.h"

#if defined(RADAR_BOARD_S3LCD185)

#include <Arduino.h>
#include <Wire.h>

#include <cmath>

#include "ui/radar_range.h"

namespace services::orientation {

namespace {

// QMI8658 registers
constexpr uint8_t kRegWhoAmI = 0x00;  // = 0x05
constexpr uint8_t kRegCtrl1 = 0x02;
constexpr uint8_t kRegCtrl2 = 0x03;   // accel config
constexpr uint8_t kRegCtrl7 = 0x08;   // sensor enable
constexpr uint8_t kRegAccelX = 0x35;  // ax_l..az_h (6 bytes, LE)

uint8_t s_addr = 0;
bool s_present = false;
int s_current = 0;
int s_cand = 0;
int s_cand_count = 0;
unsigned long s_last_log = 0;

bool writeReg(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(s_addr);
  Wire.write(reg);
  Wire.write(val);
  return Wire.endTransmission() == 0;
}

uint8_t readReg(uint8_t reg) {
  Wire.beginTransmission(s_addr);
  Wire.write(reg);
  Wire.endTransmission(false);
  Wire.requestFrom(static_cast<int>(s_addr), 1);
  return Wire.available() ? Wire.read() : 0;
}

void readRegs(uint8_t reg, uint8_t* buf, int n) {
  Wire.beginTransmission(s_addr);
  Wire.write(reg);
  Wire.endTransmission(false);
  Wire.requestFrom(static_cast<int>(s_addr), n);
  for (int i = 0; i < n && Wire.available(); ++i) {
    buf[i] = Wire.read();
  }
}

}  // namespace

void init() {
  Wire.begin(static_cast<int>(config::kExpanderPinSda),
             static_cast<int>(config::kExpanderPinScl), 400000);
  for (uint8_t addr : {0x6B, 0x6A}) {
    s_addr = addr;
    if (readReg(kRegWhoAmI) == 0x05) {
      s_present = true;
      break;
    }
  }
  if (!s_present) {
    Serial.println("orient: QMI8658 not found");
    return;
  }
  writeReg(kRegCtrl1, 0x60);  // address auto-increment
  writeReg(kRegCtrl2, 0x04);  // accel ±2g, low ODR (orientation only)
  writeReg(kRegCtrl7, 0x01);  // enable accelerometer
  Serial.printf("orient: QMI8658 @ 0x%02X\n", s_addr);
}

bool present() { return s_present; }

int update() {
  if (!s_present) {
    return -1;
  }
  uint8_t b[6] = {0};
  readRegs(kRegAccelX, b, 6);
  const int16_t ax = static_cast<int16_t>(b[0] | (b[1] << 8));
  const int16_t ay = static_cast<int16_t>(b[2] | (b[3] << 8));
  const int16_t az = static_cast<int16_t>(b[4] | (b[5] << 8));

  // Need a clear in-plane gravity component; if the device lies flat (gravity
  // mostly on Z), keep the current rotation.
  const long inplane = static_cast<long>(ax) * ax + static_cast<long>(ay) * ay;
  const long zsq = static_cast<long>(az) * az;
  if (inplane > zsq / 3) {
    float ang = atan2f(static_cast<float>(ay), static_cast<float>(ax)) *
                57.29578f;  // device-frame gravity direction
    int cand = static_cast<int>(lroundf(ang / 90.0f));
    cand = ((cand % 4) + 4) % 4;
    if (cand == s_cand) {
      if (s_cand_count < 4) {
        ++s_cand_count;
      }
      if (s_cand_count >= 3) {
        s_current = cand;
      }
    } else {
      s_cand = cand;
      s_cand_count = 0;
    }
  }

  const unsigned long now = millis();
  if (now - s_last_log > 3000) {
    s_last_log = now;
    Serial.printf("orient: ax=%d ay=%d az=%d rot=%d\n", ax, ay, az, s_current);
  }
  return s_current;
}

}  // namespace services::orientation

#else  // non-S3: no IMU

namespace services::orientation {
void init() {}
bool present() { return false; }
int update() { return -1; }
}  // namespace services::orientation

#endif  // RADAR_BOARD_S3LCD185
