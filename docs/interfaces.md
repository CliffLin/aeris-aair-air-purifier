# Aeris AAIR v2 Interfaces

## MQTT Namespace
Root: `aeris/v2/<device_id>`

### Command Topics
- `aeris/v2/<device_id>/cmd/fan_percent` payload `0..100`
- `aeris/v2/<device_id>/cmd/lights` payload `0|1`
- `aeris/v2/<device_id>/cmd/screen_light` payload `0|1` (A1 TFT backlight)
- `aeris/v2/<device_id>/cmd/ring` payload `0..255` key-light pattern byte (`0x03` power, `0x0C` AirQ, `0x30` down, `0xC0` up)
- `aeris/v2/<device_id>/cmd/ring_brightness` payload `0..100` percent, quantised to 5 duty levels; `state/ring_brightness` reports the quantised percent (`0`, `25`, `50`, `75` or `100`)
- `aeris/v2/<device_id>/cmd/ring_blink` payload half period in ms, `0` = solid
- `aeris/v2/<device_id>/cmd/status_led` payload `0|1`
- `aeris/v2/<device_id>/cmd/filter_days` payload days of filter life remaining

### State Topics
- `aeris/v2/<device_id>/state/fan_percent`
- `aeris/v2/<device_id>/state/fan_pwm`
- `aeris/v2/<device_id>/state/lights`
- `aeris/v2/<device_id>/state/ring`
- `aeris/v2/<device_id>/state/ring_brightness`
- `aeris/v2/<device_id>/state/ring_blink`
- `aeris/v2/<device_id>/state/status_led`
- `aeris/v2/<device_id>/sensor/pm25`
- `aeris/v2/<device_id>/sensor/pm10`
- `aeris/v2/<device_id>/sensor/filter_minutes`
- `aeris/v2/<device_id>/health/uptime_s`
- `aeris/v2/<device_id>/health/wifi_reconnect_count`
- `aeris/v2/<device_id>/health/mqtt_reconnect_count`
- `aeris/v2/<device_id>/health/mqtt_publish_drop_count`
- `aeris/v2/<device_id>/health/sensor_parse_errors`
- `aeris/v2/<device_id>/health/command_drop_{button,mqtt,web}_count`

Payloads are primitive strings.

## Web API
Base path on device local IP:
- `GET /` serves the built-in Web UI dashboard.
- `GET /api/v2/settings` returns current settings JSON (without secret redaction logic).
- `POST /api/v2/settings` with urlencoded form updates settings.
- `GET /api/v2/state` returns live runtime state (`pm25`, `pm10`, fan, connectivity, `screen_light_on`).
- `POST /api/v2/control` with urlencoded form sends runtime commands (`fan_percent`, `lights`, `screen_light`).
- `POST /api/v2/system/reboot` requests reboot.
- `POST /api/v2/system/dfu` requests DFU mode.

Validation failures return HTTP 400 with JSON body.

## SoftAP Setup
Captive portal path handled by `softap_http`:
- `GET /save?s=<ssid>&p=<pass>` stores Wi-Fi credentials and reboots.
