#pragma once

#include "Particle.h"

static const uint32_t SETTINGS_MAGIC = 0x41414952UL;  // 'AAIR'
static const uint16_t SETTINGS_SCHEMA_VERSION = 4;
static const int EEPROM_ADDR_SETTINGS = 0;
static const size_t SETTINGS_SCHEMA_LENGTH = 356;

struct SettingsV2 {
    char wifi_ssid[64];
    char wifi_pass[64];
    char mqtt_host[32];
    uint16_t mqtt_port;
    uint8_t mqtt_enabled;
    char mqtt_user[32];
    char mqtt_pass[32];
    char device_id[32];
    char mqtt_topic_root[64];

    int16_t fan_font_size;
    int16_t fan_x;
    int16_t fan_y;
    int16_t pm_font_size;
    int16_t pm_x;
    int16_t pm_y;

    uint16_t fan_color;
    uint16_t pm_label_color;
    uint16_t pm_value_color;
};

class SettingsStore {
public:
    bool loadOrInitialize(SettingsV2& out);
    bool save(SettingsV2& settings);
    void applyDefaults(SettingsV2& settings);
    void sanitize(SettingsV2& settings);
};
