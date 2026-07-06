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
│   ├── style.css               # CSS featuring Dark Mode, Ocean, and Sunset themes
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
4. **Network & Time:** Connects to WiFi, syncs with an NTP server (`pool.ntp.org`), and establishes the local timezone offset.
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
- `GET /api/data` -> Returns live JSON: `{"tempDHT": 25.5, "humDHT": 60, "tempBME": 26.0, "uptime": 120, "freeHeap": 230000}`
- `GET /api/history` -> Returns the raw `/history.csv` file from LittleFS flash memory.

## 3. Frontend Architecture (`data/`)

The frontend is a lightweight Single Page Application (SPA) designed to be highly responsive and visually stunning.
- **AJAX Fetching:** `app.js` polls `/api/data` every 2 seconds to update the UI instantly without reloading the page.
- **Chart.js:** Every 10 minutes, the app fetches `/api/history`, parses the CSV, and draws an interactive, scaling line-graph of the indoor vs. outdoor temperatures.
- **Dynamic CSS:** The UI calculates min/max temperatures locally and features a theme selector. The background gradient dynamically tints "warmer" if the outdoor temperature exceeds 28°C.
