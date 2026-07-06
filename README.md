# 🌌 chaos STATION — ESP32 Smart Weather Command Hub

<div align="center">
  <img src="images/e1.jpg" width="48%" alt="Hardware Device"/>
  <img src="images/w1.jpg" width="48%" alt="Web Dashboard"/>
</div>

<div align="center">
  <img src="images/w2.jpg" width="32%" alt="Theme 1"/>
  <img src="images/w3.jpg" width="32%" alt="Theme 2"/>
  <img src="images/e2.jpg" width="32%" alt="TFT Display"/>
</div>

Welcome to **chaos STATION**, a modern, multi-display, multi-theme Smart Weather Station built on the ESP32. This project combines dual hardware displays (TFT + OLED) with a sophisticated responsive web dashboard to provide a unified Command & Control Hub.

---

## ✨ Features

- 🖥️ **Dual Hardware Displays**: 
  - **128x160 TFT LCD (ST7735)**: Rich graphical UI featuring 10 dynamic pages (Dashboard, Temp Dial, Atmosphere Rings, Notes, Tasks, Forecast, Clock, System Terminal, Pomodoro Timer, and Hardware Health).
  - **128x64 OLED (SSD1306)**: At-a-glance monochrome readout for time, weather basics, and system info.
- 🌐 **Advanced Web Dashboard**: A glassmorphic, responsive web interface hosted directly on the ESP32 (via LittleFS). Control the TFT remotely, view live sensor gauges, and monitor historical trends via Chart.js.
- 🎨 **4-Theme Synchronization Engine**: Swap themes on the web dashboard and watch the physical TFT display update in real-time. Choose between:
  - ❄️ **Nord**: A cool, arctic palette with soft pastels.
  - 🌆 **Cyberpunk**: High-contrast neon on dark navy, complete with CSS scanline overlays.
  - 💻 **Coder**: Terminal green-on-black aesthetics with blinking cursors.
  - 🍂 **Gruvbox**: A warm, retro amber and earthy palette.
- ⏱️ **Focus / Pomodoro Timer**: Integrated timer synchronized between the web UI and the physical device.
- 🕹️ **Remote Page Control**: Force the hardware TFT to jump to specific pages instantly from the web interface.

---

## 🛠️ Hardware Requirements

| Component | Description |
| :--- | :--- |
| **Microcontroller** | ESP32 (e.g., NodeMCU-32S or standard WROOM-32 dev board) |
| **Primary Display** | 1.8" SPI TFT LCD (ST7735, 128x160) |
| **Secondary Display** | 0.96" I2C OLED (SSD1306, 128x64) |
| **Climate Sensor** | DHT11 (Temperature & Humidity) |
| **Pressure Sensor** | BMP180/085 (Barometric Pressure & Temperature) |

### 🔌 Circuit Wiring Diagram

```mermaid
graph TD
    %% Main Microcontroller
    ESP32[ESP32 NodeMCU]

    %% Components
    OLED[0.96" OLED I2C]
    TFT[1.8" TFT SPI]
    DHT[DHT11 Sensor]
    BMP[BMP180 Sensor]

    %% I2C Connections (Shared)
    ESP32 -- "GPIO 21 (SDA)" --> OLED
    ESP32 -- "GPIO 22 (SCL)" --> OLED
    ESP32 -- "GPIO 21 (SDA)" --> BMP
    ESP32 -- "GPIO 22 (SCL)" --> BMP

    %% SPI Connections (Dedicated TFT)
    ESP32 -- "GPIO 13 (MOSI)" --> TFT
    ESP32 -- "GPIO 14 (SCK)" --> TFT
    ESP32 -- "GPIO 15 (CS)" --> TFT
    ESP32 -- "GPIO 16 (DC)" --> TFT
    ESP32 -- "GPIO 17 (RST)" --> TFT

    %% Digital Connections
    ESP32 -- "GPIO 4 (DATA)" --> DHT

    %% Power distribution
    3V3((3.3V Power))
    GND((GND))

    3V3 --> OLED
    3V3 --> TFT
    3V3 --> DHT
    3V3 --> BMP
    
    GND --> OLED
    GND --> TFT
    GND --> DHT
    GND --> BMP
```

*See [README_PIN_DIAGRAM.md](README_PIN_DIAGRAM.md) for deeper wiring instructions and troubleshooting.*

---

## 🏗️ System Architecture

The firmware utilizes a robust `state` structure and a semantic palette system. Communication between the Web UI and the ESP32 happens via REST API endpoints (`/api/data`, `/api/settings`, `/api/status`, etc.). 

### Client-Server Flow Diagram

```mermaid
sequenceDiagram
    participant Web as Web Dashboard (Browser)
    participant API as ESP32 REST API
    participant FS as LittleFS (Storage)
    participant HW as Hardware (TFT/OLED)
    
    Web->>API: GET /api/data (Polling)
    API-->>Web: JSON (Sensors, RSSI, Heap)
    Web->>Web: Update Chart.js & CSS Gauges
    
    Web->>API: POST /api/settings (Theme change)
    API->>FS: Save to config.json
    API->>HW: Trigger TFT Palette Swap
    API-->>Web: 200 OK
    
    Web->>API: POST /api/page (Jump to page 5)
    API->>HW: Force TFT to Page 5
    API-->>Web: 200 OK
```

### Component Architecture

```mermaid
graph TD
    subgraph ESP32 Firmware
        API[AsyncWebServer API]
        Sensors[Sensor Tasks: DHT/BMP]
        Config[config.json Manager]
        UI[TFT/OLED Render Engine]
    end
    
    subgraph Web App
        HTML[index.html]
        CSS[style.css - 4 Themes]
        JS[app.js - State Sync]
    end
    
    Sensors --> API
    Config --> UI
    JS -- REST API JSON --> API
    API --> UI
    API -- Reads/Writes --> Config
    HTML --> CSS
    HTML --> JS
```

*See [README_ARCHITECTURE.md](README_ARCHITECTURE.md) for a deeper dive into the firmware and web architecture.*

---

## 🚀 Quickstart

This project is built using **PlatformIO** and includes a one-click deployment script.

1. **Clone the repository**:
   ```bash
   git clone https://github.com/yourusername/chaos-STATION.git
   cd chaos-STATION
   ```

2. **Configure WiFi**:
   Open `src/main.cpp` and update the WiFi credentials to match your local network:
   ```cpp
   const char* WIFI_SSID     = "Your_SSID";
   const char* WIFI_PASSWORD = "Your_Password";
   ```

3. **Deploy the System**:
   Run the automated deployment script to build the firmware and upload the LittleFS dashboard:
   ```bash
   ./deploy.sh
   ```
   *(If you are on Windows, you can manually run `pio run -t upload` and `pio run -t uploadfs` instead).*

4. **Access the Dashboard**:
   Once the ESP32 boots and connects to WiFi, you can access the dashboard by navigating to the IP address displayed on the OLED screen, or by visiting:
   ```text
   http://esp2display.local
   ```

---

## 📝 Notes & Limitations
- Ensure you have the required libraries installed (PlatformIO will handle this automatically via `platformio.ini`).
- The `app.js` and `style.css` are served directly from the ESP32's SPIFFS/LittleFS partition. Ensure you always run `uploadfs` if you make changes to the `data/` directory.

## 📜 License
Distributed under the MIT License. See `LICENSE` for more information.
