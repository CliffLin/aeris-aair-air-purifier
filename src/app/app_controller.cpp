#include "app_controller.h"

#include "../util/topic_validation.h"

namespace {
const int PIN_FAN = D0;
const int PIN_SENSOR_TX = A6;
const int PIN_DISP_BL = A1;
// Ring/indicator shift register (stock protocol): D5 data, D6 clock, D7 latch,
// A4 boot handshake. D6 was previously mislabeled as a display aux line.
const int PIN_RING_DATA = D5;
const int PIN_RING_CLK = D6;
const int PIN_RING_LATCH = D7;
const int PIN_RING_HANDSHAKE = A4;
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

// Filter countdown, stock semantics: wall-clock. Decremented and persisted
// every 10 min so a restart forfeits at most that much; the emulated EEPROM
// behind the settings block absorbs the write rate for decades.
const int kEepromAddrFilter = 512;
const uint32_t kFilterMagic = 0x46494C54UL;  // 'FILT'
const uint32_t kFilterTickMinutes = 10;
const uint32_t kFilterTickMs = kFilterTickMinutes * 60UL * 1000UL;

struct FilterRecord {
    uint32_t magic;
    uint32_t minutes;
};
}  // namespace

AppController::AppController()
    : fan_(PIN_FAN),
      display_(TFT_CS, TFT_DC, TFT_RST, PIN_DISP_BL),
      buttons_(BTN_UP, BTN_DOWN, BTN_EXTRA, BTN_POWER),
      key_lights_(PIN_RING_DATA, PIN_RING_CLK, PIN_RING_LATCH, PIN_RING_HANDSHAKE),
      sensor_(PIN_SENSOR_TX),
      key_light_timer_(1, &AppController::keyLightTimerTick, *this),
      web_(80),
      q_head_(0),
      q_tail_(0),
      setup_mode_(false),
      setup_web_started_(false),
      serial_prov_len_(0),
      display_reinit_pending_(false),
      display_reinit_at_ms_(0),
      last_applied_fan_(-1),
      last_applied_lights_(false),
      last_applied_status_led_(false),
      force_apply_lights_(false),
      last_filter_decrement_ms_(0),
      last_report_ms_(0),
      last_health_publish_ms_(0),
      last_sensor_sample_ms_(0),
      wifi_ip_visible_until_ms_(0) {}

void AppController::init() {
    Serial.begin(9600);
    RGB.control(true);
    // Bring the ring register up deterministically before TFT init; unclocked
    // it free-runs with random power-up contents (constant white glow).
    key_lights_.init();
    key_light_timer_.start();

    initDeviceState(state_, millis());
    settings_store_.loadOrInitialize(settings_);
    loadFilterState();

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
        RGB.color(0, 0, 0);
        display_.setLights(true);
        wifi_.beginNormalMode(settings_);
        web_.begin();
        mqtt_.configure(settings_);
    }

    applyOutputs();
}

void AppController::tick() {
    uint32_t now_ms = millis();

    if (setup_mode_) {
        tickSerialProvision();
    }
    tickDisplay(now_ms);
    tickInput(now_ms);
    tickSensor(now_ms);
    tickNetwork(now_ms);
    tickReport(now_ms);
    tickHealthPublish(now_ms);
    tickFilter(now_ms);

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

// Setup-mode serial provisioning — the deterministic fallback when the SoftAP
// TCP stack is dead. One line, tab-separated:
//   PROV\t<ssid>\t<wifi_pass>\t<mqtt_host>\t<mqtt_port>\t<mqtt_user>\t<mqtt_pass>\t<topic_root>\t<device_id>\n
// Also answers "IP?" with the current local IP. Listening mode's own console
// reads the same USB serial and would eat part of the line, so exit it first
// (system `x`, not `w` — `w` starts the Wi-Fi wizard) and refuse to parse until
// it has actually gone.
void AppController::tickSerialProvision() {
    if (WiFi.listening()) {
        // Whatever we half-read before the console took the port would otherwise
        // be prepended to the first line we accept after it lets go.
        serial_prov_len_ = 0;
        return;
    }

    while (Serial.available() > 0) {
        char c = static_cast<char>(Serial.read());
        if (c == '\r') {
            continue;
        }
        if (c != '\n') {
            if (serial_prov_len_ < sizeof(serial_prov_buf_) - 1) {
                serial_prov_buf_[serial_prov_len_++] = c;
            }
            continue;
        }
        serial_prov_buf_[serial_prov_len_] = '\0';
        serial_prov_len_ = 0;

        if (strcmp(serial_prov_buf_, "IP?") == 0) {
            Serial.println(WiFi.localIP());
            continue;
        }
        if (strncmp(serial_prov_buf_, "PROV\t", 5) != 0) {
            continue;
        }

        char* fields[8] = {nullptr};
        int n = 0;
        char* p = serial_prov_buf_ + 5;
        fields[n++] = p;
        while (n < 8 && (p = strchr(p, '\t')) != nullptr) {
            *p++ = '\0';
            fields[n++] = p;
        }
        if (n < 8) {
            Serial.println("PROV ERR fields");
            continue;
        }
        char* end = nullptr;
        long port = strtol(fields[3], &end, 10);
        if (end == nullptr || *end != '\0' || port < 1 || port > 65535) {
            Serial.println("PROV ERR port");
            continue;
        }

        if (fields[0][0] == '\0') {
            Serial.println("PROV ERR ssid");
            continue;
        }
        // An empty broker host leaves mqtt_enabled set but MqttClient disabled:
        // the same provisions-cleanly-never-connects unit the checks exist for.
        if (fields[2][0] == '\0') {
            Serial.println("PROV ERR host");
            continue;
        }
        // Silently truncating a passphrase or a broker host produces a unit that
        // provisions cleanly and then never connects, so refuse instead.
        const size_t limits[8] = {
            sizeof(settings_.wifi_ssid), sizeof(settings_.wifi_pass),
            sizeof(settings_.mqtt_host), 0,
            sizeof(settings_.mqtt_user), sizeof(settings_.mqtt_pass),
            sizeof(settings_.mqtt_topic_root), sizeof(settings_.device_id),
        };
        bool too_long = false;
        for (int f = 0; f < 8; ++f) {
            if (limits[f] != 0 && strlen(fields[f]) >= limits[f]) {
                too_long = true;
            }
        }
        if (too_long) {
            Serial.println("PROV ERR too long");
            continue;
        }
        // sanitize() rewrites these rather than failing, so an unsafe topic root
        // would answer PROV OK and put the unit on the default root instead.
        if (!isDeviceIdTopicSafe(fields[7], sizeof(settings_.device_id) - 1)) {
            Serial.println("PROV ERR device_id");
            continue;
        }
        if (!isTopicRootSafe(fields[6], sizeof(settings_.mqtt_topic_root))) {
            Serial.println("PROV ERR topic_root");
            continue;
        }

        snprintf(settings_.wifi_ssid, sizeof(settings_.wifi_ssid), "%s", fields[0]);
        snprintf(settings_.wifi_pass, sizeof(settings_.wifi_pass), "%s", fields[1]);
        snprintf(settings_.mqtt_host, sizeof(settings_.mqtt_host), "%s", fields[2]);
        settings_.mqtt_port = static_cast<uint16_t>(port);
        snprintf(settings_.mqtt_user, sizeof(settings_.mqtt_user), "%s", fields[4]);
        snprintf(settings_.mqtt_pass, sizeof(settings_.mqtt_pass), "%s", fields[5]);
        snprintf(settings_.mqtt_topic_root, sizeof(settings_.mqtt_topic_root), "%s", fields[6]);
        snprintf(settings_.device_id, sizeof(settings_.device_id), "%s", fields[7]);
        settings_.mqtt_enabled = 1;

        settings_store_.sanitize(settings_);
        if (!settings_store_.save(settings_)) {
            Serial.println("PROV ERR save");
            continue;
        }
        Serial.println("PROV OK rebooting");
        delay(200);
        System.reset();
    }
}

void AppController::keyLightTimerTick() {
    key_lights_.tick();
}

void AppController::loadFilterState() {
    FilterRecord rec;
    EEPROM.get(kEepromAddrFilter, rec);
    state_.filter_minutes = (rec.magic == kFilterMagic) ? rec.minutes : 0;
}

void AppController::saveFilterState() {
    FilterRecord rec = {kFilterMagic, state_.filter_minutes};
    EEPROM.put(kEepromAddrFilter, rec);
}

void AppController::tickFilter(uint32_t now_ms) {
    if (now_ms - last_filter_decrement_ms_ < kFilterTickMs) {
        return;
    }
    last_filter_decrement_ms_ = now_ms;
    if (state_.filter_minutes == 0) {
        return;
    }
    state_.filter_minutes = (state_.filter_minutes > kFilterTickMinutes)
                                ? state_.filter_minutes - kFilterTickMinutes
                                : 0;
    saveFilterState();
    state_.dirty_publish = true;
}

void AppController::tickNetwork(uint32_t now_ms) {
    bool prev_wifi_enabled = state_.wifi_enabled;
    bool prev_wifi_ready = state_.wifi_ready;
    bool prev_wifi_ip_visible = state_.wifi_ip_visible;

    if (state_.wifi_enabled) {
        // The system SoftAP HTTP server can come up with its TCP listeners dead
        // while DHCP/ICMP work, so the config API is served from our own
        // TCPServer instead. It cannot run on the SoftAP interface: both
        // TCPServer::begin() and available() return early unless
        // Network.ready(), which listening mode never satisfies. Station only.
        if (setup_mode_ && !setup_web_started_ && state_.wifi_ready) {
            web_.begin();
            setup_web_started_ = true;
        }
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
    if (cmd.type == CommandType::SetRing) {
        state_.ring_pattern = static_cast<uint8_t>(cmd.value);
        state_.dirty_publish = true;
        return;
    }
    if (cmd.type == CommandType::SetRingBrightness) {
        // 0-100% mapped onto the 5 PWM duty levels.
        state_.ring_brightness = static_cast<uint8_t>((cmd.value * 4 + 50) / 100);
        state_.dirty_publish = true;
        return;
    }
    if (cmd.type == CommandType::SetRingBlink) {
        state_.ring_blink_ms = static_cast<uint16_t>(cmd.value);
        state_.dirty_publish = true;
        return;
    }
    if (cmd.type == CommandType::SetFilterDays) {
        state_.filter_minutes = static_cast<uint32_t>(cmd.value) * 1440UL;
        saveFilterState();
        state_.dirty_publish = true;
        return;
    }
    if (cmd.type == CommandType::SetStatusLed) {
        state_.status_led_on = (cmd.value != 0);
        state_.dirty_publish = true;
        return;
    }
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

    key_lights_.set_pattern(state_.ring_pattern);
    key_lights_.set_brightness(state_.ring_brightness);
    key_lights_.set_blink_ms(state_.ring_blink_ms);

    if (!setup_mode_ && state_.status_led_on != last_applied_status_led_) {
        RGB.color(state_.status_led_on ? 255 : 0, state_.status_led_on ? 100 : 0, 0);
        last_applied_status_led_ = state_.status_led_on;
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
    mqtt_.enqueueStatePublish("state/ring", state_.ring_pattern);
    mqtt_.enqueueStatePublish("state/ring_brightness", state_.ring_brightness * 25);
    mqtt_.enqueueStatePublish("state/ring_blink", state_.ring_blink_ms);
    mqtt_.enqueueStatePublish("state/status_led", state_.status_led_on ? 1 : 0);
    mqtt_.enqueueStatePublish("sensor/pm25", state_.pm25_smooth);
    mqtt_.enqueueStatePublish("sensor/pm10", state_.pm10_smooth);
    mqtt_.enqueueStatePublish("sensor/filter_minutes", state_.filter_minutes);
}
