#pragma once

#include "Particle.h"
#include "../core/settings_store.h"
#include "../core/device_state.h"

class WifiManager {
public:
    WifiManager();

    void beginSetupMode();
    void beginNormalMode(const SettingsV2& settings);
    void tick(uint32_t now_ms, DeviceState& state);
    const char* softApSsid() const;

private:
    void buildSoftApIdentity();

    uint32_t last_connect_ms_;
    bool normal_mode_started_;
    char softap_ssid_[40];
};
