# Aeris AAIR Components and Available Interfaces

This document summarizes the MCU and peripheral components currently used by the firmware (Photon target), with additional teardown notes for reference.  
Date: 2026-02-22

## 1. MCU and System Layer

### 1.1 Main MCU
- Model / platform: `Particle Photon` (firmware compile target: `photon`)
- Firmware runtime mode:
  - `SYSTEM_MODE(SEMI_AUTOMATIC)`
  - `SYSTEM_THREAD(ENABLED)`
- Boot behavior:
  - SoftAP prefix: `Aeris`
  - SoftAP page handler registered: `WebConfigServer::softApHandler`

### 1.2 System Interfaces Available at MCU Layer
- Wi-Fi:
  - `WifiManager::beginSetupMode()` enables `WiFi.listen()` (SoftAP)
  - `WifiManager::beginNormalMode()` sets STA credentials and calls `WiFi.connect()`
- EEPROM:
  - `SettingsStore` reads/writes `SettingsV2` (with CRC32 validation/default/sanitize)
- System control:
  - `System.reset()` (reboot)
  - `System.dfu(false)` (enter DFU mode)
- Diagnostics:
  - `Serial.begin(9600)` (debug serial)
  - `RGB.control(true)` + `RGB.color(...)` (status LED)

## 2. Firmware-Used Peripheral Components (Code-Based)

### 2.1 Fan Driver
- Module: `FanDriver`
- Pin: `D0`
- Hardware interface: `GPIO PWM` (`analogWrite`)
- Supported operation:
  - `setPercent(0..100)`: clamps to valid range and maps to PWM `0..255`
- Upstream control sources:
  - Buttons (`+/-5` adjustment)
  - MQTT `cmd/fan_percent`
  - Internal power toggle logic

### 2.2 Display and Lighting
- Module: `DisplayDriver`
- Display chipset/library: `ST7789` (`Adafruit_ST7735_RK` / `Adafruit_ST7789`)
- Pins:
  - TFT: `CS=A2`, `DC=A0`, `RST=-1`
  - TFT SPI data/clock: `MOSI`, `SCK` (hardware SPI; firmware limits to 8 MHz)
  - Backlight: `A1`
  - `D6`: display-related helper control line (set `OUTPUT LOW` at boot to avoid floating)
- Hardware interfaces:
  - `SPI` (TFT)
  - `GPIO` (backlight)
- Supported operations:
  - `renderSetupScreen()` (Wi-Fi setup guidance)
  - `renderConnectingScreen()` (connecting screen)
  - `render(state, settings)` (shows fan % and PM2.5)
  - `setLights(bool)` (backlight)

### 2.3 Button Inputs (4 keys)
- Module: `ButtonDriver`
- Pins: `D1`, `D2`, `D3`, `D4`
- Hardware interface: `GPIO input pulldown`
- Software handling:
  - debounce: `40ms`
  - command triggered on `LOW -> HIGH` only
- Supported operation interface (button to command mapping):
  - `D1`: `AdjustFanPercent +5`
  - `D2`: `AdjustFanPercent -5`
  - `D3`: short press `ToggleLights`; long press (>=5s) `ToggleWifi`
  - `D4`: short press `TogglePower`; long press (>=8s) `ResetWifiSettings` (clear saved Wi-Fi and reboot)

### 2.4 PM Sensor
- Module: `SensorDriver`
- Pins:
  - Sensor TX control pin: `A6` (bit-bang output for wake command)
  - Data receive: `Serial1` (9600)
- Hardware interfaces:
  - `UART` (`Serial1`)
  - `GPIO bit-bang` to send wake/start command
- Protocol characteristics (current implementation):
  - Parses 32-byte frames, header `0x32 0x3D`
  - Checksum: sum first 30 bytes and compare against the last 2 bytes
  - Extracted fields: `pm25_raw`, `pm10_raw`
  - If no data for 10 seconds (watchdog), sends 3 wake command groups in sequence

## 3. Communication and Control Interfaces (External)

### 3.1 MQTT Interface
- Module: `MqttClient`
- Broker settings source: `SettingsV2` (`mqtt_host/mqtt_port/mqtt_user/mqtt_pass/device_id`)
- Topic root: `aeris/v2/<device_id>`
- Subscriptions (control):
  - `aeris/v2/<device_id>/cmd/fan_percent` (payload `0..100`)
  - `aeris/v2/<device_id>/cmd/lights` (payload `0|1`)
  - `aeris/v2/<device_id>/cmd/screen_light` (payload `0|1`, A1 backlight only)
- Publishes (state):
  - `state/fan_percent`
  - `state/fan_pwm`
  - `state/lights`
  - `sensor/pm25`
  - `sensor/pm10`
  - `health/uptime_s`
  - `health/wifi_reconnect_count`
  - `health/mqtt_reconnect_count`
  - `health/sensor_parse_errors`

### 3.2 Local Web API (LAN)
- Module: `WebConfigServer`
- Service port: `80`
- Endpoints:
  - `GET /` (built-in web UI dashboard)
  - `GET /api/v2/settings`
  - `POST /api/v2/settings` (`x-www-form-urlencoded`)
  - `GET /api/v2/state`
  - `POST /api/v2/control` (`x-www-form-urlencoded`)
  - `POST /api/v2/system/reboot`
  - `POST /api/v2/system/dfu`
- Key response fields in `GET /api/v2/state`: `fan_percent`, `lights_on`, `screen_light_on`, `pm25`, `pm10`, `wifi_ready`, `mqtt_connected`
- Settings fields (POST):
  - Network: `wifi_ssid`, `wifi_pass`
  - MQTT: `mqtt_host`, `mqtt_port`, `mqtt_user`, `mqtt_pass`, `device_id`
  - UI: `fan_font_size`, `fan_x`, `fan_y`, `pm_font_size`, `pm_x`, `pm_y`,
    `fan_color`, `pm_label_color`, `pm_value_color`

### 3.3 SoftAP Setup Interface
- Entry: Photon listening mode + `softap_http`
- Page handler: `WebConfigServer::softApHandler`
- Basic flow:
  - `GET /save?s=<ssid>&p=<pass>` stores Wi-Fi credentials and sends reboot command
  - Without query parameters, returns a simple HTML form page

## 4. Current Pin Mapping (Firmware v2)

| Function | Pin | Interface Type | Module |
|---|---|---|---|
| Fan output | `D0` | PWM/GPIO | `FanDriver` |
| Button UP | `D1` | GPIO in | `ButtonDriver` |
| Button DOWN | `D2` | GPIO in | `ButtonDriver` |
| Button EXTRA | `D3` | GPIO in | `ButtonDriver` |
| Button POWER | `D4` | GPIO in | `ButtonDriver` |
| Display helper control | `D6` | GPIO out | `AppController` |
| TFT DC | `A0` | SPI control pin | `DisplayDriver` |
| TFT backlight | `A1` | GPIO out | `DisplayDriver` |
| TFT CS | `A2` | SPI control pin | `DisplayDriver` |
| TFT MOSI | `MOSI` | SPI data | `DisplayDriver` |
| TFT SCK | `SCK` | SPI clock | `DisplayDriver` |
| Sensor TX control | `A6` | GPIO out (bit-bang) | `SensorDriver` |
| Sensor data stream | `Serial1` | UART | `SensorDriver` |

## 5. Teardown References (External, Needs Hardware Confirmation)

Reference source:  
`https://gist.github.com/martinszelcel/0afaadd5701700c9c62d966b6a8ecd08`

Components mentioned in the teardown notes (summary):
- Mainboard MCU: `Silan SC92F8623B`
- Top control board: `P0A-01 V1.0`, with `CY8CKIT-059` (PSoC 5LP)
- Display: 2.4" TFT, controller `ST7789` (older notes mentioned `ILI9341`)
- Fan driver IC: `JY01`
- Particulate sensor: Honeywell series (mentions `HPMA115S0`)

Notes:
- These teardown details are external observations and do not necessarily represent all targets directly controlled by this firmware.
- Based on the current code in this repo, the control target is `Particle Photon`, and it directly uses TFT, buttons, fan PWM, and PM sensor UART.

## 6. Recommended Next Steps

- Use an oscilloscope or logic analyzer to verify the actual sensor model and command set behind `A6` and `Serial1`.
- Add a wiring diagram showing physical connections between `Particle Photon` and the original board MCU(s) if it is a dual-MCU architecture.
- Keep this document synchronized with `docs/interfaces.md` whenever topics/API are changed.
