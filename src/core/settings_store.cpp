#include "settings_store.h"

#include "Adafruit_ST77xx.h"
#include "../util/crc32.h"
#include "../util/string_safety.h"
#include "../util/topic_validation.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

namespace {
struct PersistentSettings {
    uint32_t magic;
    uint16_t version;
    uint16_t length;
    uint32_t crc32;

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

static_assert(sizeof(PersistentSettings) == SETTINGS_SCHEMA_LENGTH,
              "Persistent settings schema size changed unexpectedly");
static_assert(offsetof(PersistentSettings, wifi_ssid) == 12,
              "Persistent settings payload offset must stay stable");

int clampInt(int value, int min_v, int max_v) {
    if (value < min_v) {
        return min_v;
    }
    if (value > max_v) {
        return max_v;
    }
    return value;
}

void persistentToRuntime(const PersistentSettings& src, SettingsV2& dst) {
    safeCopy(dst.wifi_ssid, sizeof(dst.wifi_ssid), src.wifi_ssid);
    safeCopy(dst.wifi_pass, sizeof(dst.wifi_pass), src.wifi_pass);
    safeCopy(dst.mqtt_host, sizeof(dst.mqtt_host), src.mqtt_host);
    dst.mqtt_port = src.mqtt_port;
    dst.mqtt_enabled = src.mqtt_enabled ? 1 : 0;
    safeCopy(dst.mqtt_user, sizeof(dst.mqtt_user), src.mqtt_user);
    safeCopy(dst.mqtt_pass, sizeof(dst.mqtt_pass), src.mqtt_pass);
    safeCopy(dst.device_id, sizeof(dst.device_id), src.device_id);
    safeCopy(dst.mqtt_topic_root, sizeof(dst.mqtt_topic_root), src.mqtt_topic_root);

    dst.fan_font_size = src.fan_font_size;
    dst.fan_x = src.fan_x;
    dst.fan_y = src.fan_y;
    dst.pm_font_size = src.pm_font_size;
    dst.pm_x = src.pm_x;
    dst.pm_y = src.pm_y;

    dst.fan_color = src.fan_color;
    dst.pm_label_color = src.pm_label_color;
    dst.pm_value_color = src.pm_value_color;
}

void runtimeToPersistent(const SettingsV2& src, PersistentSettings& dst) {
    safeCopy(dst.wifi_ssid, sizeof(dst.wifi_ssid), src.wifi_ssid);
    safeCopy(dst.wifi_pass, sizeof(dst.wifi_pass), src.wifi_pass);
    safeCopy(dst.mqtt_host, sizeof(dst.mqtt_host), src.mqtt_host);
    dst.mqtt_port = src.mqtt_port;
    dst.mqtt_enabled = src.mqtt_enabled ? 1 : 0;
    safeCopy(dst.mqtt_user, sizeof(dst.mqtt_user), src.mqtt_user);
    safeCopy(dst.mqtt_pass, sizeof(dst.mqtt_pass), src.mqtt_pass);
    safeCopy(dst.device_id, sizeof(dst.device_id), src.device_id);
    safeCopy(dst.mqtt_topic_root, sizeof(dst.mqtt_topic_root), src.mqtt_topic_root);

    dst.fan_font_size = src.fan_font_size;
    dst.fan_x = src.fan_x;
    dst.fan_y = src.fan_y;
    dst.pm_font_size = src.pm_font_size;
    dst.pm_x = src.pm_x;
    dst.pm_y = src.pm_y;

    dst.fan_color = src.fan_color;
    dst.pm_label_color = src.pm_label_color;
    dst.pm_value_color = src.pm_value_color;
}

uint32_t calculatePersistentCrc(const PersistentSettings& settings) {
    PersistentSettings copy = settings;
    copy.crc32 = 0;
    return crc32_bytes(reinterpret_cast<const uint8_t*>(&copy), sizeof(copy));
}

bool validatePersistent(const PersistentSettings& s) {
    if (s.magic != SETTINGS_MAGIC) {
        return false;
    }
    if (s.version != SETTINGS_SCHEMA_VERSION) {
        return false;
    }
    if (s.length != sizeof(PersistentSettings)) {
        return false;
    }
    if (!hasNullTerminator(s.wifi_ssid, sizeof(s.wifi_ssid))) {
        return false;
    }
    if (!hasNullTerminator(s.wifi_pass, sizeof(s.wifi_pass))) {
        return false;
    }
    if (!hasNullTerminator(s.mqtt_host, sizeof(s.mqtt_host))) {
        return false;
    }
    if (!hasNullTerminator(s.mqtt_user, sizeof(s.mqtt_user))) {
        return false;
    }
    if (!hasNullTerminator(s.mqtt_pass, sizeof(s.mqtt_pass))) {
        return false;
    }
    if (!hasNullTerminator(s.device_id, sizeof(s.device_id))) {
        return false;
    }
    if (!hasNullTerminator(s.mqtt_topic_root, sizeof(s.mqtt_topic_root))) {
        return false;
    }
    if (!isDeviceIdTopicSafe(s.device_id, sizeof(s.device_id) - 1)) {
        return false;
    }
    if (!isTopicRootSafe(s.mqtt_topic_root, sizeof(s.mqtt_topic_root))) {
        return false;
    }
    if (!(s.mqtt_enabled == 0 || s.mqtt_enabled == 1)) {
        return false;
    }
    return s.crc32 == calculatePersistentCrc(s);
}
}  // namespace

bool SettingsStore::loadOrInitialize(SettingsV2& out) {
    PersistentSettings persisted = {};
    EEPROM.get(EEPROM_ADDR_SETTINGS, persisted);

    if (validatePersistent(persisted)) {
        persistentToRuntime(persisted, out);
        sanitize(out);
        return true;
    }

    applyDefaults(out);
    save(out);
    return false;
}

bool SettingsStore::save(SettingsV2& settings) {
    sanitize(settings);

    PersistentSettings persisted = {};
    runtimeToPersistent(settings, persisted);
    persisted.magic = SETTINGS_MAGIC;
    persisted.version = SETTINGS_SCHEMA_VERSION;
    persisted.length = sizeof(PersistentSettings);
    persisted.crc32 = calculatePersistentCrc(persisted);

    EEPROM.put(EEPROM_ADDR_SETTINGS, persisted);
    return true;
}

void SettingsStore::applyDefaults(SettingsV2& settings) {
    memset(&settings, 0, sizeof(settings));

    safeCopy(settings.mqtt_host, sizeof(settings.mqtt_host), "");
    settings.mqtt_port = 1883;
    settings.mqtt_enabled = 0;
    safeCopy(settings.mqtt_user, sizeof(settings.mqtt_user), "mqtt_user");

    String id = System.deviceID();
    safeCopy(settings.device_id, sizeof(settings.device_id), id.c_str());
    buildDefaultTopicRoot(settings.device_id, settings.mqtt_topic_root, sizeof(settings.mqtt_topic_root));

    settings.fan_font_size = 10;
    settings.fan_x = 30;
    settings.fan_y = 70;
    settings.pm_font_size = 3;
    settings.pm_x = 60;
    settings.pm_y = 180;

    settings.fan_color = ST77XX_WHITE;
    settings.pm_label_color = ST77XX_CYAN;
    settings.pm_value_color = ST77XX_WHITE;
    sanitize(settings);
}

void SettingsStore::sanitize(SettingsV2& s) {
    s.wifi_ssid[sizeof(s.wifi_ssid) - 1] = '\0';
    s.wifi_pass[sizeof(s.wifi_pass) - 1] = '\0';
    s.mqtt_host[sizeof(s.mqtt_host) - 1] = '\0';
    s.mqtt_user[sizeof(s.mqtt_user) - 1] = '\0';
    s.mqtt_pass[sizeof(s.mqtt_pass) - 1] = '\0';
    s.device_id[sizeof(s.device_id) - 1] = '\0';
    s.mqtt_topic_root[sizeof(s.mqtt_topic_root) - 1] = '\0';

    if (!isDeviceIdTopicSafe(s.device_id, sizeof(s.device_id) - 1)) {
        char normalized_id[sizeof(s.device_id)] = {0};
        normalizeDeviceId(s.device_id, normalized_id, sizeof(normalized_id));
        safeCopy(s.device_id, sizeof(s.device_id), normalized_id);
    }
    if (!isTopicRootSafe(s.mqtt_topic_root, sizeof(s.mqtt_topic_root))) {
        buildDefaultTopicRoot(s.device_id, s.mqtt_topic_root, sizeof(s.mqtt_topic_root));
    }

    s.mqtt_port = static_cast<uint16_t>(clampInt(s.mqtt_port, 1, 65535));
    s.mqtt_enabled = s.mqtt_enabled ? 1 : 0;

    s.fan_font_size = static_cast<int16_t>(clampInt(s.fan_font_size, 1, 12));
    s.pm_font_size = static_cast<int16_t>(clampInt(s.pm_font_size, 1, 12));
    s.fan_x = static_cast<int16_t>(clampInt(s.fan_x, 0, 319));
    s.pm_x = static_cast<int16_t>(clampInt(s.pm_x, 0, 319));
    s.fan_y = static_cast<int16_t>(clampInt(s.fan_y, 0, 239));
    s.pm_y = static_cast<int16_t>(clampInt(s.pm_y, 0, 239));
}
