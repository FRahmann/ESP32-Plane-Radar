# Plane Radar

<img width="800" height="450" alt="plane-radar" src="https://github.com/user-attachments/assets/716d0992-dab8-47ba-8f1a-2aec7f607419" />

Firmware that shows a circular **ADS-B radar** around your location on a small round display. Live aircraft from [adsb.fi](https://opendata.adsb.fi/) sweep across a sonar-style grid, with airport runways and (optionally) coastlines drawn from OpenStreetMap.

Two boards are supported from one codebase (selected by a build flag):

| Board | Display | PlatformIO env |
|-------|---------|----------------|
| **ESP32-C3 Super Mini** | 1.28″ GC9A01, 240×240, SPI | `supermini` |
| **Waveshare ESP32-S3-LCD-1.85** | 1.85″ ST77916, 360×360, QSPI | `s3lcd185` |

The **ESP32-S3** build adds an animated radar sweep, a live web config page (map location picker + settings), automatic OSM airfields/coastlines, a beep speaker, multi-network Wi-Fi, and IMU auto-rotation. The original C3 build keeps its simpler feature set.

> Upstream project (original C3/GC9A01 firmware + 3D-printed case): **[MatixYo/ESP32-Plane-Radar](https://github.com/MatixYo/ESP32-Plane-Radar)** · [case on MakerWorld](https://makerworld.com/en/models/2872376-esp32-plane-radar-live-ads-b-on-a-round-display).

---

## ⚠️ Before you publish a fork

`include/config.h` contains **default Wi-Fi credentials** (`kDefaultWifiSsid` / `kDefaultWifiPass`) so the device auto-connects without the portal. **These are committed in plain text.** Before pushing a public repo, blank them:

```cpp
constexpr char kDefaultWifiSsid[] = "";
constexpr char kDefaultWifiPass[] = "";
```

With them empty the device simply falls back to the Wi-Fi setup portal — everything still works, you just enter Wi-Fi once via the captive portal (or add networks later on the web page).

---

## Quick start

```bash
# ESP32-C3 Super Mini
pio run -e supermini -t upload && pio device monitor -e supermini

# Waveshare ESP32-S3-LCD-1.85
pio run -e s3lcd185 -t upload && pio device monitor -e s3lcd185
```

Serial: **115200** baud (USB-CDC). On first boot with no saved Wi-Fi the device opens the **`PlaneRadar-Setup`** captive portal; connect to it, open `http://192.168.4.1`, and enter your home Wi-Fi + latitude/longitude.

---

## Controls

### ESP32-C3 (BOOT button, GPIO 9, active LOW)

| Action | Effect |
|--------|--------|
| **Short tap** | Cycle range preset; saved to flash |
| **Hold 3 s** | Clear Wi-Fi, location & units; reboot into setup portal |

### ESP32-S3-LCD-1.85 (PWR button, GPIO 6)

| Action | Effect |
|--------|--------|
| **Single click** | Toggle map contours (coastline/water) |
| **Double click** | Toggle text labels |
| **Hold ~0.8 s** | Cycle range preset |

> On the S3 the **BOOT pin (GPIO 0) reads spurious LOWs under Wi-Fi load**, so its hold-to-reset is disabled and range cycling lives on the PWR button. To reset Wi-Fi on the S3, use the **“WLAN zurücksetzen”** button on the web config page. (A BOOT tap may still cycle range when the pin behaves.)

Almost everything is also adjustable on the web config page (below), so the buttons are optional.

---

## Web configuration page (ESP32-S3)

While the device is connected to your Wi-Fi, open **`http://plane-radar.local`** (or the device IP) from a phone/PC on the same network. The page needs internet for the map tiles, which is why it runs in normal operation rather than the offline captive portal.

It offers:

- 🗺️ **Map location picker** — tap/drag to set the radar centre (Leaflet + OpenStreetMap)
- **Sweep speed** (revolution time) and **sweep line colour**
- **Aircraft/blip colour**
- **Display brightness**
- **Range** preset, **km/miles**, **runways** on/off
- **Map contours** (coastline/water) on/off
- **Labels** (airport/aircraft text) on/off
- **Beep** on/off + **volume**
- **Auto-rotation** (IMU) on/off
- **Wi-Fi networks** — add/remove multiple networks (see Multi-Wi-Fi below)
- **Reset** — clear all networks/location/units and reboot to the setup portal

All settings persist in NVS and apply live. The captive portal (first-time Wi-Fi setup, offline) keeps simple lat/lon fields as a fallback.

---

## Radar display

### Grid
- Dark blue background, subdued green rings and crosshairs
- **N / S / E / W** at the bezel; range label on the **east** spoke (ring 3 = ¾ of outer radius)
- Center dot. Layout/colours: `include/ui/radar_theme.h` (geometry scales automatically from the 240-px reference to the 360-px panel)

### Sweep (ESP32-S3 only)
A classic radar sweep line rotates clockwise. Aircraft are invisible until the line crosses their bearing, then **light up and slowly fade** over one revolution (afterglow), reappearing on the next pass. Sweep period and colour are configurable; a short **beep** can play on each contact.

### Range presets
`5 / 10 / 15 / 25 / 50 / 100 / 200 km` (label on ring 3; outer radius = ring-3 ÷ 0.75). Default **15 km**. Preset, units and the other settings persist (`planeradar` NVS namespace). Defined in `include/ui/radar_range.h`.

### Aircraft
- **Inside the outer ring** — heading triangle, speed vector (clipped at the ring), callsign / type / altitude tags
- **Outside the ring** (still within fetch radius) — small dot on the screen rim at the correct bearing
- Tags are placed toward the centre

### Airports & runways
- **Built-in:** major airports from OurAirports (`large_airport`), embedded; regenerate with `python3 scripts/build_large_airports.py`
- **ESP32-S3 — automatic, online:** all airfields/runways around your location are fetched from **OpenStreetMap (Overpass)**, refreshed when the centre or range changes. Gliding sites (name starting “Segel…”) are filtered out. Fetch radius is capped at 60 km so the 100/200 km presets stay light.
- Teal runway lines + one ident label per airport; toggle with **runways**

### Map contours (ESP32-S3, optional)
Coastline + large water bodies/rivers from OpenStreetMap, drawn as faint blue lines under the grid. Decimated and capped to fit RAM. Toggle on the web page or with a PWR single-click.

### ADS-B
- Source: `https://opendata.adsb.fi/api/v3/`, poll interval `kAdsbFetchIntervalMs` (3 s)
- Fetch radius scales with the active range so rim dots have data
- Ground aircraft hidden by default (`kAdsbShowGroundAircraft`)
- **ESP32-S3:** ADS-B and the OSM (airfield/contour) fetches run on **core 0**, so the blocking HTTPS requests never stall the sweep, which renders on **core 1**

---

## Multi-Wi-Fi (ESP32-S3)

The device stores a list of networks (NVS) and connects to whichever is in range via **WiFiMulti** — so it works across locations. The built-in default (`config::kDefaultWifi*`) is seeded automatically. Add/remove networks under **Wi-Fi networks** on the web page. The captive portal is used only when none of the known networks are reachable.

## IMU auto-rotation (ESP32-S3)

The on-board **QMI8658** accelerometer rotates the radar in 90° steps so it stays upright however the device is mounted (no magnetometer, so this is gravity-based, not compass north). Works when the device is upright/tilted; a flat-lying device keeps the last rotation. Toggle with **Auto-rotation**. Axis/sign mapping is in `src/services/orientation.cpp`.

---

## Wiring

### GC9A01 ↔ ESP32-C3 Super Mini

| Display | ESP32-C3 |
|---------|----------|
| VCC / GND | 3V3 / GND |
| RST | GPIO 0 |
| CS | GPIO 1 |
| DC | GPIO 10 |
| SDA (MOSI) | GPIO 3 |
| SCL (SCLK) | GPIO 4 |
| BOOT (user) | GPIO 9 |

### Waveshare ESP32-S3-LCD-1.85 (fixed on-board)

| Function | GPIO |
|----------|------|
| QSPI CLK | 40 |
| QSPI D0 / D1 / D2 / D3 | 46 / 45 / 42 / 41 |
| LCD CS | 21 |
| **LCD RST** | **TCA9554 expander, output bit 1** (not a GPIO) |
| Backlight (PWM) | 5 |
| Expander + IMU I²C (SDA / SCL) | 11 / 10 |
| QMI8658 IMU | I²C `0x6B` |
| Speaker I²S (BCLK / WS / DOUT) | 48 / 38 / 47 |
| BOOT / PWR buttons | 0 / 6 |

---

## Configuration

Edit **`include/config.h`** for hardware and behaviour. It is split into an `#if defined(RADAR_BOARD_S3LCD185)` block (S3 pins, QSPI, expander, PWR/speaker) and the C3 defaults.

| Area | Keys |
|------|------|
| Default Wi-Fi | `kDefaultWifiSsid`, `kDefaultWifiPass` (**blank before publishing!**) |
| Portal / mDNS | `kPortalApName`, `kPortalIp`, `kPortalHostname` (`-DWM_MDNS`) |
| Buttons | `kBootPin`, `kPwrPin`, `kPwrLongPressMs`, … |
| Display | QSPI/SPI pins, `kDisplayInvert`, `kDisplayRgbOrder`, `kDisplaySpiWriteHz` |
| Expander / speaker | `kExpander*`, `kSpeaker*` |
| Default location | `kDefaultRadarLat`, `kDefaultRadarLon` |
| ADS-B | `kAdsbFetchIntervalMs`, `kAdsbShowGroundAircraft` |

Runtime settings (range, units, colours, sweep speed, brightness, beep, contours, labels, auto-rotate, Wi-Fi list) are stored in NVS and set via the web page / buttons.

---

## How the S3 display works (porting notes)

The ST77916 speaks **QSPI-DBI**, which LovyanGFX’s generic `Panel_LCD` does **not** frame correctly (commands need a `0x02 …` wrapper, pixel writes a `0x32 … 0x2C` wrapper). The vendored `Panel_ST77916` in `include/hardware/lgfx_config.hpp` therefore derives from LovyanGFX’s **`Panel_SH8601Z`** (which implements that framing) and only swaps in the ST77916 init sequence, 360×360 geometry and a PWM backlight. This needs the LovyanGFX **develop** branch (pinned in `platformio.ini`).

Other board quirks (all handled in firmware):

- **Reset via I²C expander** — TCA9554 at `0x20`, output bit 1, pulsed in `displayInit()` before `tft.init()` (panel `pin_rst = -1`).
- **Inversion at runtime** — `tft.invertDisplay()` is called in `displayInit()`; setting it during panel init does not stick. The panel needs INVON for correct colours.
- **Everything renders into a PSRAM sprite, then `pushSprite`** — many small direct draws to this panel are unreliable (status screens included).
- **I²S left idle after install** — a continuously-running I²S DMA crashed bring-up; it’s started only for the duration of a beep.

### Bring-up checklist (black/wrong screen)
1. **Black** → reset path: confirm TCA9554 at `0x20` on SDA 11/SCL 10, reset bit `1`.
2. **No image but init runs** → lower `kDisplaySpiWriteHz` (40 → 20 MHz).
3. **Colours inverted/swapped** → flip `kDisplayInvert` / `kDisplayRgbOrder`.
4. **Sprite alloc fails** → `board_build.arduino.memory_type = qio_opi` + `-DBOARD_HAS_PSRAM` (set in the `s3lcd185` env).

---

## Project layout

```
include/
  config.h
  hardware/      lgfx_config.hpp, display.h, display_font.h
  data/          large_airports.h
  ui/            radar_theme.h, radar_range.h, radar_display.h,
                 runway_overlay.h, status_screens.h
  services/      wifi_setup.h, wifi_store.h, config_server.h,
                 radar_location.h, adsb_client.h, custom_airfields.h,
                 map_contours.h, audio.h, orientation.h
data/  ui_font.vlw          — embedded smooth UI font (Noto Sans Bold)
scripts/  build_large_airports.py, merge_firmware.py
src/
  main.cpp
  data/      large_airports_data.cpp
  hardware/  display.cpp, display_font.cpp
  ui/        radar_display.cpp, radar_range.cpp, runway_overlay.cpp,
             status_screens.cpp
  services/  wifi_setup.cpp, wifi_store.cpp, config_server.cpp,
             radar_location.cpp, adsb_client.cpp, custom_airfields.cpp,
             map_contours.cpp, audio.cpp, orientation.cpp
```

> `custom_airfields.*` hosts the online OSM airfield fetch; `map_contours.*` the coastline/water fetch; `config_server.*` the web page; `orientation.*` the IMU; `audio.*` the beep; `wifi_store.*` the multi-network list.

---

## Web-flashable image (ESP32-C3)

Single `.bin` for [esptool-js](https://espressif.github.io/esptool-js/) (ESP32-C3, 4 MB, flash at `0x0`):

```bash
pio run -e supermini
pio run -t merge -e supermini    # → .pio/build/supermini/firmware-merged.bin
```

Put the board in download mode (hold **BOOT**, tap **RESET**), then flash with Chrome/Edge over USB. The S3 build is large (16 MB flash, OPI PSRAM); flash it via `pio run -e s3lcd185 -t upload`.

## Dependencies

- [LovyanGFX](https://github.com/lovyan03/LovyanGFX) (C3: `^1.2.7`; S3: `develop` branch — QSPI)
- [WiFiManager](https://github.com/tzapu/WiFiManager)
- [ArduinoJson](https://github.com/bblanchon/ArduinoJson)
- ESP32 Arduino `WiFiMulti`, `WebServer`, I²S, `Wire` (core libraries)
