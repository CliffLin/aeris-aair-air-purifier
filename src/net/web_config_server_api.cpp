#include "web_config_server.h"

#include "../util/parse_int.h"
#include "../util/string_safety.h"
#include "../util/topic_validation.h"

#include <stdio.h>
#include <string.h>

namespace {
void appendErrorField(char* errors, size_t size, const char* field_name) {
    if (size == 0) {
        return;
    }
    if (errors[0] != '\0') {
        strncat(errors, " ", size - strlen(errors) - 1);
    }
    strncat(errors, field_name, size - strlen(errors) - 1);
}
}  // namespace

void WebConfigServer::handleApiSettingsGet(TCPClient& client) {
    char json[832];
    int written = snprintf(
        json,
        sizeof(json),
        "{\"wifi_ssid\":\"%s\",\"mqtt_enabled\":%u,\"mqtt_host\":\"%s\",\"mqtt_port\":%u,\"mqtt_user\":\"%s\",\"device_id\":\"%s\",\"mqtt_topic_root\":\"%s\",\"fan_font_size\":%d,\"fan_x\":%d,\"fan_y\":%d,\"pm_font_size\":%d,\"pm_x\":%d,\"pm_y\":%d}",
        settings_->wifi_ssid,
        static_cast<unsigned>(settings_->mqtt_enabled),
        settings_->mqtt_host,
        settings_->mqtt_port,
        settings_->mqtt_user,
        settings_->device_id,
        settings_->mqtt_topic_root,
        settings_->fan_font_size,
        settings_->fan_x,
        settings_->fan_y,
        settings_->pm_font_size,
        settings_->pm_x,
        settings_->pm_y);

    size_t body_len = (written < 0) ? 0 : static_cast<size_t>(written);
    if (body_len >= sizeof(json)) {
        body_len = sizeof(json) - 1;
    }
    respondN(client, 200, "application/json", json, body_len);
}

void WebConfigServer::handleApiSettingsPost(TCPClient& client, const char* form_data) {
    char errors[128] = {0};
    char value[128] = {0};
    char device_id_candidate[sizeof(settings_->device_id)] = {0};
    char topic_root_candidate[sizeof(settings_->mqtt_topic_root)] = {0};

    int port_value = settings_->mqtt_port;
    int mqtt_enabled_value = settings_->mqtt_enabled ? 1 : 0;
    int fan_font_value = settings_->fan_font_size;
    int pm_font_value = settings_->pm_font_size;

    if (getParam(form_data, "mqtt_port", value, sizeof(value))) {
        if (!parseIntStrict(value, 1, 65535, port_value)) {
            appendErrorField(errors, sizeof(errors), "mqtt_port");
        }
    }
    if (getParam(form_data, "mqtt_enabled", value, sizeof(value))) {
        if (!parseIntStrict(value, 0, 1, mqtt_enabled_value)) {
            appendErrorField(errors, sizeof(errors), "mqtt_enabled");
        }
    }

    if (getParam(form_data, "fan_font_size", value, sizeof(value))) {
        if (!parseIntStrict(value, 1, 12, fan_font_value)) {
            appendErrorField(errors, sizeof(errors), "fan_font_size");
        }
    }

    if (getParam(form_data, "pm_font_size", value, sizeof(value))) {
        if (!parseIntStrict(value, 1, 12, pm_font_value)) {
            appendErrorField(errors, sizeof(errors), "pm_font_size");
        }
    }

    bool has_device_id = getParam(form_data, "device_id", device_id_candidate, sizeof(device_id_candidate));
    if (mqtt_enabled_value != 0 && has_device_id &&
        !isDeviceIdTopicSafe(device_id_candidate, sizeof(device_id_candidate) - 1)) {
        appendErrorField(errors, sizeof(errors), "device_id");
    }

    bool has_topic_root =
        getParam(form_data, "mqtt_topic_root", topic_root_candidate, sizeof(topic_root_candidate));
    if (mqtt_enabled_value != 0 && has_topic_root && topic_root_candidate[0] != '\0' &&
        !isTopicRootSafe(topic_root_candidate, sizeof(topic_root_candidate))) {
        appendErrorField(errors, sizeof(errors), "mqtt_topic_root");
    }

    if (errors[0] != '\0') {
        char body[192];
        snprintf(body,
                 sizeof(body),
                 "{\"error\":\"validation_failed\",\"fields\":\"%s\"}",
                 errors);
        respond(client, 400, "application/json", body);
        return;
    }

    if (getParam(form_data, "wifi_ssid", value, sizeof(value))) {
        safeCopy(settings_->wifi_ssid, sizeof(settings_->wifi_ssid), value);
    }
    if (getParam(form_data, "wifi_pass", value, sizeof(value))) {
        safeCopy(settings_->wifi_pass, sizeof(settings_->wifi_pass), value);
    }
    if (getParam(form_data, "mqtt_host", value, sizeof(value))) {
        safeCopy(settings_->mqtt_host, sizeof(settings_->mqtt_host), value);
    }
    if (getParam(form_data, "mqtt_user", value, sizeof(value))) {
        safeCopy(settings_->mqtt_user, sizeof(settings_->mqtt_user), value);
    }
    if (getParam(form_data, "mqtt_pass", value, sizeof(value))) {
        safeCopy(settings_->mqtt_pass, sizeof(settings_->mqtt_pass), value);
    }
    if (has_device_id) {
        safeCopy(settings_->device_id, sizeof(settings_->device_id), device_id_candidate);
    }
    if (has_topic_root) {
        safeCopy(settings_->mqtt_topic_root, sizeof(settings_->mqtt_topic_root), topic_root_candidate);
    }

    int parsed = 0;
    if (getParam(form_data, "fan_x", value, sizeof(value)) && parseIntStrict(value, -32768, 32767, parsed)) {
        settings_->fan_x = static_cast<int16_t>(parsed);
    }
    if (getParam(form_data, "fan_y", value, sizeof(value)) && parseIntStrict(value, -32768, 32767, parsed)) {
        settings_->fan_y = static_cast<int16_t>(parsed);
    }
    if (getParam(form_data, "pm_x", value, sizeof(value)) && parseIntStrict(value, -32768, 32767, parsed)) {
        settings_->pm_x = static_cast<int16_t>(parsed);
    }
    if (getParam(form_data, "pm_y", value, sizeof(value)) && parseIntStrict(value, -32768, 32767, parsed)) {
        settings_->pm_y = static_cast<int16_t>(parsed);
    }
    if (getParam(form_data, "fan_color", value, sizeof(value)) && parseIntStrict(value, 0, 65535, parsed)) {
        settings_->fan_color = static_cast<uint16_t>(parsed);
    }
    if (getParam(form_data, "pm_label_color", value, sizeof(value)) && parseIntStrict(value, 0, 65535, parsed)) {
        settings_->pm_label_color = static_cast<uint16_t>(parsed);
    }
    if (getParam(form_data, "pm_value_color", value, sizeof(value)) && parseIntStrict(value, 0, 65535, parsed)) {
        settings_->pm_value_color = static_cast<uint16_t>(parsed);
    }

    settings_->mqtt_port = static_cast<uint16_t>(port_value);
    settings_->mqtt_enabled = static_cast<uint8_t>(mqtt_enabled_value);
    settings_->fan_font_size = static_cast<int16_t>(fan_font_value);
    settings_->pm_font_size = static_cast<int16_t>(pm_font_value);

    store_->sanitize(*settings_);
    store_->save(*settings_);

    respond(client, 200, "application/json", "{\"ok\":true,\"reboot_required\":true}");
}

void WebConfigServer::handleApiStateGet(TCPClient& client) {
    if (state_ == nullptr) {
        respond(client, 500, "application/json", "{\"error\":\"state_unavailable\"}");
        return;
    }

    char json[512];
    uint32_t now_ms = millis();
    uint32_t uptime_s = (now_ms - state_->boot_ms) / 1000;
    uint32_t sensor_age_ms = now_ms - state_->last_sensor_packet_ms;
    uint32_t mqtt_enabled = (settings_ != nullptr && settings_->mqtt_enabled != 0) ? 1 : 0;

    int written = snprintf(json,
                           sizeof(json),
                           "{\"fan_percent\":%d,\"lights_on\":%d,\"screen_light_on\":%d,\"pm25\":%d,\"pm10\":%d,"
                           "\"wifi_ready\":%d,\"mqtt_connected\":%d,\"mqtt_enabled\":%lu,\"uptime_s\":%lu,"
                           "\"sensor_parse_errors\":%lu,\"sensor_age_ms\":%lu,"
                           "\"command_drop_button_count\":%lu,\"command_drop_mqtt_count\":%lu,"
                           "\"command_drop_web_count\":%lu,\"mqtt_publish_drop_count\":%lu}",
                           state_->fan_percent,
                           state_->lights_on ? 1 : 0,
                           state_->screen_light_on ? 1 : 0,
                           state_->pm25_smooth,
                           state_->pm10_smooth,
                           state_->wifi_ready ? 1 : 0,
                           state_->mqtt_connected ? 1 : 0,
                           static_cast<unsigned long>(mqtt_enabled),
                           static_cast<unsigned long>(uptime_s),
                           static_cast<unsigned long>(state_->sensor_parse_errors),
                           static_cast<unsigned long>(sensor_age_ms),
                           static_cast<unsigned long>(state_->command_drop_button_count),
                           static_cast<unsigned long>(state_->command_drop_mqtt_count),
                           static_cast<unsigned long>(state_->command_drop_web_count),
                           static_cast<unsigned long>(state_->mqtt_publish_drop_count));
    size_t body_len = (written < 0) ? 0 : static_cast<size_t>(written);
    if (body_len >= sizeof(json)) {
        body_len = sizeof(json) - 1;
    }
    respondN(client, 200, "application/json", json, body_len);
}

void WebConfigServer::handleApiControlPost(TCPClient& client, const char* form_data) {
    char errors[64] = {0};
    char value[32] = {0};

    int fan_percent = 0;
    int lights = 0;
    int screen_light = 0;

    bool has_fan = false;
    bool has_lights = false;
    bool has_screen = false;

    if (getParam(form_data, "fan_percent", value, sizeof(value))) {
        has_fan = true;
        if (!parseIntStrict(value, 0, 100, fan_percent)) {
            appendErrorField(errors, sizeof(errors), "fan_percent");
        }
    }
    if (getParam(form_data, "lights", value, sizeof(value))) {
        has_lights = true;
        if (!parseIntStrict(value, 0, 1, lights)) {
            appendErrorField(errors, sizeof(errors), "lights");
        }
    }
    if (getParam(form_data, "screen_light", value, sizeof(value))) {
        has_screen = true;
        if (!parseIntStrict(value, 0, 1, screen_light)) {
            appendErrorField(errors, sizeof(errors), "screen_light");
        }
    }

    if (!has_fan && !has_lights && !has_screen) {
        respond(client, 400, "application/json", "{\"error\":\"missing_control_fields\"}");
        return;
    }
    if (errors[0] != '\0') {
        char body[128];
        snprintf(body,
                 sizeof(body),
                 "{\"error\":\"validation_failed\",\"fields\":\"%s\"}",
                 errors);
        respond(client, 400, "application/json", body);
        return;
    }

    Command commands[3];
    size_t command_count = 0;

    if (has_fan) {
        commands[command_count].type = CommandType::SetFanPercent;
        commands[command_count].value = fan_percent;
        commands[command_count].source = CommandSource::Web;
        ++command_count;
    }
    if (has_lights) {
        commands[command_count].type = CommandType::SetLights;
        commands[command_count].value = lights;
        commands[command_count].source = CommandSource::Web;
        ++command_count;
    }
    if (has_screen) {
        commands[command_count].type = CommandType::SetScreenLight;
        commands[command_count].value = screen_light;
        commands[command_count].source = CommandSource::Web;
        ++command_count;
    }

    bool ok = false;
    if (batch_sink_ != nullptr) {
        ok = batch_sink_(commands, command_count, sink_ctx_);
    } else if (command_count == 1) {
        ok = pushCommand(commands[0].type, commands[0].value, commands[0].source);
    } else {
        respond(client, 503, "application/json", "{\"error\":\"command_batch_unavailable\"}");
        return;
    }

    if (!ok) {
        respond(client, 503, "application/json", "{\"error\":\"command_queue_full\"}");
        return;
    }
    respond(client, 200, "application/json", "{\"ok\":true}");
}

void WebConfigServer::handleApiSystemReboot(TCPClient& client) {
    pushCommand(CommandType::Reboot, 0, CommandSource::Web);
    respond(client, 200, "application/json", "{\"ok\":true,\"action\":\"reboot\"}");
}

void WebConfigServer::handleApiSystemDfu(TCPClient& client) {
    pushCommand(CommandType::EnterDfu, 0, CommandSource::Web);
    respond(client, 200, "application/json", "{\"ok\":true,\"action\":\"dfu\"}");
}

bool WebConfigServer::pushCommand(CommandType type, int value, CommandSource source) {
    if (sink_ == nullptr) {
        return false;
    }
    Command cmd;
    cmd.type = type;
    cmd.value = value;
    cmd.source = source;
    return sink_(cmd, sink_ctx_);
}
