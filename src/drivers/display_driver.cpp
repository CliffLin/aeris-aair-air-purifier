#include "display_driver.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

namespace {
constexpr uint16_t kMainBgColor = ST77XX_BLACK;
constexpr uint16_t kFanTextColor = ST77XX_WHITE;
constexpr int kPmBlockXOffset = 34;
constexpr int kPmMaxSubscriptChars = 3;  // Align value column to longest subscript: "2.5"
constexpr uint32_t kDisplayPowerSettleMs = 120;
constexpr uint32_t kDisplaySpiHz = 8000000;
constexpr int kWifiIconMinSize = 18;
constexpr int kWifiIconMaxSize = 120;
constexpr int kStatusTextSize = 2;
constexpr float kDegToRad = 0.0174532925f;
constexpr float kWifiArcStartDeg = 225.0f;
constexpr float kWifiArcEndDeg = 315.0f;
constexpr float kWifiArcStepDeg = 2.0f;
constexpr uint16_t kWifiOkColor = ST77XX_GREEN;
// This panel wiring/color-order renders RGB565 blue as visible red on-device.
constexpr uint16_t kWifiAlertColor = ST77XX_BLUE;
constexpr int kSetupTextX = 26;
constexpr int kSetupTitleY = 50;
constexpr int kSetupConnectY = 100;
constexpr int kSetupSsidY = 130;
constexpr int kSetupIpY = 180;

int clampInt(int value, int min_v, int max_v) {
    if (value < min_v) {
        return min_v;
    }
    if (value > max_v) {
        return max_v;
    }
    return value;
}

int wifiStatusCode(bool wifi_enabled, bool wifi_ready) {
    if (!wifi_enabled) {
        return 2;
    }
    if (!wifi_ready) {
        return 1;
    }
    return 0;
}

const char* wifiStatusText(int status_code) {
    if (status_code == 1) {
        return "wifi connecting";
    }
    if (status_code == 2) {
        return "wifi disconnected";
    }
    return "";
}

void buildWifiStatusText(int status_code, bool wifi_ip_visible, char* out, size_t out_size) {
    if (out_size == 0) {
        return;
    }
    out[0] = '\0';

    if (status_code == 1) {
        strncpy(out, wifiStatusText(status_code), out_size - 1);
        out[out_size - 1] = '\0';
        return;
    }
    if (status_code == 2) {
        if (wifi_ip_visible) {
            strncpy(out, wifiStatusText(status_code), out_size - 1);
            out[out_size - 1] = '\0';
        }
        return;
    }

    if (wifi_ip_visible) {
        IPAddress ip = WiFi.localIP();
        snprintf(out, out_size, "wifi %u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
        return;
    }
}

uint16_t wifiStatusColor(int status_code) {
    if (status_code == 1) {
        return ST77XX_WHITE;
    }
    if (status_code == 0) {
        return kWifiOkColor;
    }
    return kWifiAlertColor;
}

void drawWifiArc(Adafruit_ST7789& tft, int cx, int cy, int radius, int thickness, uint16_t color) {
    if (radius <= 0 || thickness <= 0) {
        return;
    }
    int effective_thickness = thickness;
    if (effective_thickness > radius) {
        effective_thickness = radius;
    }

    for (int layer = 0; layer < effective_thickness; ++layer) {
        int r = radius - layer;
        bool has_prev = false;
        int prev_x = 0;
        int prev_y = 0;
        for (float deg = kWifiArcStartDeg; deg <= (kWifiArcEndDeg + 0.01f); deg += kWifiArcStepDeg) {
            float rad = deg * kDegToRad;
            int x = cx + static_cast<int>(cosf(rad) * r);
            int y = cy + static_cast<int>(sinf(rad) * r);
            if (has_prev) {
                tft.drawLine(prev_x, prev_y, x, y, color);
            } else {
                tft.drawPixel(x, y, color);
                has_prev = true;
            }
            prev_x = x;
            prev_y = y;
        }
    }

    int cap_r = effective_thickness / 2;
    if (cap_r < 1) {
        cap_r = 1;
    }
    int cap_radius = radius - (effective_thickness / 2);
    if (cap_radius < 1) {
        cap_radius = 1;
    }
    float start_rad = kWifiArcStartDeg * kDegToRad;
    float end_rad = kWifiArcEndDeg * kDegToRad;
    int left_x = cx + static_cast<int>(cosf(start_rad) * cap_radius);
    int left_y = cy + static_cast<int>(sinf(start_rad) * cap_radius);
    int right_x = cx + static_cast<int>(cosf(end_rad) * cap_radius);
    int right_y = cy + static_cast<int>(sinf(end_rad) * cap_radius);
    tft.fillCircle(left_x, left_y, cap_r, color);
    tft.fillCircle(right_x, right_y, cap_r, color);
}

void drawWifiIcon(Adafruit_ST7789& tft,
                  int x,
                  int y,
                  int icon_size,
                  bool wifi_enabled,
                  bool wifi_ready) {
    const int screen_w = tft.width();
    const int screen_h = tft.height();
    if (x >= screen_w || y >= screen_h) {
        return;
    }
    if (x < 0) {
        x = 0;
    }
    if (y < 0) {
        y = 0;
    }

    int clear_w = icon_size;
    int clear_h = icon_size;
    if (x + clear_w > screen_w) {
        clear_w = screen_w - x;
    }
    if (y + clear_h > screen_h) {
        clear_h = screen_h - y;
    }
    if (clear_w <= 0 || clear_h <= 0) {
        return;
    }

    tft.fillRect(x, y, clear_w, clear_h, kMainBgColor);

    const int center_x = x + (clear_w / 2);
    const uint16_t color = (wifi_enabled && wifi_ready) ? kWifiOkColor : kWifiAlertColor;

    int stroke = clear_w / 10;
    if (stroke < 2) {
        stroke = 2;
    }
    int gap = stroke + 1;
    int anchor_dot_r = stroke;
    if (anchor_dot_r < 2) {
        anchor_dot_r = 2;
    }
    int arc_center_y = y + clear_h - anchor_dot_r - 1;
    if (arc_center_y <= y + 2) {
        arc_center_y = y + 2;
    }
    int outer = (clear_w / 2) - 2;
    if (outer < 6) {
        outer = 6;
    }
    int mid = outer - (stroke + gap);
    if (mid < 4) {
        mid = 4;
    }
    if (mid >= outer) {
        mid = outer - 1;
    }
    int inner = mid - (stroke + gap);
    if (inner < 2) {
        inner = 2;
    }
    if (inner >= mid) {
        inner = mid - 1;
    }

    // Draw three smooth line arcs plus a dot, matching common Wi-Fi icon style.
    drawWifiArc(tft, center_x, arc_center_y, outer, stroke, color);
    drawWifiArc(tft, center_x, arc_center_y, mid, stroke, color);
    drawWifiArc(tft, center_x, arc_center_y, inner, stroke, color);
}

void drawPmLine(Adafruit_ST7789& tft, int x, int y, int value, const char* subscript,
                const SettingsV2& settings) {
    const int value_size = settings.pm_font_size > 0 ? settings.pm_font_size : 1;
    const int label_size = value_size > 1 ? value_size - 1 : 1;
    const int sub_size = label_size > 1 ? label_size - 1 : 1;
    const int value_char_w = 6 * value_size;
    const int value_char_h = 8 * value_size;
    const int label_char_w = 6 * label_size;
    const int label_char_h = 8 * label_size;
    const int sub_char_w = 6 * sub_size;
    const int sub_char_h = 8 * sub_size;

    const int screen_w = tft.width();
    const int screen_h = tft.height();
    if (x >= screen_w || y >= screen_h) {
        return;
    }

    const int line_w = screen_w - x;
    const int line_h = (y + value_char_h <= screen_h) ? value_char_h : (screen_h - y);
    if (line_w <= 0 || line_h <= 0) {
        return;
    }

    tft.fillRect(x, y, line_w, line_h, kMainBgColor);

    int label_y = y + ((value_char_h - label_char_h) / 2);
    if (label_y < y) {
        label_y = y;
    }

    tft.setTextSize(label_size);
    tft.setTextColor(settings.pm_label_color, kMainBgColor);
    tft.setCursor(x, label_y);
    tft.print("PM");

    const int sub_x = x + (2 * label_char_w);
    int sub_y = label_y + (label_char_h - sub_char_h);
    if (sub_y + sub_char_h > screen_h) {
        sub_y = screen_h - sub_char_h;
    }
    if (sub_y < 0) {
        sub_y = 0;
    }

    tft.setTextSize(sub_size);
    tft.setCursor(sub_x, sub_y);
    tft.print(subscript);

    // Keep the value column fixed for PM2.5/PM10 so both numeric values align vertically.
    const int value_x = x + (2 * label_char_w) + (kPmMaxSubscriptChars * sub_char_w) + label_char_w;
    char value_text[12];
    snprintf(value_text, sizeof(value_text), "%d", value);

    tft.setTextSize(value_size);
    tft.setTextColor(settings.pm_value_color, kMainBgColor);
    tft.setCursor(value_x, y);
    tft.print(value_text);

    const int unit_x = value_x + (int) strlen(value_text) * value_char_w + (label_char_w / 2);
    const int unit_y = label_y;
    tft.setTextColor(settings.pm_label_color, kMainBgColor);
    tft.setTextSize(label_size);
    tft.setCursor(unit_x, unit_y);
    tft.print("ug/m3");
}
}  // namespace

DisplayDriver::DisplayDriver(int cs, int dc, int rst, int bl_pin)
    : tft_(cs, dc, rst),
      bl_pin_(bl_pin),
      screen_light_on_(false),
      last_fan_percent_(-1),
      last_pm25_(-1),
      last_pm10_(-1),
      last_wifi_enabled_(false),
      last_wifi_ready_(false),
      last_wifi_status_code_(-1),
      last_wifi_status_x_(0),
      last_wifi_status_y_(0),
      last_wifi_status_w_(0),
      has_drawn_(false),
      setup_screen_drawn_(false) {
    last_wifi_status_text_[0] = '\0';
    last_setup_ssid_[0] = '\0';
    last_setup_ip_[0] = '\0';
}

void DisplayDriver::init() {
    pinMode(bl_pin_, OUTPUT);
    initPanel();
}

void DisplayDriver::reinitialize() {
    const bool restore_light = screen_light_on_;
    initPanel();
    setScreenLight(restore_light);
}

void DisplayDriver::initPanel() {
    setScreenLight(false);
    delay(kDisplayPowerSettleMs);
    tft_.init(240, 320);
    tft_.setSPISpeed(kDisplaySpiHz);
    tft_.setRotation(1);
    // Keep landscape coordinate geometry from setRotation(1), then override
    // MADCTL for panel-specific orientation: MV + MX + MY flips 180 degrees
    // from MV-only, fixing upside-down rendering without reintroducing mirror.
    const uint8_t madctl =
        ST77XX_MADCTL_MX | ST77XX_MADCTL_MY | ST77XX_MADCTL_MV | ST77XX_MADCTL_RGB;
    tft_.sendCommand(ST77XX_MADCTL, &madctl, 1);
    tft_.invertDisplay(false);
    tft_.fillScreen(kMainBgColor);
    resetRenderCache();
}

void DisplayDriver::setLights(bool on) {
    setScreenLight(on);
    if (!on) {
        resetRenderCache();
    }
}

void DisplayDriver::setScreenLight(bool on) {
    screen_light_on_ = on;
    digitalWrite(bl_pin_, on ? HIGH : LOW);
}

void DisplayDriver::resetRenderCache() {
    has_drawn_ = false;
    setup_screen_drawn_ = false;
    last_fan_percent_ = -1;
    last_pm25_ = -1;
    last_pm10_ = -1;
    last_wifi_enabled_ = false;
    last_wifi_ready_ = false;
    last_wifi_status_code_ = -1;
    last_wifi_status_text_[0] = '\0';
    last_wifi_status_x_ = 0;
    last_wifi_status_y_ = 0;
    last_wifi_status_w_ = 0;
    last_setup_ssid_[0] = '\0';
    last_setup_ip_[0] = '\0';
}

void DisplayDriver::renderSetupScreen(const char* softap_ssid, const char* softap_ip) {
    const char* ssid = (softap_ssid != nullptr && softap_ssid[0] != '\0') ? softap_ssid : "Aeris-XXXX";
    const char* ip = (softap_ip != nullptr && softap_ip[0] != '\0') ? softap_ip : "192.168.0.1";

    if (setup_screen_drawn_ &&
        strcmp(last_setup_ssid_, ssid) == 0 &&
        strcmp(last_setup_ip_, ip) == 0) {
        return;
    }

    tft_.fillScreen(ST77XX_BLACK);
    tft_.setTextColor(ST77XX_WHITE);
    tft_.setTextSize(3);
    tft_.setCursor(kSetupTextX, kSetupTitleY);
    tft_.println("SETUP MODE");
    tft_.setTextSize(2);
    tft_.setCursor(kSetupTextX, kSetupConnectY);
    tft_.println("Connect to:");
    tft_.setCursor(kSetupTextX, kSetupSsidY);
    tft_.println(ssid);
    tft_.setCursor(kSetupTextX, kSetupIpY);
    tft_.print("http://");
    tft_.println(ip);

    strncpy(last_setup_ssid_, ssid, sizeof(last_setup_ssid_) - 1);
    last_setup_ssid_[sizeof(last_setup_ssid_) - 1] = '\0';
    strncpy(last_setup_ip_, ip, sizeof(last_setup_ip_) - 1);
    last_setup_ip_[sizeof(last_setup_ip_) - 1] = '\0';
    setup_screen_drawn_ = true;
    has_drawn_ = false;
}

void DisplayDriver::renderConnectingScreen() {
    setup_screen_drawn_ = false;
    tft_.fillScreen(ST77XX_BLACK);
    tft_.setTextColor(ST77XX_WHITE);
    tft_.setTextSize(2);
    tft_.setCursor(10, 100);
    tft_.println("Connecting...");
}

void DisplayDriver::render(const DeviceState& state, const SettingsV2& settings) {
    if (!state.lights_on) {
        return;
    }

    setup_screen_drawn_ = false;

    if (!has_drawn_) {
        tft_.fillScreen(kMainBgColor);
        has_drawn_ = true;
        last_fan_percent_ = -1;
        last_pm25_ = -1;
        last_pm10_ = -1;
        last_wifi_enabled_ = !state.wifi_enabled;
        last_wifi_ready_ = !state.wifi_ready;
        last_wifi_status_code_ = -1;
        last_wifi_status_text_[0] = '\0';
        last_wifi_status_x_ = 0;
        last_wifi_status_y_ = 0;
        last_wifi_status_w_ = 0;
    }

    int line_height = 8 * settings.pm_font_size;
    if (line_height < 8) {
        line_height = 8;
    }
    int pm10_y = settings.pm_y + line_height + 2;
    int max_y = tft_.height() - line_height;
    if (pm10_y > max_y) {
        pm10_y = max_y;
    }
    if (pm10_y < 0) {
        pm10_y = 0;
    }
    int pm_block_h = (pm10_y - settings.pm_y) + line_height;
    if (pm_block_h < line_height) {
        pm_block_h = line_height;
    }

    int wifi_status = wifiStatusCode(state.wifi_enabled, state.wifi_ready);
    char status_text[sizeof(last_wifi_status_text_)] = {0};
    buildWifiStatusText(wifi_status, state.wifi_ip_visible, status_text, sizeof(status_text));

    if (last_wifi_status_code_ != wifi_status ||
        strcmp(last_wifi_status_text_, status_text) != 0) {
        last_wifi_status_code_ = wifi_status;
        strncpy(last_wifi_status_text_, status_text, sizeof(last_wifi_status_text_) - 1);
        last_wifi_status_text_[sizeof(last_wifi_status_text_) - 1] = '\0';

        int status_h = 8 * kStatusTextSize;
        int status_y = settings.fan_y - status_h - 4;
        if (status_y < 0) {
            status_y = 0;
        }

        if (last_wifi_status_w_ > 0) {
            tft_.fillRect(last_wifi_status_x_, last_wifi_status_y_, last_wifi_status_w_, status_h,
                          kMainBgColor);
            last_wifi_status_w_ = 0;
        }

        int status_w = 0;
        if (status_text[0] != '\0') {
            status_w = (int) strlen(status_text) * 6 * kStatusTextSize;
            if (status_w < 1) {
                status_w = 1;
            }
        }
        if (status_w > 0) {
            int centered_x = (tft_.width() - status_w) / 2;
            if (centered_x < 0) {
                centered_x = 0;
            }
            if (centered_x + status_w > tft_.width()) {
                centered_x = tft_.width() - status_w;
                if (centered_x < 0) {
                    centered_x = 0;
                }
            }
            tft_.fillRect(centered_x, status_y, status_w, status_h, kMainBgColor);
            tft_.setTextSize(kStatusTextSize);
            tft_.setTextColor(wifiStatusColor(wifi_status), kMainBgColor);
            tft_.setCursor(centered_x, status_y);
            tft_.print(status_text);

            last_wifi_status_x_ = centered_x;
            last_wifi_status_y_ = status_y;
            last_wifi_status_w_ = status_w;
        } else {
            last_wifi_status_x_ = 0;
            last_wifi_status_y_ = status_y;
            last_wifi_status_w_ = 0;
        }
    }

    if (last_wifi_enabled_ != state.wifi_enabled || last_wifi_ready_ != state.wifi_ready) {
        last_wifi_enabled_ = state.wifi_enabled;
        last_wifi_ready_ = state.wifi_ready;

        int wifi_size = clampInt(pm_block_h, kWifiIconMinSize, kWifiIconMaxSize);
        int wifi_x = clampInt(settings.pm_x - (wifi_size / 2), 0, tft_.width() - 1);
        int wifi_y = clampInt(settings.pm_y - 12, 0, tft_.height() - 1);
        drawWifiIcon(tft_, wifi_x, wifi_y, wifi_size, state.wifi_enabled, state.wifi_ready);
    }

    if (last_fan_percent_ != state.fan_percent) {
        last_fan_percent_ = state.fan_percent;
        tft_.setCursor(settings.fan_x, settings.fan_y);
        tft_.setTextColor(kFanTextColor, kMainBgColor);
        tft_.setTextSize(settings.fan_font_size);

        if (state.fan_percent <= 0) {
            tft_.print("OFF");
            tft_.fillRect(settings.fan_x + (3 * 6 * settings.fan_font_size), settings.fan_y, 120,
                          8 * settings.fan_font_size, kMainBgColor);
        } else {
            char fan_text[8];
            snprintf(fan_text, sizeof(fan_text), "%3d", state.fan_percent);
            tft_.print(fan_text);
            int symbol_size = settings.fan_font_size > 1 ? settings.fan_font_size / 2 : 1;
            tft_.setTextSize(symbol_size);
            tft_.print("% ");
        }
    }

    if (last_pm25_ != state.pm25_smooth) {
        last_pm25_ = state.pm25_smooth;
        int pm_x = settings.pm_x + kPmBlockXOffset;
        if (pm_x < 0) {
            pm_x = 0;
        }
        if (pm_x >= tft_.width()) {
            pm_x = tft_.width() - 1;
        }
        drawPmLine(tft_, pm_x, settings.pm_y, state.pm25_smooth, "2.5", settings);
    }

    if (last_pm10_ != state.pm10_smooth) {
        last_pm10_ = state.pm10_smooth;
        int pm_x = settings.pm_x + kPmBlockXOffset;
        if (pm_x < 0) {
            pm_x = 0;
        }
        if (pm_x >= tft_.width()) {
            pm_x = tft_.width() - 1;
        }
        drawPmLine(tft_, pm_x, pm10_y, state.pm10_smooth, "10", settings);
    }
}
