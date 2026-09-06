# NEXUS Ocean Monitoring System

A comprehensive marine monitoring telemetry and analytics dashboard combining Arduino sensor nodes, ESP32-CAM optical surveillance, and an interactive real-time telemetry dashboard.

---

## 1. Hardware Wiring Guide

* **Arduino GND** ➔ Common Ground Rail (Breadboard)
* **Arduino 5V** ➔ Common 5V Rail (Breadboard)
* **External 5V Power Supply** ➔ ESP32-CAM 5V (GND connected to Common GND)

### Sensors & Pinout:
* **Turbidity Sensor (Water Clarity):** VCC ➔ 5V | GND ➔ GND | OUT ➔ Arduino **A0**
* **DS18B20 (Water Temperature):** VCC ➔ 5V | GND ➔ GND | Data ➔ Arduino **D5** *(Requires 4.7kΩ pull-up resistor between Data and 5V)*
* **BMP180 (Atmospheric Temp & Pressure):** VCC ➔ 5V | GND ➔ GND | SDA ➔ Arduino **A4** | SCL ➔ Arduino **A5**
* **HMC5883L (Magnetometer / Compass):** VCC ➔ 5V | GND ➔ GND | SDA ➔ Arduino **A4** | SCL ➔ Arduino **A5**

---

## 2. Directory Structure

- `arduino/ocean_sensor_node/ocean_sensor_node.ino`: Arduino firmware reading turbidity, temperature, barometer, and heading.
- `index.html`: Cyberpunk/Glassmorphic real-time telemetry dashboard with telemetry views and analytics tab.

---

## 3. Communication & Integration Architecture Options

1. **Direct Web Serial Bridge**:
   Connect the Arduino directly to the laptop browser via USB using the Web Serial API for instant live sensor telemetry without intermediate servers.
2. **ESP32-CAM WiFi / Firebase Relay**:
   Arduino streams sensor packets over UART (TX/RX) to the ESP32-CAM. The ESP32-CAM publishes telemetry to Firebase Realtime Database and streams live video feed to the dashboard.
3. **Python Telemetry Bridge**:
   A lightweight Python service reading Arduino over Serial (`pyserial`) and broadcasting to WebSockets / Firebase / Local REST endpoints.
