#pragma once

#include "Particle.h"
#include "Adafruit_ST7789.h"
#include "../core/device_state.h"
#include "../core/settings_store.h"

class DisplayDriver {
public:
    DisplayDriver(int cs, int dc, int rst, int bl_pin);

    void init();
    void reinitialize();
    void renderSetupScreen(const char* softap_ssid, const char* softap_ip);
    void renderConnectingScreen();
    void render(const DeviceState& state, const SettingsV2& settings);
    void setLights(bool on);
    void setScreenLight(bool on);

private:
    void initPanel();
    void resetRenderCache();

    Adafruit_ST7789 tft_;
    int bl_pin_;
    bool screen_light_on_;

    int last_fan_percent_;
    int last_pm25_;
    int last_pm10_;
    bool last_wifi_enabled_;
    bool last_wifi_ready_;
    int last_wifi_status_code_;
    char last_wifi_status_text_[32];
    int last_wifi_status_x_;
    int last_wifi_status_y_;
    int last_wifi_status_w_;
    bool has_drawn_;
    bool setup_screen_drawn_;
    char last_setup_ssid_[40];
    char last_setup_ip_[24];
};
