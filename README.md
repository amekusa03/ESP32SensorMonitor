# ESP32 1.9" LCD Smart Clock & Environment Monitor

English | [日本語 (README.JP.md)](README.JP.md)

A **Wi-Fi NTP-synchronized digital clock and indoor environment monitor** designed for ESP32 boards equipped with a 1.9-inch IPS TFT LCD (ST7789 driver).  
Powered by ESP-IDF's native `esp_lcd` driver and internal DMA frame buffering to deliver flicker-free, smooth, and high-performance UI rendering.

---

## 🌟 Features

- **NTP Time Synchronization**: Automatically fetches and synchronizes accurate date, day of week, and time over Wi-Fi (JST / UTC+9 default).
- **SwitchBot + Sensor Web API Integration**:
  - Periodically fetches environmental metrics via mDNS from local network sensor nodes (`http://esp32-switchbot.local/api/sensor`).
  - **Room Temperature (TEMP)**: Prominently displayed in extra-large scale font with dynamic color mapping based on temperature range.
  - **Ambient Illuminance (BRIGHTNESS / LUX)**: Real-time illuminance monitor in the lower card section.
- **Hardware PWM Auto-Dimming**:
  - Automatically adjusts LCD backlight brightness smoothly based on ambient lux readings for comfortable day/night viewing.
- **Modern & High-Contrast UI**:
  - Sleek dark theme layout optimized for 320×170 landscape resolution with a unified bitmap font.

---

## 🖥 Screen Layout

```text
+-------------------------------------------------------------+
|  2026/09/06 (SUNDAY)                        12:34           | <- Date / Day & Clock
|-------------------------------------------------------------|
|  +-------------------------------------------------------+  |
|  | ROOM TEMP                                             |  |
|  |                                                       |  |
|  |                 2 5 . 3   C                           |  | <- Main: Extra-large temperature
|  |                                                       |  |
|  +-------------------------------------------------------+  |
|  +-------------------------------------------------------+  |
|  | BRIGHTNESS                       28.5 lx              |  | <- Ambient Lux Monitor
|  +-------------------------------------------------------+  |
+-------------------------------------------------------------+
```

---

## 🔧 Hardware Specifications & Pinout

- **MCU**: ESP32 (Xtensa Dual-Core 240MHz, 16MB Flash, CH340 USB-Serial)
- **Display**: 1.9-inch IPS TFT LCD (Resolution: 170×320, Driver: ST7789)

### LCD Pin Mapping (SPI)

| LCD Pin | ESP32 GPIO | Description |
| :--- | :--- | :--- |
| **VCC** | 3.3V | Power Supply |
| **GND** | GND | Ground |
| **DC** | **GPIO 2** | Data / Command Selection |
| **RST** | **GPIO 4** | Reset |
| **CS** | **GPIO 15** | Chip Select |
| **SCLK** | **GPIO 18** | SPI Clock |
| **MOSI** | **GPIO 23** | SPI Data Output |
| **BLK** | **GPIO 32** | Backlight Control (LEDC PWM) |

---

## 📁 Directory Structure

```text
ESP32SensorMonitor/
├── CMakeLists.txt              # Project root CMake configuration
├── sdkconfig.defaults          # Default configuration (16MB Flash, etc.)
├── .gitignore                  # Git ignore rules
├── README.md                   # English documentation
├── README.JP.md                # Japanese documentation
└── main/
    ├── CMakeLists.txt          # Component dependencies
    ├── idf_component.yml       # Component dependencies (mDNS, etc.)
    ├── main.c                  # Main application source code
    ├── secrets.example.h       # Wi-Fi credentials template
    └── secrets.h               # Wi-Fi credentials (excluded from Git)
```

---

## 🚀 Getting Started

### 1. Prerequisites
- **ESP-IDF v5.x** (v5.4 recommended)

### 2. Set Up the Environment
```bash
source $HOME/esp/esp-idf/export.sh
```

### 3. Configure Wi-Fi Credentials
Copy `secrets.example.h` to `secrets.h` and provide your Wi-Fi credentials:

```bash
cp main/secrets.example.h main/secrets.h
```

Edit `main/secrets.h`:
```c
#pragma once
#define WIFI_SSID      "YOUR_WIFI_SSID"
#define WIFI_PASS      "YOUR_WIFI_PASSWORD"
```

### 4. Set Target (First time only)
```bash
idf.py set-target esp32
```

### 5. Build
```bash
idf.py build
```

### 6. Flash and Monitor
```bash
idf.py -p /dev/ttyUSB0 flash monitor
```

*(Press `Ctrl + ]` to exit the serial monitor)*

---

## 📜 License

MIT License
