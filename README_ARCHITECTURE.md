# ESP32 Smart Weather Station - Architecture & Code Structure

This document outlines the software architecture, project structure, and hardware logic of the ESP32 Smart Weather Station.

## 1. Project Directory Structure

```text
esp-2display-weather/
├── platformio.ini              # PlatformIO configuration (board, upload speeds, LittleFS, dependencies)
├── README_PIN_DIAGRAM.md       # Hardware wiring map (ESP32, ST7735 TFT, SSD1306 OLED, BMP180, DHT11)
├── README_ARCHITECTURE.md      # This file
├── data/                       # LittleFS File System (Served to the Web Client)
│   ├── index.html              # Main Web Dashboard UI
│   ├── style.css               # CSS featuring Nord, Cyberpunk, Coder & Gruvbox themes
│   └── app.js                  # Frontend logic: Chart.js rendering, AJAX API fetching
└── src/
    └── main.cpp                # Master C++ Firmware
```

## 2. Firmware Architecture (`src/main.cpp`)

The ESP32 firmware operates on a concurrent loop architecture, handling multiple subsystems simultaneously without blocking (using `millis()` instead of `delay()`).

### 2.1. Subsystems Initialization (`setup()`)
1. **Serial & I2C/SPI:** Initializes Serial output, I2C bus for the OLED and BMP180, and hardware SPI for the TFT.
2. **Sensors:** Bootstraps the DHT11 and BMP180 sensors.
3. **Displays:** Initializes the 1.8" ST7735 TFT and 0.96" SSD1306 OLED.
4. **Network & Time:** Connects to WiFi using a **multi-network failover** — walks the `WIFI_NETWORKS[]` list in order, falling through to the next if one is unavailable, then syncs with an NTP server (`pool.ntp.org`) and establishes the local timezone offset. The main loop also rotates to the next network whenever the active connection drops.
5. **Filesystem (LittleFS):** Mounts the internal flash memory to serve HTML/CSS/JS and store historical CSV data.
6. **Web Server:** Configures RESTful API routes (`/api/data`, `/api/history`) and serves static files.

### 2.2. The Main Loop (`loop()`)
The `loop()` function is entirely non-blocking and relies on delta-time (`millis() - lastTime`) to trigger events:
- **Sensor Reading (Every 2s):** Fetches fresh Temperature, Humidity, and Pressure data.
- **Web Server Polling (Continuous):** Listens for incoming HTTP requests and serves API JSON/Files.
- **Display Updating (Every 2s / Dynamic):** Re-renders the OLED and TFT displays.
- **Carousel State Machine (Every 8s):** Cycles the TFT display through multiple "Pages" to show more data.
- **Data Logging (Every 10m):** Appends the latest sensor readings to `/history.csv` on the flash drive.

### 2.3. TFT Custom Graphics Engine (Trigonometry)
Because the Adafruit GFX library lacks complex shapes, we implemented custom mathematical graphics:
- **`drawArc()`:** Uses `sin()` and `cos()` to draw thick circular progress rings for Humidity and Pressure.
- **`drawNeedle()`:** Calculates polar coordinates to draw an angled Analog Speedometer needle based on the Temperature.
- **`drawSun()` / `drawRain()`:** Dynamic animations that render in the corner depending on current weather conditions.

### 2.4. Web Server API Endpoints
- `GET /` -> Serves `index.html`
- `GET /style.css`, `GET /app.js` -> Dashboard assets
- `GET /api/data` -> Live sensor JSON: `{"tempDHT":25.5,"humDHT":60,"tempBME":26.0,"pressBME":101325,"uptime":120,"freeHeap":230000,"rssi":-58}`
- `GET /api/history` -> Raw `/history.csv` from LittleFS flash memory.
- `GET /api/settings` / `POST /api/settings` -> Read/write display settings (OLED mode, theme, carousel, page enable flags, font sizes). Persisted to `/config.json`.
- `GET /api/notes` / `POST /api/notes` -> Read/write TFT notes and the 5-item task list. Persisted to `/notes.json`.
- `POST /api/pomodoro` -> `{action:"start"|"stop", duration:<min>}` to control the focus timer.
- `GET /api/status` -> Current TFT page, pomodoro state + remaining seconds, active theme, RSSI. Used by the dashboard to sync.
- `POST /api/page` -> Force the TFT to jump to a specific page: `{page:<0-9>}`.

### 2.5. OLED Multi-Mode Renderer
The 128×64 OLED supports **10 selectable modes** (`oledMode` 0–9), each a branch in `renderOLED()`:

| Mode | Name | Backed by |
| :--: | :--- | :--- |
| 0 | Weather | DHT + BMP live sensors |
| 1 | Clock | NTP time |
| 2 | System | WiFi / heap / uptime |
| 3 | Pomodoro | running timer + `pomodoroDuration` |
| 4 | Forecast | pressure-trend heuristic |
| 5 | Temp Graph | `tftHistTemp[]` history ring buffer |
| 6 | Notes | `tftNotes` |
| 7 | Tasks | `tftTodos[]` |
| 8 | Analog | NTP time (trigonometric hands) |
| 9 | Binary | NTP time (BCD dot matrix) |

`oledMode` is constrained to `0–9` on both load and POST, persisted in `/config.json`, and selectable from the dashboard's *Display Settings → OLED Mode* button group.

## 3. Frontend Architecture (`data/`)

The frontend is a lightweight Single Page Application (SPA) designed to be highly responsive and visually stunning.
- **AJAX Fetching:** `app.js` polls `/api/data` every 2 s and `/api/status` every 3 s to keep the UI and the TFT/OLED state in sync without reloading the page.
- **Chart.js:** Fetches `/api/history`, parses the CSV, and draws an interactive line-graph of the outdoor (BMP) temperature over time.
- **4-Theme Engine:** A synchronized palette system — **Nord, Cyberpunk, Coder, Gruvbox** — applied to the web dashboard (CSS variables) and the physical TFT (RGB565 `TFTPalette` struct) simultaneously. Switching a theme POSTs to `/api/settings`, which re-renders the TFT with the new palette on the next frame.
- **TFT Remote & OLED Mode:** The dashboard can force the TFT to any page (`POST /api/page`) and select any of the 10 OLED modes via the settings button group.
