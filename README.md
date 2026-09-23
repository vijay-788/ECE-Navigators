# EcoStay IoT
### Intelligent Edge-Driven Energy Optimizer for Budget Hotels

**Team ECE Navigators** | Smart India Hackathon 2026 | Problem Statement ID: **26221**
Theme: Travel & Tourism | Category: Hardware | Organization: AICTE

---

## 📌 Problem

Budget and mid-tier Indian hotels lose an estimated **35% of energy** to appliances (ACs, geysers) left running in empty rooms. Existing fixes fall short:

- **PIR motion sensors** falsely cut power when guests are asleep or sitting still.
- **Mechanical keycard slots** are easily tricked with a dummy card or paper cutout.
- **Hotel staff** have no real-time, room-by-room visibility into energy use.

## 💡 Solution

EcoStay IoT is a low-cost, retrofit hardware module that slips behind an existing modular switchboard — no rewiring or wall-breaking needed. It tracks **absolute body heat**, not motion, so power is never cut while a guest is sleeping or reading, and it can't be tricked by a fake keycard.

| Step | What happens |
|---|---|
| 1. Entry | Guest swipes RFID → RC522 authenticates → main relay energizes the room |
| 2. Thermal scan | AMG8833 continuously samples an 8×8 infrared grid for a human heat signature |
| 3. Edge logic | ESP32 compares readings against a DHT22-calibrated ambient baseline |
| 4. Load shedding | If 0 occupants are detected for a continuous 5-minute window, relays cut power to AC/Geyser only (low-power sockets stay live) |
| 5. Telemetry | Live occupancy, temperature, and relay status stream to a dashboard over Wi-Fi/MQTT |

## 🧩 Hardware

| Component | Role | Interface |
|---|---|---|
| ESP32 DevKit V1 | Central processing brain | — |
| AMG8833 Thermal Grid-EYE | 8×8 body-heat occupancy sensor (no camera/lens) | I2C |
| RC522 RFID Module | Guest check-in / door access | SPI |
| DHT22 | Ambient temperature calibration | 1-Wire |
| 30A Optocoupler-Isolated Relay ×2 | Cuts AC & Geyser power independently | GPIO |

### Pinout

| Module | Pin | ESP32 GPIO |
|---|---|---|
| AMG8833 | SDA / SCL | 21 / 22 |
| RC522 | SDA(SS) / SCK / MOSI / MISO / RST | 5 / 18 / 23 / 19 / 4 |
| DHT22 | DATA (+10kΩ pull-up to 3.3V) | 15 |
| Relay | IN1 (AC) / IN2 (Geyser) | 26 / 27 |

Full wiring diagrams are in [`docs/`](docs/).

## 💰 Bill of Materials (~₹1,600 per room)

| Item | Cost |
|---|---|
| ESP32 Dev Board | ₹350 |
| AMG8833 Thermal Sensor | ₹750 |
| RC522 RFID Module | ₹150 |
| 30A Relay Module | ₹250 |
| Enclosure & wiring | ₹100 |

Prototype build cost (incl. breadboard, tools): ₹2,500–2,800. Deployable per-room kit: **₹1,600**. Estimated ROI: **under 2–4 months** at ~₹10,000/month savings across 10 rooms.

## 🔧 Firmware

See [`firmware/ecostay_iot.ino`](firmware/ecostay_iot.ino).

**Required Arduino IDE libraries:**
- `Adafruit AMG88xx` (Adafruit)
- `MFRC522` (GitHubCommunity)
- `DHT sensor library` (Adafruit) + Adafruit Unified Sensor dependency
- `PubSubClient` (Nick O'Leary)

**Before flashing**, update in the sketch:
```cpp
const char* ssid        = "YOUR_HOTEL_WIFI_SSID";
const char* password    = "YOUR_WIFI_PASSWORD";
const char* mqtt_server = "YOUR_MQTT_BROKER_IP_OR_HOST";
```

Board: **ESP32 Dev Module**, 115200 baud on the Serial Monitor.

## ✅ Test Sequence

1. Power on — relays should be OFF at boot.
2. Swipe RFID card → relays energize (bulb/fan/AC ON).
3. Hold a hand over the AMG8833 → Serial Monitor confirms thermal detection, power stays active.
4. Remove hand, wait 5 minutes → relays open, load powers OFF automatically.
5. Confirm MQTT payloads are arriving at the dashboard every ~10s.

## 🌟 Why It's Different

- **Motionless-accurate** — tracks body heat, not movement, so it never falsely cuts power on a sleeping guest.
- **100% privacy-safe** — the AMG8833 has no optical lens or camera; it only outputs 64 raw temperature values, which can't reconstruct a face or image.
- **Tamper-proof** — cross-checks RFID state against live thermal data, so fake/dummy keycards can't hold power on indefinitely.
- **Affordable** — ~₹1,600/room vs. ₹12,000–₹60,000 for commercial BMS systems.
- **Fails safe** — core logic runs entirely on-device; if Wi-Fi/MQTT drops, room automation keeps working locally.

## 📁 Suggested Repo Structure

```
ECE-Navigators/
├── README.md
├── firmware/
│   └── ecostay_iot.ino
├── docs/
│   ├── circuit_diagram.png
│   ├── breadboard_wiring.png
│   ├── system_architecture.png
│   └── BOM.png
└── presentation/
    └── SIH_Presentation.pdf
```

## 👥 Team

**ECE Navigators** — Team ID 147383 — SIH 2026, PS ID 26221

---
*Built for Smart India Hackathon 2026 — Travel & Tourism theme.*
