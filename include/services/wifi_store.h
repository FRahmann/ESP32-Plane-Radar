#pragma once

#include <cstddef>

namespace services::wifi_store {

// A list of known Wi-Fi networks (persisted in NVS). The device connects to
// whichever one is in range (via WiFiMulti), so it works across locations.
struct Net {
  char ssid[33];
  char pass[65];
};

constexpr size_t kMax = 8;

/** Load the list from flash and seed the built-in default if empty. */
void init();

size_t count();
const Net* list();

/** Add a network, or update the password if the SSID already exists. */
bool add(const char* ssid, const char* pass);

/** Remove the network at index. */
bool remove(size_t index);

/** Remove all stored networks (the built-in default is re-seeded by init()). */
void clear();

}  // namespace services::wifi_store
