#pragma once

#include <cstdint>

namespace services::audio {

/** Initialise the I2S speaker output and start the beep task (core 0). */
void begin();

/** Request a short beep (non-blocking). Plays on the beep task. */
void requestBeep();

/** Set beep volume 0–100 %. */
void setVolume(uint8_t pct);

}  // namespace services::audio
