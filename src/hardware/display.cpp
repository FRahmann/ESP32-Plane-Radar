#include "hardware/display.h"

#include "hardware/display_font.h"

#if defined(RADAR_BOARD_S3LCD185)
#include <Arduino.h>
#include <Wire.h>

#include "config.h"
#endif

LGFX tft;

#if defined(RADAR_BOARD_S3LCD185)
namespace {

// Write one byte to a TCA9554 register over I2C. Returns true on ACK.
bool expanderWrite(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(config::kExpanderI2cAddr);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

// The ST77916 reset line hangs off a TCA9554 I2C expander, not a GPIO.
// Drive it through a low pulse before LovyanGFX initialises the panel.
//   reg 0x01 = output port, reg 0x03 = config (1 = input, 0 = output).
// Only the LCD-reset bit is switched to an output; the rest are left as
// inputs (power-on default) so we don't disturb other expander loads.
void expanderResetDisplay() {
  Wire.begin(static_cast<int>(config::kExpanderPinSda),
             static_cast<int>(config::kExpanderPinScl), 400000);

  const uint8_t bit = static_cast<uint8_t>(1u << config::kExpanderLcdResetBit);

  // LCD-reset bit -> output (clear its config bit, keep others as inputs so we
  // don't disturb the panel/backlight defaults). Only this bit is pulsed.
  expanderWrite(0x03, static_cast<uint8_t>(~bit));

  expanderWrite(0x01, bit);  // RST high (idle)
  delay(10);
  expanderWrite(0x01, 0x00);  // RST low (assert reset)
  delay(20);
  expanderWrite(0x01, bit);  // RST high (release)
  delay(120);
}

}  // namespace
#endif  // RADAR_BOARD_S3LCD185

void displayInit() {
#if defined(RADAR_BOARD_S3LCD185)
  expanderResetDisplay();
#endif
  tft.init();
  tft.setRotation(0);
  tft.setBrightness(255);
  tft.setTextWrap(false);
#if defined(RADAR_BOARD_S3LCD185)
  // ST77916 needs INVON for correct colours. Applying it during panel init
  // does not stick, so set it here at runtime (proven via invertDisplay test).
  tft.invertDisplay(config::kDisplayInvert);
#endif
  displayFontInit();
}
