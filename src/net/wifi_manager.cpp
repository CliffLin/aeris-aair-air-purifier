#include "wifi_manager.h"

#include <stdio.h>
#include <string.h>

namespace {
const char kSoftApPrefix[] = "Aeris";
const char kBase36Digits[] = "0123456789abcdefghijklmnopqrstuvwxyz";
const int kSoftApSuffixChars = 4;
}

WifiManager::WifiManager() : last_connect_ms_(0), normal_mode_started_(false) {
    snprintf(softap_ssid_, sizeof(softap_ssid_), "%s-XXXX", kSoftApPrefix);
}

void WifiManager::buildSoftApIdentity() {
    uint32_t hash = 2166136261UL;
    String id = System.deviceID();
    for (size_t i = 0; i < id.length(); ++i) {
        hash ^= static_cast<uint8_t>(id.charAt(i));
        hash *= 16777619UL;
    }

    char suffix[kSoftApSuffixChars + 1];
    for (int i = kSoftApSuffixChars - 1; i >= 0; --i) {
        suffix[i] = kBase36Digits[hash % 36];
        hash /= 36;
    }
    suffix[kSoftApSuffixChars] = '\0';

    System.set(SYSTEM_CONFIG_SOFTAP_PREFIX, kSoftApPrefix);
    System.set(SYSTEM_CONFIG_SOFTAP_SUFFIX, suffix);
    snprintf(softap_ssid_, sizeof(softap_ssid_), "%s-%s", kSoftApPrefix, suffix);
}

void WifiManager::beginSetupMode() {
    buildSoftApIdentity();
    WiFi.on();
    WiFi.listen();
    normal_mode_started_ = false;
}

void WifiManager::beginNormalMode(const SettingsV2& settings) {
    WiFi.on();
    if (strlen(settings.wifi_ssid) > 0) {
        if (strlen(settings.wifi_pass) > 0) {
            WiFi.setCredentials(settings.wifi_ssid, settings.wifi_pass);
        } else {
            WiFi.setCredentials(settings.wifi_ssid);
        }
    }
    WiFi.connect();
    last_connect_ms_ = millis();
    normal_mode_started_ = true;
}

void WifiManager::tick(uint32_t now_ms, DeviceState& state) {
    state.wifi_ready = WiFi.ready();
    if (!normal_mode_started_ || WiFi.listening()) {
        return;
    }
    if (!state.wifi_ready && now_ms - last_connect_ms_ > 5000) {
        WiFi.connect();
        last_connect_ms_ = now_ms;
        state.wifi_reconnect_count += 1;
    }
}

const char* WifiManager::softApSsid() const {
    return softap_ssid_;
}
