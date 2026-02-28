#include "app_controller.h"

namespace {
const int PIN_FAN = D0;
const int PIN_SENSOR_TX = A6;
const int PIN_DISP_BL = A1;
const int PIN_DISP_AUX_EN = D6;
const int BTN_UP = D1;
const int BTN_DOWN = D2;
const int BTN_EXTRA = D3;
const int BTN_POWER = D4;

const int TFT_CS = A2;
const int TFT_DC = A0;
const int TFT_RST = -1;

const uint32_t kReportIntervalMs = 5000;
const uint32_t kHealthPublishIntervalMs = 30000;
const uint32_t kDisplayReinitDelayMs = 2500;
}  // namespace

AppController::AppController()
    : fan_(PIN_FAN),
      display_(TFT_CS, TFT_DC, TFT_RST, PIN_DISP_BL),
      buttons_(BTN_UP, BTN_DOWN, BTN_EXTRA, BTN_POWER),
      sensor_(PIN_SENSOR_TX),
      web_(80),
      q_head_(0),
      q_tail_(0),
      setup_mode_(false),
      display_reinit_pending_(false),
      display_reinit_at_ms_(0),
      last_applied_fan_(-1),
      last_applied_lights_(false),
      force_apply_lights_(false),
      last_report_ms_(0),
      last_health_publish_ms_(0),
      last_sensor_sample_ms_(0),
      wifi_ip_visible_until_ms_(0) {}

void AppController::init() {
    Serial.begin(9600);
    RGB.control(true);
    // Keep this legacy line deterministic before TFT init; floating here can cause white screen on assembled units.
    pinMode(PIN_DISP_AUX_EN, OUTPUT);
    digitalWrite(PIN_DISP_AUX_EN, LOW);

    initDeviceState(state_, millis());
    settings_store_.loadOrInitialize(settings_);

    fan_.init();
    display_.init();
    display_reinit_pending_ = true;
    display_reinit_at_ms_ = millis() + kDisplayReinitDelayMs;
    buttons_.init();
    sensor_.init();

    web_.init(&settings_, &settings_store_, &state_);
    web_.setCommandSink(enqueueFromModule, this);
    web_.setCommandBatchSink(enqueueBatchFromModule);

    mqtt_.setCommandSink(enqueueFromModule, this);

    if (strlen(settings_.wifi_ssid) == 0) {
        setup_mode_ = true;
        RGB.color(0, 0, 255);
        display_.setLights(true);
        wifi_.beginSetupMode();
        display_.renderSetupScreen(wifi_.softApSsid(), "192.168.0.1");
    } else {
        setup_mode_ = false;
        RGB.color(255, 100, 0);
        display_.setLights(true);
        wifi_.beginNormalMode(settings_);
        web_.begin();
        mqtt_.configure(settings_);
    }

    applyOutputs();
}

void AppController::tick() {
    uint32_t now_ms = millis();

    tickDisplay(now_ms);
    tickInput(now_ms);
    tickSensor(now_ms);
    tickNetwork(now_ms);
    tickReport(now_ms);
    tickHealthPublish(now_ms);

    if (state_.dirty_publish) {
        queueStatePublish();
        state_.dirty_publish = false;
    }

    applyOutputs();
}

WebConfigServer* AppController::webServer() {
    return &web_;
}

void AppController::tickDisplay(uint32_t now_ms) {
    if (display_reinit_pending_ && now_ms >= display_reinit_at_ms_) {
        display_reinit_pending_ = false;
        display_.reinitialize();
        if (state_.lights_on) {
            if (setup_mode_) {
                display_.renderSetupScreen(wifi_.softApSsid(), "192.168.0.1");
            } else {
                display_.renderConnectingScreen();
            }
        }
        state_.dirty_display = true;
    }
}

void AppController::tickInput(uint32_t now_ms) {
    processCommands();

    Command button_cmd;
    if (buttons_.poll(button_cmd, now_ms)) {
        if (enqueueCommand(button_cmd)) {
            // Apply button long-press actions immediately to avoid extra user wait.
            processCommands();
        }
    }
}

void AppController::tickSensor(uint32_t now_ms) {
    sensor_.tick(now_ms, state_);

    if (state_.last_sensor_packet_ms != last_sensor_sample_ms_) {
        last_sensor_sample_ms_ = state_.last_sensor_packet_ms;
        pm25_avg_.add(state_.pm25_raw);
        pm10_avg_.add(state_.pm10_raw);
    }
}

void AppController::tickNetwork(uint32_t now_ms) {
    bool prev_wifi_enabled = state_.wifi_enabled;
    bool prev_wifi_ready = state_.wifi_ready;
    bool prev_wifi_ip_visible = state_.wifi_ip_visible;

    if (state_.wifi_enabled && !WiFi.listening()) {
        web_.tick();
    }

    if (state_.wifi_enabled) {
        wifi_.tick(now_ms, state_);
    } else {
        state_.wifi_ready = false;
        state_.mqtt_connected = false;
    }

    if (!setup_mode_) {
        if (state_.wifi_enabled) {
            mqtt_.tick(now_ms, state_);
        } else {
            state_.mqtt_connected = false;
        }
    }

    if (!prev_wifi_ready && state_.wifi_ready) {
        // Show IP for a short period after Wi-Fi becomes ready.
        wifi_ip_visible_until_ms_ = now_ms + 3000;
    }
    if (prev_wifi_ready && !state_.wifi_ready) {
        // Show disconnected status briefly after link loss.
        wifi_ip_visible_until_ms_ = now_ms + 3000;
    }
    if (wifi_ip_visible_until_ms_ > 0 && now_ms >= wifi_ip_visible_until_ms_) {
        wifi_ip_visible_until_ms_ = 0;
    }
    state_.wifi_ip_visible = (wifi_ip_visible_until_ms_ > 0);

    if (state_.wifi_enabled != prev_wifi_enabled || state_.wifi_ready != prev_wifi_ready ||
        state_.wifi_ip_visible != prev_wifi_ip_visible) {
        state_.dirty_display = true;
        state_.dirty_publish = true;
    }
}

void AppController::tickReport(uint32_t now_ms) {
    if (now_ms - last_report_ms_ >= kReportIntervalMs) {
        last_report_ms_ = now_ms;
        state_.pm25_smooth = pm25_avg_.average();
        state_.pm10_smooth = pm10_avg_.average();
        state_.dirty_display = true;
        state_.dirty_publish = true;
    }
}

void AppController::tickHealthPublish(uint32_t now_ms) {
    if (now_ms - last_health_publish_ms_ >= kHealthPublishIntervalMs) {
        last_health_publish_ms_ = now_ms;
        state_.mqtt_publish_drop_count = mqtt_.publishDropCount();
        mqtt_.enqueueStatePublish("health/uptime_s", (now_ms - state_.boot_ms) / 1000);
        mqtt_.enqueueStatePublish("health/wifi_reconnect_count", state_.wifi_reconnect_count);
        mqtt_.enqueueStatePublish("health/mqtt_reconnect_count", state_.mqtt_reconnect_count);
        mqtt_.enqueueStatePublish("health/sensor_parse_errors", state_.sensor_parse_errors);
        mqtt_.enqueueStatePublish("health/command_drop_button_count", state_.command_drop_button_count);
        mqtt_.enqueueStatePublish("health/command_drop_mqtt_count", state_.command_drop_mqtt_count);
        mqtt_.enqueueStatePublish("health/command_drop_web_count", state_.command_drop_web_count);
        mqtt_.enqueueStatePublish("health/mqtt_publish_drop_count", state_.mqtt_publish_drop_count);
    }
}

bool AppController::enqueueCommand(const Command& cmd) {
    uint8_t next = static_cast<uint8_t>((q_tail_ + 1) % kCommandQueueSize);
    if (next == q_head_) {
        recordCommandDrop(cmd.source);
        return false;
    }
    queue_[q_tail_] = cmd;
    q_tail_ = next;
    return true;
}

bool AppController::enqueueCommands(const Command* cmds, size_t count) {
    if (cmds == nullptr || count == 0) {
        return false;
    }
    if (count > queueFreeSlots()) {
        for (size_t i = 0; i < count; ++i) {
            recordCommandDrop(cmds[i].source);
        }
        return false;
    }

    for (size_t i = 0; i < count; ++i) {
        queue_[q_tail_] = cmds[i];
        q_tail_ = static_cast<uint8_t>((q_tail_ + 1) % kCommandQueueSize);
    }
    return true;
}

bool AppController::enqueueFromModule(const Command& cmd, void* ctx) {
    if (ctx == nullptr) {
        return false;
    }
    AppController* app = static_cast<AppController*>(ctx);
    return app->enqueueCommand(cmd);
}

bool AppController::enqueueBatchFromModule(const Command* cmds, size_t count, void* ctx) {
    if (ctx == nullptr) {
        return false;
    }
    AppController* app = static_cast<AppController*>(ctx);
    return app->enqueueCommands(cmds, count);
}

uint8_t AppController::queueFreeSlots() const {
    uint8_t used = static_cast<uint8_t>((q_tail_ + kCommandQueueSize - q_head_) % kCommandQueueSize);
    return static_cast<uint8_t>((kCommandQueueSize - 1) - used);
}

void AppController::recordCommandDrop(CommandSource source, uint32_t count) {
    if (count == 0) {
        return;
    }

    if (source == CommandSource::Button) {
        state_.command_drop_button_count += count;
        return;
    }
    if (source == CommandSource::Mqtt) {
        state_.command_drop_mqtt_count += count;
        return;
    }
    if (source == CommandSource::Web) {
        state_.command_drop_web_count += count;
    }
}

bool AppController::popCommand(Command& out) {
    if (q_head_ == q_tail_) {
        return false;
    }
    out = queue_[q_head_];
    q_head_ = static_cast<uint8_t>((q_head_ + 1) % kCommandQueueSize);
    return true;
}

void AppController::processCommands() {
    Command cmd;
    while (popCommand(cmd)) {
        applyCommand(cmd);
    }
}

void AppController::applyCommand(const Command& cmd) {
    if (cmd.type == CommandType::SetScreenLight) {
        bool on = (cmd.value != 0);
        display_.setScreenLight(on);
        state_.screen_light_on = on;
        return;
    }
    if (cmd.type == CommandType::ToggleWifi) {
        state_.wifi_enabled = !state_.wifi_enabled;
        if (state_.wifi_enabled) {
            if (setup_mode_) {
                wifi_.beginSetupMode();
            } else {
                wifi_.beginNormalMode(settings_);
                web_.begin();
            }
        } else {
            WiFi.disconnect();
            WiFi.off();
            state_.wifi_ready = false;
            state_.wifi_ip_visible = true;
            state_.mqtt_connected = false;
            wifi_ip_visible_until_ms_ = millis() + 3000;
        }
        state_.dirty_display = true;
        state_.dirty_publish = true;
        return;
    }
    if (cmd.type == CommandType::ResetWifiSettings) {
        settings_.wifi_ssid[0] = '\0';
        settings_.wifi_pass[0] = '\0';
        settings_store_.sanitize(settings_);
        settings_store_.save(settings_);

        WiFi.disconnect();
        WiFi.clearCredentials();
        WiFi.off();

        System.reset();
        return;
    }
    bool applied = command_router_.apply(cmd, state_);
    if (applied && cmd.type == CommandType::SetLights) {
        force_apply_lights_ = true;
    }
}

void AppController::applyOutputs() {
    if (state_.fan_percent != last_applied_fan_) {
        fan_.setPercent(state_.fan_percent);
        last_applied_fan_ = state_.fan_percent;
    }

    if (force_apply_lights_ || state_.lights_on != last_applied_lights_) {
        display_.setLights(state_.lights_on);
        state_.screen_light_on = state_.lights_on;
        last_applied_lights_ = state_.lights_on;
        force_apply_lights_ = false;
    }

    if (state_.dirty_display) {
        if (setup_mode_ || (state_.wifi_enabled && WiFi.listening())) {
            display_.renderSetupScreen(wifi_.softApSsid(), "192.168.0.1");
        } else {
            display_.render(state_, settings_);
        }
        state_.dirty_display = false;
    }
}

void AppController::queueStatePublish() {
    int fan_pwm = map(state_.fan_percent, 0, 100, 0, 255);

    mqtt_.enqueueStatePublish("state/fan_percent", state_.fan_percent);
    mqtt_.enqueueStatePublish("state/fan_pwm", fan_pwm);
    mqtt_.enqueueStatePublish("state/lights", state_.lights_on ? 1 : 0);
    mqtt_.enqueueStatePublish("sensor/pm25", state_.pm25_smooth);
    mqtt_.enqueueStatePublish("sensor/pm10", state_.pm10_smooth);
}
