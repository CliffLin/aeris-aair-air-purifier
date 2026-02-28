#pragma once

#include "Particle.h"

struct DeviceState {
    int fan_percent;
    int saved_fan_percent;
    bool lights_on;
    bool screen_light_on;

    int pm25_raw;
    int pm10_raw;
    int pm25_smooth;
    int pm10_smooth;

    bool wifi_ready;
    bool wifi_enabled;
    bool wifi_ip_visible;
    bool mqtt_connected;

    uint32_t boot_ms;
    uint32_t last_sensor_packet_ms;

    uint32_t wifi_reconnect_count;
    uint32_t mqtt_reconnect_count;
    uint32_t sensor_parse_errors;
    uint32_t command_drop_button_count;
    uint32_t command_drop_mqtt_count;
    uint32_t command_drop_web_count;
    uint32_t mqtt_publish_drop_count;

    bool dirty_display;
    bool dirty_publish;
};

inline void initDeviceState(DeviceState& state, uint32_t now_ms) {
    state.fan_percent = 25;
    state.saved_fan_percent = 25;
    state.lights_on = true;
    state.screen_light_on = true;
    state.pm25_raw = 0;
    state.pm10_raw = 0;
    state.pm25_smooth = 0;
    state.pm10_smooth = 0;
    state.wifi_ready = false;
    state.wifi_enabled = true;
    state.wifi_ip_visible = false;
    state.mqtt_connected = false;
    state.boot_ms = now_ms;
    state.last_sensor_packet_ms = now_ms;
    state.wifi_reconnect_count = 0;
    state.mqtt_reconnect_count = 0;
    state.sensor_parse_errors = 0;
    state.command_drop_button_count = 0;
    state.command_drop_mqtt_count = 0;
    state.command_drop_web_count = 0;
    state.mqtt_publish_drop_count = 0;
    state.dirty_display = true;
    state.dirty_publish = true;
}
