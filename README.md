# ESP32 Home Automation — Alexa 2-Channel Relay + PIR + 3D Enclosure

A cloud-synced, low-power **ESP32 smart home controller** powered by [Sinric Pro](https://sinric.pro/) with **Amazon Alexa voice control**, **HC-SR501 PIR smart occupancy automation**, optional **physical pushbuttons** with two-way state sync, and a custom **support-free 3D-printable FreeCAD enclosure**.

---

## Features

- **Amazon Alexa Voice Control (Sinric Pro):** Control both relay channels from any Echo speaker or the Alexa mobile app—at home or away on mobile data.
- **Smart PIR Occupancy + Voice Override:**
  - Walking into the room automatically turns **Relay 1 ON** and syncs the state to the Alexa app.
  - Automatically turns **Relay 1 OFF** after 60 seconds of no motion (configurable in `src/main.cpp`).
  - **Voice / Manual Override:** Turning Relay 1 ON via Alexa (or Button 1) disables the auto-OFF timer so the light stays ON even if you sit motionless. Turning it OFF manually starts a 15-second cooldown so the PIR doesn't immediately re-trigger while you leave the room.
- **Low-Power Mode (Always Alexa-Ready):**
  - Bluetooth disabled (`btStop()`)
  - CPU clock lowered from 240 MHz → **80 MHz**
  - Wi-Fi Modem Sleep (`WIFI_PS_MIN_MODEM`)
  - FreeRTOS idle yield (`delay(10)`) in `loop()`
- **Safe Boot:** Relay pins are initialized OFF before configuring as `OUTPUT` to prevent startup chatter on Active-LOW relay boards.
- **Custom 3D-Printable Enclosure:** Parametric FreeCAD design + ready-to-slice STLs with exact clearances for upside-down ESP32 mounting, 40 × 50 mm 2-channel relay, and 25 × 32 mm HC-SR501 PIR.

---

## Repository Structure

```text
home-automation-esp32/
├── alexa-relay/                  # PlatformIO ESP32 firmware
│   ├── platformio.ini            # Board, framework, and SinricPro dependency
│   ├── include/
│   │   └── secrets.h.example     # Template for Wi-Fi & Sinric Pro credentials
│   └── src/
│       └── main.cpp              # Alexa + PIR + Pushbuttons + Low-Power logic
└── enclosure/                    # 3D-printable 2-piece enclosure
    ├── alexa_pir_enclosure.FCStd # FreeCAD 1.1 parametric source file
    ├── EnclosureBase.stl         # Print-ready base tray (no supports)
    └── EnclosureLid.stl          # Print-ready lid, flipped flat (no supports)
```

---

## Hardware Required

- **ESP32 DevKit** (classic 4 MB ESP32, `52 × 28 mm`, `47.0 × 23.5 mm` mounting holes)
- **2-Channel 5V Active-LOW Opto-Isolated Relay Module** (`JQC3F-05VDC-C`, `40 × 50 mm`, `34.0 × 45.0 mm` mounting holes)
- **HC-SR501 PIR Motion Sensor** (`25 × 32 mm`, `28.0 mm` mounting holes, `24 × 24 mm` dome base)
- *(Optional)* 2× tactile pushbuttons for local manual toggle
- **Screws (direct self-tapping into PLA/PETG, no brass inserts needed):**
  - 4× `M2.5 × 8 mm` or `10 mm` Pan Head (lid corners)
  - 4× `M2.5 × 6 mm` Pan Head (relay standoffs)
  - 4× `M2 × 6 mm` Pan Head (ESP32 standoffs)
  - 2× `M2 × 6 mm` Pan Head (PIR sensor on lid)

---

## Wiring Diagram

> **Electrical Safety:** Always bench-test with **no AC mains power** connected to the relay screw terminals (`COM` / `NO` / `NC`). Only connect AC loads once fully verified and safely enclosed.

### ESP32 ↔ 2-Channel Relay Module
| ESP32 Pin | Relay Pin | Notes |
| :--- | :--- | :--- |
| `5V` (`VIN`) | `VCC` | Keep `RY-VCC`/`VCC` jumper ON *(or remove jumper: `RY-VCC` → `5V`, `VCC` → `3.3V`)* |
| `GND` | `GND` | Common ground |
| `GPIO 26` | `IN1` | Relay 1 (Alexa + PIR Auto-Light) |
| `GPIO 27` | `IN2` | Relay 2 (Alexa) |

### ESP32 ↔ HC-SR501 PIR Sensor
| ESP32 Pin | HC-SR501 Pin | Notes |
| :--- | :--- | :--- |
| `5V` (`VIN`) | `VCC` | Share `5V` rail with Relay |
| `GND` | `GND` | Common ground |
| `GPIO 33` | `OUT` | 3.3V digital output (ESP32-safe) |

**Recommended HC-SR501 settings:**
- Set the trigger jumper to **`H`** (*Repeatable Trigger*).
- Turn the **Time Delay** potentiometer all the way **counter-clockwise** (minimum ~3–5s) so the ESP32 manages the timeout cleanly.

### Optional Pushbuttons
| ESP32 Pin | Component | Notes |
| :--- | :--- | :--- |
| `GPIO 18` | Pushbutton 1 → `GND` | Toggles Relay 1 & syncs to Alexa |
| `GPIO 19` | Pushbutton 2 → `GND` | Toggles Relay 2 & syncs to Alexa |
| `GPIO 2` | Onboard Blue LED | Lights up whenever either relay is ON |

---

## Setup & Flashing

### 1. Configure Sinric Pro & Alexa
1. Create a free account at [portal.sinric.pro](https://portal.sinric.pro/) (free tier supports up to 3 devices).
2. Add **2 Switch devices** under **Devices → Add Device** (e.g., *Lamp* and *Fan*).
3. Copy your **APP KEY**, **APP SECRET** (from **Credentials**), and both **Device IDs**.
4. In the **Amazon Alexa app**, enable the **Sinric Pro** skill and link your account.

### 2. Add Local Credentials
Copy the template file and fill in your credentials:
```powershell
Copy-Item alexa-relay\include\secrets.h.example alexa-relay\include\secrets.h
```
Edit `alexa-relay/include/secrets.h` with your 2.4 GHz Wi-Fi SSID/password, Sinric Pro App Key/Secret, and Device IDs.

### 3. Build & Flash (PlatformIO)
Update `upload_port` in `alexa-relay/platformio.ini` if your ESP32 is not on `COM5`, then run:
```powershell
python -m platformio run -d alexa-relay -t upload
```

### 4. Monitor Serial Output
```powershell
python -m serial.tools.miniterm COM5 115200
```
You should see:
```text
=== ESP32 Sinric Pro 2-Channel Relay + PIR + Low Power ===
Wi-Fi connected! IP: 192.168.x.x (Modem Sleep enabled, CPU 80 MHz)
Connected to SinricPro! Ready for Alexa + PIR.
```

---

## 3D-Printable Enclosure (`enclosure/`)

- **Outer size:** `110 × 68 × 40.4 mm` (`2.4 mm` walls, `5 mm` corner fillets)
- **ESP32 orientation:** Mounted **upside-down** on `5.5 mm` standoffs so Dupont jumper pins face upward toward the lid, with a lowered micro-USB cutout (`Z = 3–12 mm`) on the left wall.
- **PIR cutout:** Exact `24 × 24 mm` square opening in the lid with 2× M2 underside bosses (`28 mm` spacing).
- **Recommended slicer settings:** PLA or PETG, `0.2 mm` layer height, 3 walls, 20% infill, **no supports required**.
