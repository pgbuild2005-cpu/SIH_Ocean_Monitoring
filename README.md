# NEXUS Ocean Monitoring System

A comprehensive marine monitoring telemetry and analytics dashboard combining Arduino sensor nodes, ESP32-CAM optical surveillance, and an interactive real-time telemetry dashboard.

---

## 1. Hardware Wiring Guide

### Power Distribution
* **Common Ground**: Connect Arduino GND, External 5V Power GND, and ESP32-CAM GND to a shared ground rail.
* **Arduino Power**: 5V via USB or 7-12V barrel jack / VIN.
* **ESP32-CAM Power**: Dedicated external 5V (2A minimum) connected to ESP32-CAM **5V** and **GND**. *(Note: Do NOT power the ESP32-CAM from the Arduino 5V pin; camera and WiFi transmission require up to 1.5A peak pulses).*

### Sensors to Arduino Pinout:
* **Turbidity Sensor (Water Clarity):** VCC ➔ 5V | GND ➔ GND | OUT ➔ Arduino **A0**
* **DS18B20 (Water Temperature):** VCC ➔ 5V | GND ➔ GND | Data ➔ Arduino **D5** *(Requires 4.7kΩ pull-up resistor between Data and 5V)*
* **BMP180 (Atmospheric Temp & Pressure):** VCC ➔ 5V | GND ➔ GND | SDA ➔ Arduino **A4** | SCL ➔ Arduino **A5**
* **HMC5883L (Magnetometer / Compass):** VCC ➔ 5V | GND ➔ GND | SDA ➔ Arduino **A4** | SCL ➔ Arduino **A5**

### Arduino to ESP32-CAM Telemetry Bridge:
* **Arduino TX (Pin D1)** ➔ Voltage Divider (Arduino 5V logic to ESP32 3.3V logic) ➔ **ESP32-CAM U0RXD (GPIO 3)**
  * *Voltage Divider Circuit*:
    * Arduino D1 ➔ 1kΩ resistor ➔ Node A
    * Node A ➔ ESP32-CAM U0RXD (GPIO 3)
    * Node A ➔ 2kΩ resistor ➔ GND
* **Common GND**: Essential for UART signal reference.

---

## 2. Directory Structure

```
├── README.md                                    # System wiring, architecture & usage instructions
├── index.html                                   # NEXUS real-time glassmorphic telemetry & video dashboard
├── arduino/
│   └── ocean_sensor_node/
│       └── ocean_sensor_node.ino                # Sensor polling firmware (Turbidity, Temp, Pressure, Compass)
└── esp32_cam/
    └── ocean_cam_node/
        └── ocean_cam_node.ino                   # AI-Thinker OV2640 MJPEG streaming & telemetry gateway
```

---

## 3. ESP32-CAM Firmware Guide

### Flashing with FTDI / USB-to-UART Adapter:
1. Connect FTDI to ESP32-CAM:
   * FTDI VCC (5V) ➔ ESP32-CAM 5V
   * FTDI GND ➔ ESP32-CAM GND
   * FTDI TX ➔ ESP32-CAM U0RXD (GPIO 3)
   * FTDI RX ➔ ESP32-CAM U0TXD (GPIO 1)
   * **Bridge GPIO 0 to GND** (enters flash bootloader mode)
2. In Arduino IDE:
   * Board: `AI Thinker ESP32-CAM`
   * CPU Frequency: `240MHz (WiFi/BT)`
   * Flash Frequency: `80MHz`
   * Flash Mode: `QIO`
   * Partition Scheme: `Huge APP (3MB No OTA/1MB SPIFFS)`
   * Upload Speed: `115200`
3. Click **Upload**, wait until completion, then **disconnect GPIO 0 from GND** and press the on-board **RST** button.

### Network Modes:
* **Station Mode**: Connects to your configured local WiFi.
* **Field Access Point Mode**: If WiFi connection fails or is unavailable in marine environments, the node automatically broadcasts an AP:
  * **SSID**: `NEXUS-Ocean-Cam`
  * **Password**: `nexus12345`
  * **Gateway IP**: `192.168.4.1`

### HTTP Endpoints:
| Endpoint | Port | Description |
| :--- | :---: | :--- |
| `/stream` | `81` | Real-time MJPEG video stream (low latency) |
| `/capture` | `80` | High-resolution still frame capture |
| `/telemetry` | `80` | Live JSON sensor telemetry (`{ waterTemp, turbidity, ... }`) with CORS enabled |
| `/flash?state=on\|off` | `80` | Toggles high-power LED flash for night or underwater visibility |
| `/` | `80` | Built-in node status portal |
