# Complete Hardware Integration & Circuit Diagram

This document contains the complete wiring schematic, breadboard connection map, power distribution architecture, and voltage level-shifting design for the **NEXUS Ocean Monitoring System**.

---

## 1. System Block Diagram

```mermaid
graph TD
    subgraph PowerSystem ["⚡ Power Distribution Architecture"]
        EXT_PWR["External 5V 2A Power Supply / Buck Converter"]
        USB_PWR["Arduino USB / 7-12V DC Jack"]
        GND_BUS["════ COMMON GROUND RAIL ════"]
        V5_BUS["════ ARDUINO 5V RAIL ════"]
        
        EXT_PWR -->|GND| GND_BUS
        USB_PWR -->|GND| GND_BUS
        USB_PWR -->|5V Out| V5_BUS
    end

    subgraph ArduinoSensors ["🌊 Submerged & Atmospheric Sensors"]
        TURB["Analog Turbidity Sensor"]
        DS18B20["DS18B20 Water Temp (Waterproof)"]
        BMP["BMP180 Barometric Pressure"]
        HMC["HMC5883L 3-Axis Compass"]
        RES_PU["4.7kΩ Pull-up Resistor"]
        
        V5_BUS -->|VCC| TURB
        V5_BUS -->|VCC| DS18B20
        V5_BUS -->|VCC| BMP
        V5_BUS -->|VCC| HMC
        V5_BUS -->|Pull-up 5V| RES_PU
        
        GND_BUS -->|GND| TURB
        GND_BUS -->|GND| DS18B20
        GND_BUS -->|GND| BMP
        GND_BUS -->|GND| HMC
    end

    subgraph Microcontroller ["🎛️ Primary Sensor Node: Arduino Uno / Nano"]
        ARDUINO["Arduino (ATmega328P)"]
        
        TURB -->|Signal Analog| ARDUINO["A0 (Analog In)"]
        DS18B20 -->|Data 1-Wire| ARDUINO["D5 (Digital In)"]
        RES_PU -.->|Bias| DS18B20
        
        BMP -->|SDA (I2C Data)| ARDUINO["A4 (SDA)"]
        BMP -->|SCL (I2C Clock)| ARDUINO["A5 (SCL)"]
        
        HMC -->|SDA (I2C Data)| ARDUINO["A4 (SDA)"]
        HMC -->|SCL (I2C Clock)| ARDUINO["A5 (SCL)"]
        
        ARDUINO["D1 (TX) - 5V Logic"] -->|UART 9600 Baud| VDIV
    end

    subgraph VoltageShifter ["⚡ Logic Level Converter (5V to 3.3V)"]
        VDIV["Voltage Divider:
        R1 = 1kΩ (in series)
        R2 = 2kΩ (to GND)
        Output = ~3.3V"]
        GND_BUS -->|Reference GND| VDIV
    end

    subgraph WirelessNode ["📷 Optical & Telemetry Node: ESP32-CAM"]
        ESP32["AI-Thinker ESP32-CAM (OV2640)"]
        CAP["100µF - 470µF Buffer Capacitor"]
        
        EXT_PWR -->|Dedicated 5V 2A| ESP32["5V Pin"]
        EXT_PWR -->|GND| ESP32["GND Pin"]
        CAP -.->|Filter Noise| ESP32
        
        VDIV -->|Safe 3.3V Serial| ESP32["U0RXD (GPIO 3)"]
    end

    subgraph RemoteClients ["💻 Monitoring Dashboard"]
        ESP32 -->|WiFi MJPEG Stream :81/stream| DASH["NEXUS Dashboard (index.html)"]
        ESP32 -->|WiFi JSON Telemetry :80/telemetry| DASH
        DASH -->|Night Flash Command :80/flash| ESP32
    end

    style PowerSystem fill:#0f172a,stroke:#00f0ff,stroke-width:2px,color:#f8fafc
    style ArduinoSensors fill:#020617,stroke:#10b981,stroke-width:1.5px,color:#f8fafc
    style Microcontroller fill:#0f172a,stroke:#00f0ff,stroke-width:2px,color:#f8fafc
    style VoltageShifter fill:#1e293b,stroke:#f59e0b,stroke-width:1.5px,color:#f8fafc
    style WirelessNode fill:#020617,stroke:#ff0055,stroke-width:2px,color:#f8fafc
    style RemoteClients fill:#0f172a,stroke:#00f0ff,stroke-width:2px,color:#f8fafc
```

---

## 2. Complete Pin-to-Pin Interconnection Table

| Component | Pin / Wire | Connects To | Signal / Function | Notes |
| :--- | :--- | :--- | :--- | :--- |
| **Common Ground** | All GNDs | **Breadboard GND Rail** | 0V Common Reference | **CRITICAL:** Tie Arduino GND & ESP32-CAM GND together! |
| **Common 5V** | Arduino 5V | **Breadboard 5V Rail** | Sensor Power | Powers Turbidity, DS18B20, BMP180, HMC5883L |
| **Turbidity Sensor** | VCC | Breadboard 5V Rail | Power (4.5V - 5V) | Analog clarity probe |
| | GND | Breadboard GND Rail | Ground | |
| | OUT / AOUT | **Arduino Pin A0** | Analog Output (0-5V) | ADC reading (0 = murky, ~700-800 = clear) |
| **DS18B20 Temp** | VCC (Red) | Breadboard 5V Rail | Power | Submersible stainless probe |
| | GND (Black) | Breadboard GND Rail | Ground | |
| | DATA (Yellow/White) | **Arduino Pin D5** | 1-Wire Digital Data | **Requires 4.7kΩ pull-up to 5V Rail** |
| **BMP180 (GY-68)** | VCC | Breadboard 5V Rail | Power | Barometer & Air Temp |
| | GND | Breadboard GND Rail | Ground | |
| | SDA | **Arduino Pin A4** | I2C Data Line | Shared with HMC5883L |
| | SCL | **Arduino Pin A5** | I2C Clock Line | Shared with HMC5883L |
| **HMC5883L (GY-271)**| VCC | Breadboard 5V Rail | Power | 3-Axis Magnetometer / Compass |
| | GND | Breadboard GND Rail | Ground | |
| | SDA | **Arduino Pin A4** | I2C Data Line | Shared with BMP180 |
| | SCL | **Arduino Pin A5** | I2C Clock Line | Shared with BMP180 |
| **Telemetry Bridge** | **Arduino Pin D1 (TX)** | 1kΩ Resistor (Terminal 1) | 5V UART Output | Do not connect directly to ESP32! |
| **Voltage Divider** | 1kΩ Resistor (Terminal 2) | **ESP32-CAM U0RXD (GPIO 3)** | 3.3V Stepped-down UART | Also connected to 2kΩ Resistor (Term 1) |
| | 2kΩ Resistor (Terminal 2) | Breadboard GND Rail | Ground Reference | \(V_{out} = 5V \times \frac{2k}{1k + 2k} = 3.33V\) |
| **ESP32-CAM** | **5V Pin** | **External 5V Power (+) (2A)** | Dedicated Board Power | **DO NOT power from Arduino 5V pin!** |
| | **GND Pin** | **Breadboard GND Rail** | Common Ground | Tie to External Power Supply (-) |
| | **GPIO 0** | GND (Only during flashing) | Boot Mode | Leave open / floating during normal run! |

---

## 3. Detailed Circuit Schematics

### A. Arduino to ESP32-CAM Voltage Divider (5V ➔ 3.3V)
The Arduino outputs 5V TTL logic on its TX pin (Pin D1). Connecting 5V directly to the ESP32-CAM (which is strictly a 3.3V chip) will damage its GPIO pins.

```
Arduino D1 (TX)  ──────[ 1kΩ Resistor ]──────┬──────> ESP32-CAM U0RXD (GPIO 3)
                                             │
                                      [ 2kΩ Resistor ]
                                             │
                                             ▼
                                     Common Ground (GND)
```

> **Calculation**:  
> $$V_{\text{ESP32\_RX}} = V_{\text{Arduino\_TX}} \times \frac{R_2}{R_1 + R_2} = 5.0\text{V} \times \frac{2000\Omega}{1000\Omega + 2000\Omega} = 3.33\text{V}$$  
> This safely matches the 3.3V logic requirement of the ESP32!

---

### B. DS18B20 OneWire Pull-up Connection
The DS18B20 1-Wire protocol requires an open-drain pull-up resistor (4.7kΩ) between Data and 5V:

```
5V Rail ───────┬────────────────────────────────┐
               │                                │
         [ 4.7kΩ Resistor ]                     │ (Red Wire)
               │                                │
Arduino D5 ────┴──────── (Yellow / White Wire) ───[ DS18B20 Probe ]
                                                │ (Black Wire)
GND Rail ───────────────────────────────────────┘
```

---

### C. I2C Bus Multiplexing (BMP180 + HMC5883L)
Both sensors communicate over I2C and have distinct 7-bit hardware addresses:
* **BMP180 I2C Address**: `0x77`
* **HMC5883L I2C Address**: `0x1E` (or `0x0D` for QMC5883L)

Because their addresses are unique, they connect in parallel directly to Arduino **A4 (SDA)** and **A5 (SCL)**:

```
Arduino A4 (SDA) ──────┬─────────────────────> BMP180 SDA
                       └─────────────────────> HMC5883L SDA

Arduino A5 (SCL) ──────┬─────────────────────> BMP180 SCL
                       └─────────────────────> HMC5883L SCL
```

---

## 4. Power Architecture & Brownout Prevention

```
[ 5V 2A External Power Supply ]
        │
        ├──(+) 5V ────────────┬──────> ESP32-CAM 5V Pin
        │                     │
        │                 [+] │
        │               [ 220µF - 470µF ] (Electrolytic Capacitor)
        │                 [-] │
        │                     │
        └──(-) GND ───────────┴──────> ESP32-CAM GND Pin ──┐
                                                           │
[ Arduino Uno / Nano USB ]                                 ├──> COMMON GROUND RAIL
        └── GND Pin ───────────────────────────────────────┘
```

> [!CAUTION]
> **Why the Arduino 5V pin cannot power the ESP32-CAM:**
> The Arduino's on-board linear voltage regulator can provide a maximum of ~400mA–500mA. The ESP32-CAM consumes up to **800mA–1500mA peak pulses** during Wi-Fi packet bursts and when firing the flash LED. Powering it from the Arduino will trigger an immediate brownout (`Brownout detector was triggered`), causing the ESP32 to restart continuously in a reboot loop. Always use a dedicated 5V 2A external source with a common ground.

---

## 5. Breadboard Step-by-Step Setup Checklist

1. **Setup Rails**:
   - Establish the upper breadboard rail as **5V (Arduino)** and **GND (Common)**.
   - Establish a separate rail for **External 5V Power** to supply the ESP32-CAM.
   - **Connect the GND of both rails together**.
2. **Mount Sensors**:
   - Plug in the BMP180 and HMC5883L modules. Run VCC to 5V and GND to GND.
   - Jump both SDA pins to Arduino **A4**.
   - Jump both SCL pins to Arduino **A5**.
3. **Wire DS18B20**:
   - Red wire to 5V, Black wire to GND, Yellow wire to Arduino **D5**.
   - Place a **4.7kΩ resistor** between 5V and D5.
4. **Wire Turbidity**:
   - VCC to 5V, GND to GND, OUT to Arduino **A0**.
5. **Wire Voltage Divider & ESP32-CAM**:
   - Arduino **D1 (TX)** to 1kΩ resistor.
   - Other end of 1kΩ resistor to ESP32-CAM **U0RXD (GPIO 3)** AND 2kΩ resistor.
   - Other end of 2kΩ resistor to Common GND.
   - ESP32-CAM **5V** to External 5V (+).
   - ESP32-CAM **GND** to Common GND (-).
   - Place a 100µF–470µF capacitor directly across ESP32-CAM 5V and GND.
6. **Flashing vs Running ESP32-CAM**:
   - To flash firmware: Connect **GPIO 0 to GND**, plug FTDI into USB, click Upload in Arduino IDE.
   - Once uploaded: **Disconnect GPIO 0 from GND** and press **RST** button.
   - When flashing the Arduino code: Briefly disconnect the wire on Arduino Pin D1 to avoid USB upload interference.
