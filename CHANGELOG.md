# Changelog

All notable changes to **chaos STATION** from the v2 review session are documented here.
Each entry is grouped by the feature area it belongs to and lists the exact files,
symbols, and behavior affected.

The format is loosely based on [Keep a Changelog](https://keepachangelog.com/),
and the project targets the ESP32 (Arduino framework via PlatformIO).

---

## [Unreleased] — Session Worklog (2026-07-06 → 2026-07-07)

### Summary

Three bodies of work were delivered in this session, all verified with a clean
PlatformIO build (**73.6% flash / 15.3% RAM**):

1. **Code review & audit** — identified bugs, optimizations, and feature ideas.
2. **OLED multi-mode display** — expanded the secondary display from 3 to 10 modes.
3. **Multi-network WiFi failover** — automatic fallback between configured networks.

Each is detailed below.

---

## 1. Code Review & Audit

A full read-through of `src/main.cpp` (965 lines), `data/index.html`,
`data/app.js`, and `data/style.css` produced a categorized list of issues and
opportunities. No code was changed in this step — it set the agenda.

### Bugs identified

| # | Location | Issue |
|---|---|---|
| 1 | `main.cpp` pomodoro arc | Progress hardcoded to 25 min regardless of chosen duration. |
| 2 | `main.cpp` `logData()` | `history.csv` appended forever; will eventually fill LittleFS. |
| 3 | `index.html` | Chart.js loaded from public CDN — breaks on a LAN without internet. |
| 4 | `main.cpp` `renderOLED()` | `nan C` printed when DHT/BMP sensors fail (no `isnan()` guard). |
| 5 | `main.cpp` `drawAnimatedSun()` | Sun core color hardcoded to `NORD13` — palette leak on other themes. |
| 6 | `main.cpp` settings POST | No guard against disabling all TFT pages. |
| 7 | `main.cpp` | WiFi credentials hardcoded in source and committed to git. |
| 8 | `main.cpp` `refreshSensors()` | Daily high/low reset only fires inside the BMP-success branch. |
| 9 | `main.cpp` | `tftHistTemp[]` written every log but never read (dead memory). |
| 10 | `main.cpp` | No HTTP auth / no body-size cap on POST handlers. |
| 11 | `main.cpp` loop | WiFi reconnect has no backoff — hammers `WiFi.begin()`. |
| 12 | `app.js` `fetchStatus()` | Swallows errors silently — never sets the connection pill offline. |
| 13 | `app.js` `fetchHistory()` | No downsampling — thousands of CSV rows lag the chart. |

### Optimizations identified

- **#14** `drawArc()` — pixel-by-pixel SPI writes are the biggest perf/flicker hit.
- **#15** No double buffering — every 2 s redraw flickers.
- **#16** `refreshSensors()` runs synchronously on the HTTP loop and blocks requests.
- **#17** `saveSettingsFS()` / `logData()` do blocking LittleFS writes in `loop()`.
- **#18** DHT11 polled every 2 s — faster than the sensor's ~1 Hz spec needs.
- **#19** Polling could be replaced with WebSocket/SSE for real-time push.
- **#20** `WiFi.localIP()`, `WiFi.RSSI()`, `ESP.getFreeHeap()` recomputed per frame.
- **#21** `ArduinoJson` docs could use `filter()` to shrink payload parsing.

### Features proposed

OTA updates, WiFiManager captive portal, MQTT + Home Assistant, WebSocket push,
push notifications, a TFT temperature graph, brightness/backlight PWM,
OTA alarms/scheduler, air-quality sensors, multi-station sync, InfluxDB logging.

### BLE direction proposed

Eight ranked BLE feature ideas (Environmental Sensing Service, companion control,
beacon advertising, HID keyboard, Current Time Service, third-party sensor ingest,
proximity automation, BLE mesh). Recommended first pass: Environmental Sensing +
beacon advertising.

> The audit is preserved in the session transcript; this changelog records only
> the items that were subsequently **implemented** (sections 2, 3, and 4 below).

---

## 2. Added — OLED Multi-Mode Display

Expanded the 128×64 SSD1306 OLED from 3 fixed readouts to **10 user-selectable
modes**, switchable live from the web dashboard. All new modes are backed by data
the firmware already holds — no new sensors or wiring required.

### New OLED modes (`src/main.cpp` `renderOLED()`)

| `oledMode` | Icon | Name | Data source |
|:--:|:--:|---|---|
| 0 | 🌡️ | Weather | DHT + BMP live sensors *(existing, improved)* |
| 1 | 🕐 | Clock | NTP time *(existing)* |
| 2 | 💻 | System | WiFi / heap / uptime *(existing)* |
| 3 | 🍅 | **Pomodoro** | running timer + `pomodoroDuration` |
| 4 | ⛅ | **Forecast** | pressure-trend heuristic |
| 5 | 📈 | **Temp Graph** | `tftHistTemp[]` history ring buffer |
| 6 | 📝 | **Notes** | `tftNotes` |
| 7 | ✅ | **Tasks** | `tftTodos[]` |
| 8 | ⌚ | **Analog** | NTP time (trigonometric hands) |
| 9 | ⠿ | **Binary** | NTP time (BCD dot matrix) |

### Firmware changes (`src/main.cpp`)

- **New `oledHeader()` helper** — renders a consistent title-left / clock-right
  header strip with a divider line for the new modes.
- **`OLED_MODE_NAMES[10]`** constant array — mode title strings.
- **`OLED_MONTHS[12]`** constant array — 3-letter month names for the analog
  clock's date strip.
- **`renderOLED()` rewritten** as a `switch (oledMode)` with a branch per mode.
  Modes 0–2 preserved their original layout; 3–9 are new.
- **`oledMode` constrained to `0–9`** in both `initFS()` (config load) and
  `handlePostSettings()` (POST), so a stale `config.json` can't break rendering.
- **Weather mode (0) hardened** — `isnan()` guards added so a failed sensor
  shows `--` instead of `nan C` (audit bug #4).
- **`pomodoroDuration`** global added and stored on timer start — the OLED
  Pomodoro progress bar and the TFT arc now scale to the actual session length
  (also fixes audit bug #1 for the OLED side).

### Web UI changes (`data/index.html`)

- **OLED Mode button group** expanded from 3 to 10 buttons. The existing
  `app.js` wiring handles any `data-mode` value generically, so no JS changes
  were needed — the buttons work immediately and persist via `submitSettings()`.

### Behavior notes

- The active mode is persisted in `/config.json` and restored on reboot.
- Modes **3 (Pomodoro)** and **5 (Temp Graph)** depend on runtime state:
  - Pomodoro shows `READY 25:00` until a session is started from the web.
  - Temp Graph shows `Collecting...` until ≥ 2 history points exist
    (one log every 10 min, or the first log fires at boot).

---

## 3. Added — Multi-Network WiFi Failover

Replaced the single hardcoded SSID/password with an ordered **network list**.
The device walks the list at boot and rotates to the next network on any drop,
so it stays online when the primary is unavailable.

### Firmware changes (`src/main.cpp`)

- **New `WifiNetwork` struct** and **`WIFI_NETWORKS[]`** array (priority order):
  ```cpp
  WifiNetwork WIFI_NETWORKS[] = {
    { "Airtel_a204",   "rahulkhanki"  },   // primary
    { "wifi",          "wifiwifiwifi" }    // fallback
  };
  ```
  Any number of `{ ssid, pass }` pairs may be added.
- **`WIFI_NET_COUNT`** computed from the array; **`wifiNetIndex`** tracks the
  active network.
- **`WIFI_PER_NET_TIMEOUT`** (10 s) caps each connection attempt.
- **New `tryWifi(const WifiNetwork&)` helper** — attempts one network within
  the timeout, prints dotted progress, returns success/failure, disconnects on
  failure.
- **`initWiFi()` rewritten** — walks the list in order, stops at the first
  success, retries the primary once if all fail, then configures NTP regardless.
- **Loop reconnect rewritten** — every 30 s, if the link is down, it now
  **rotates to the next network** instead of retrying the same one. Over time
  it cycles through all configured networks and lands on whichever is up.

### Behavior matrix

| Situation | Result |
|---|---|
| Boot, primary up | Connects to primary in seconds |
| Boot, primary down | Falls through to `wifi` after 10 s |
| Boot, all down | Retries primary, then runs offline until loop recovery |
| Running, primary drops | Next 30 s check fails over to `wifi` |
| Running, `wifi` also drops | Keeps rotating each cycle until one returns |

---

## 4. Documentation

All documentation was updated to match the implemented features.

### `README.md`
- OLED feature bullet rewritten to advertise **10 selectable modes**.
- **New `🖥️ OLED Display Modes` section** — full table of all 10 modes with
  persistence and sync notes.
- Quickstart step 2 rewritten for the **`WIFI_NETWORKS[]`** list and failover.

### `README_ARCHITECTURE.md`
- `data/` file-tree comment — corrected stale theme names
  ("Dark Mode, Ocean, Sunset" → "Nord, Cyberpunk, Coder, Gruvbox").
- §2.1 step 4 — describes the multi-network failover at boot and the loop's
  rotation on drop.
- §2.4 API Endpoints — expanded from 3 to the full set of 10 endpoints with
  accurate JSON shapes.
- **New §2.5 OLED Multi-Mode Renderer** — documents the 10 `oledMode` branches,
  their data sources, the `0–9` constraint, and persistence.
- §3 Frontend — replaced the outdated "tints warmer above 28°C" claim with the
  real 4-Theme synchronization engine and corrected polling cadence
  (2 s data / 3 s status).

### `README_PIN_DIAGRAM.md`
- No changes — purely hardware/wiring, unaffected by software features.

### `CHANGELOG.md`
- This file — a complete record of the session's implemented work.

---

## Build Verification

Every change above was verified with a clean PlatformIO build:

```
RAM:   [==        ]  15.3% (used 50152 bytes from 327680 bytes)
Flash: [=======   ]  73.6% (used 964457 bytes from 1310720 bytes)
========================= [SUCCESS] =========================
```

No compiler warnings. Total delta from the session start point is small
(firmware grew from 964 029 → 964 457 bytes, +428 bytes net).

---

## Out of Scope (Proposed, Not Yet Implemented)

The following items from the audit were **not** implemented in this session and
remain available as next steps:

- **Bug fixes:** `history.csv` rotation (#2), bundling Chart.js into LittleFS
  (#3), `drawAnimatedSun` palette fix (#5), all-pages-off guard (#6),
  WiFiManager portal to remove hardcoded credentials (#7), daily reset outside
  the BMP-success branch (#8), HTTP auth/body-size cap (#10), reconnect
  backoff (#11), `fetchStatus` error surfacing (#12), chart downsampling (#13).
- **Optimizations:** `drawArc()` batching (#14), double buffering via
  `GFXcanvas16` (#15), decoupling sensor reads onto a FreeRTOS task (#16),
  async filesystem writes (#17), reduced DHT cadence (#18), WebSocket push
  (#19), snapshot cached values (#20), ArduinoJson `filter()` (#21).
- **Features:** OTA, MQTT/Home Assistant, push notifications, TFT temp graph
  rendering, brightness/backlight PWM, InfluxDB logging.
- **BLE:** Environmental Sensing Service + beacon advertising (recommended
  first pass), companion control, HID keyboard, Current Time Service,
  third-party sensor ingest, proximity automation, BLE mesh.

---

## Files Touched This Session

| File | Change |
|---|---|
| `src/main.cpp` | OLED 10-mode renderer, WiFi failover, pomodoro duration, NaN guards, constants |
| `data/index.html` | 7 new OLED mode buttons |
| `README.md` | OLED modes section, WiFi quickstart |
| `README_ARCHITECTURE.md` | API list, OLED renderer section, themes, failover |
| `CHANGELOG.md` | Added (this file) |
