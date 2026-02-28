# Aeris AAIR Firmware v2 Architecture

## Module Map
- `src/app/main.cpp`: Particle firmware entrypoint and startup hooks.
- `src/app/app_controller.*`: scheduler, module orchestration, command queue.
- `src/app/command_router.*`: centralized state transitions from commands.
- `src/core/device_state.h`: runtime source of truth for all mutable state.
- `src/core/settings_store.*`: EEPROM SettingsV2, validation, defaults, CRC.
- `src/drivers/*`: fan, display, button, sensor hardware drivers.
- `src/net/*`: Wi-Fi lifecycle, MQTT v2 transport, Web API config endpoints.
- `src/util/*`: shared utilities (CRC32, moving average).

## Scheduler Order
1. Drain pending commands.
2. Poll buttons and enqueue commands.
3. Poll sensor stream and parse packets.
4. Serve Web API.
5. Tick Wi-Fi manager.
6. Tick MQTT manager.
7. Periodic report aggregation and health publish.
8. Apply hardware outputs.

## SettingsV2 Lifecycle
- Boot: `load -> validate crc/magic/version/length`.
- If invalid: apply defaults.
- On success: sanitize and use settings in-memory (no per-boot EEPROM write).
