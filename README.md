# chaos STATION — ESP32 Smart Weather Command Hub

Welcome to **chaos STATION**, a modern, multi-display, multi-theme Smart Weather Station built on the ESP32. This project combines dual hardware displays (TFT + OLED) with a sophisticated responsive web dashboard to provide a unified Command & Control Hub.

## ✨ Features

- **Dual Hardware Displays**: 
  - **128x160 TFT LCD (ST7735)**: Rich graphical UI featuring 10 dynamic pages (Dashboard, Temp Dial, Atmosphere Rings, Notes, Tasks, Forecast, Clock, System Terminal, Pomodoro Timer, and Hardware Health).
  - **128x64 OLED (SSD1306)**: At-a-glance monochrome readout for time, weather basics, and system info.
- **Advanced Web Dashboard**: A glassmorphic, responsive web interface hosted directly on the ESP32 (via LittleFS). Control the TFT remotely, view live sensor gauges, and monitor historical trends via Chart.js.
- **4-Theme Synchronization Engine**: Swap themes on the web dashboard and watch the physical TFT display update in real-time. Choose between:
  - ❄️ **Nord**: A cool, arctic palette with soft pastels.
  - 🌆 **Cyberpunk**: High-contrast neon on dark navy, complete with CSS scanline overlays.
  - 💻 **Coder**: Terminal green-on-black aesthetics with blinking cursors.
  - 🍂 **Gruvbox**: A warm, retro amber and earthy palette.
- **Focus / Pomodoro Timer**: Integrated timer synchronized between the web UI and the physical device.
- **Remote Page Control**: Force the hardware TFT to jump to specific pages instantly from the web interface.

## 🛠️ Hardware Requirements

- **Microcontroller**: ESP32 (e.g., NodeMCU-32S or standard ESP32-WROOM-32 dev board)
- **Displays**: 
  - 1.8" SPI TFT LCD (ST7735, 128x160)
  - 0.96" I2C OLED (SSD1306, 128x64)
- **Sensors**:
  - DHT11 (Temperature & Humidity)
  - BMP180/085 (Barometric Pressure & Temperature)

*See [README_PIN_DIAGRAM.md](README_PIN_DIAGRAM.md) for detailed wiring instructions.*

## 🏗️ Architecture

The firmware utilizes a robust `state` structure and a semantic palette system. Communication between the Web UI and the ESP32 happens via REST API endpoints (`/api/data`, `/api/settings`, `/api/status`, etc.). 

*See [README_ARCHITECTURE.md](README_ARCHITECTURE.md) for a deep dive into the firmware and web architecture.*

## 🚀 Quickstart

This project is built using **PlatformIO**. 

1. **Clone the repository**:
   ```bash
   git clone https://github.com/yourusername/esp-2display-weather.git
   cd esp-2display-weather
   ```

2. **Configure WiFi**:
   Open `src/main.cpp` and update the WiFi credentials to match your local network:
   ```cpp
   const char* WIFI_SSID     = "Your_SSID";
   const char* WIFI_PASSWORD = "Your_Password";
   ```

3. **Build and Upload Firmware**:
   ```bash
   pio run -t upload
   ```

4. **Upload the Web Filesystem (LittleFS)**:
   ```bash
   pio run -t uploadfs
   ```

5. **Access the Dashboard**:
   Once the ESP32 boots and connects to WiFi, you can access the dashboard by navigating to the IP address displayed on the OLED screen, or by visiting:
   ```text
   http://esp2display.local
   ```

## 📝 Notes & Limitations
- Ensure you have the required libraries installed (PlatformIO will handle this automatically via `platformio.ini`).
- The `app.js` and `style.css` are served directly from the ESP32's SPIFFS/LittleFS partition. Ensure you always run `uploadfs` if you make changes to the `data/` directory.

## 📜 License
Distributed under the MIT License. See `LICENSE` for more information.
