#include "services/wifi_store.h"

#include <Arduino.h>
#include <Preferences.h>

#include <cstring>

#include "config.h"

namespace services::wifi_store {

namespace {

constexpr char kNamespace[] = "wifinets";
constexpr char kKeyCount[] = "count";
constexpr char kKeyBlob[] = "nets";

Net s_nets[kMax];
size_t s_count = 0;

void persist() {
  Preferences prefs;
  if (!prefs.begin(kNamespace, false)) {
    return;
  }
  prefs.putUChar(kKeyCount, static_cast<uint8_t>(s_count));
  prefs.putBytes(kKeyBlob, s_nets, s_count * sizeof(Net));
  prefs.end();
}

int indexOf(const char* ssid) {
  for (size_t i = 0; i < s_count; ++i) {
    if (strncmp(s_nets[i].ssid, ssid, sizeof(s_nets[i].ssid)) == 0) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

}  // namespace

void init() {
  Preferences prefs;
  if (prefs.begin(kNamespace, true)) {
    size_t n = prefs.getUChar(kKeyCount, 0);
    if (n > kMax) {
      n = kMax;
    }
    if (n > 0) {
      prefs.getBytes(kKeyBlob, s_nets, n * sizeof(Net));
    }
    s_count = n;
    prefs.end();
  }

  // Seed the built-in default network if not already present.
  if (config::kDefaultWifiSsid[0] != '\0' &&
      indexOf(config::kDefaultWifiSsid) < 0) {
    add(config::kDefaultWifiSsid, config::kDefaultWifiPass);
  }
  Serial.printf("wifi_store: %u network(s)\n", static_cast<unsigned>(s_count));
}

size_t count() { return s_count; }

const Net* list() { return s_nets; }

bool add(const char* ssid, const char* pass) {
  if (ssid == nullptr || ssid[0] == '\0') {
    return false;
  }
  const int existing = indexOf(ssid);
  Net* slot = nullptr;
  if (existing >= 0) {
    slot = &s_nets[existing];  // update password
  } else if (s_count < kMax) {
    slot = &s_nets[s_count++];
  } else {
    return false;  // list full
  }
  strncpy(slot->ssid, ssid, sizeof(slot->ssid) - 1);
  slot->ssid[sizeof(slot->ssid) - 1] = '\0';
  strncpy(slot->pass, pass ? pass : "", sizeof(slot->pass) - 1);
  slot->pass[sizeof(slot->pass) - 1] = '\0';
  persist();
  return true;
}

bool remove(size_t index) {
  if (index >= s_count) {
    return false;
  }
  for (size_t i = index; i + 1 < s_count; ++i) {
    s_nets[i] = s_nets[i + 1];
  }
  --s_count;
  persist();
  return true;
}

void clear() {
  s_count = 0;
  persist();
}

}  // namespace services::wifi_store
