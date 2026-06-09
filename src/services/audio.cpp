#include "services/audio.h"

#include "config.h"

#if defined(RADAR_BOARD_S3LCD185)

#include <Arduino.h>
#include <driver/i2s.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include <cmath>

namespace services::audio {

namespace {

constexpr i2s_port_t kPort = I2S_NUM_0;
constexpr int kSampleRate = 16000;
constexpr float kToneHz = 1320.0f;        // crisp "ping"
constexpr int kToneMs = 45;
constexpr int kToneSamples = kSampleRate * kToneMs / 1000;
constexpr int kMaxAmplitude = 11000;      // at 100 % volume

SemaphoreHandle_t s_beep_sem = nullptr;
bool s_ready = false;
volatile int s_amplitude = 7000;          // ~64 % default

void playTone() {
  i2s_start(kPort);
  int16_t buf[128];
  size_t written = 0;
  for (int i = 0; i < kToneSamples;) {
    int n = 0;
    for (; n < 128 && i < kToneSamples; ++n, ++i) {
      float s = sinf(2.0f * 3.14159265f * kToneHz * i / kSampleRate);
      // Short attack/decay envelope to avoid clicks.
      float env = 1.0f;
      const int fade = kSampleRate * 4 / 1000;  // 4 ms
      if (i < fade) env = static_cast<float>(i) / fade;
      else if (i > kToneSamples - fade) env = static_cast<float>(kToneSamples - i) / fade;
      buf[n] = static_cast<int16_t>(s * env * s_amplitude);
    }
    i2s_write(kPort, buf, n * sizeof(int16_t), &written, portMAX_DELAY);
  }
  // Flush a few silent samples so the amp settles, then idle the bus.
  int16_t zeros[64] = {0};
  i2s_write(kPort, zeros, sizeof(zeros), &written, portMAX_DELAY);
  i2s_stop(kPort);
}

void beepTask(void*) {
  for (;;) {
    if (xSemaphoreTake(s_beep_sem, portMAX_DELAY) == pdTRUE) {
      playTone();
    }
  }
}

}  // namespace

void begin() {
  if (s_ready) {
    return;
  }
  i2s_config_t cfg = {};
  cfg.mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_TX);
  cfg.sample_rate = kSampleRate;
  cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
  cfg.channel_format = I2S_CHANNEL_FMT_ONLY_LEFT;
  cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  cfg.intr_alloc_flags = 0;
  cfg.dma_buf_count = 4;
  cfg.dma_buf_len = 256;
  cfg.use_apll = false;
  cfg.tx_desc_auto_clear = true;

  if (i2s_driver_install(kPort, &cfg, 0, nullptr) != ESP_OK) {
    Serial.println("audio: i2s install failed");
    return;
  }
  i2s_pin_config_t pins = {};
  pins.bck_io_num = config::kSpeakerBclkPin;
  pins.ws_io_num = config::kSpeakerWsPin;
  pins.data_out_num = config::kSpeakerDoutPin;
  pins.data_in_num = I2S_PIN_NO_CHANGE;
  i2s_set_pin(kPort, &pins);
  i2s_zero_dma_buffer(kPort);
  i2s_stop(kPort);  // keep the bus idle until a beep — avoids continuous DMA

  s_beep_sem = xSemaphoreCreateBinary();
  xTaskCreatePinnedToCore(beepTask, "beep", 4096, nullptr, 1, nullptr, 0);
  s_ready = true;
  Serial.println("audio: I2S speaker ready");
}

void requestBeep() {
  if (s_ready && s_beep_sem != nullptr) {
    xSemaphoreGive(s_beep_sem);
  }
}

void setVolume(uint8_t pct) {
  if (pct > 100) pct = 100;
  s_amplitude = kMaxAmplitude * pct / 100;
}

}  // namespace services::audio

#else  // non-S3: no speaker hardware

namespace services::audio {
void begin() {}
void requestBeep() {}
void setVolume(uint8_t) {}
}  // namespace services::audio

#endif  // RADAR_BOARD_S3LCD185
