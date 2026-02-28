#include "mqtt_client.h"

#include "../util/parse_int.h"
#include "../util/string_safety.h"
#include "../util/topic_validation.h"

#include <stdio.h>
#include <string.h>

namespace {
const uint32_t kReconnectBaseIntervalMs = 3000;
const uint8_t kReconnectBackoffMaxShift = 5;
const uint8_t kSuspendAfterFailStreak = 4;
const uint32_t kSuspendDurationMs = 300000;  // 5 min

struct MqttCallbackRouter {
    void (*handler)(void* ctx, char* topic, uint8_t* payload, unsigned int length);
    void* ctx;
};

MqttCallbackRouter g_mqtt_router = {nullptr, nullptr};

void mqttCallbackBridge(char* topic, uint8_t* payload, unsigned int length) {
    if (g_mqtt_router.handler == nullptr || g_mqtt_router.ctx == nullptr) {
        return;
    }
    g_mqtt_router.handler(g_mqtt_router.ctx, topic, payload, length);
}
}  // namespace

MqttClient::MqttClient()
    : client_(nullptr),
      sink_(nullptr),
      sink_ctx_(nullptr),
      q_head_(0),
      q_tail_(0),
      last_reconnect_attempt_ms_(0),
      last_publish_ms_(0),
      publish_drop_count_(0),
      connect_fail_streak_(0),
      suspended_until_ms_(0),
      enabled_(false) {
    memset(&settings_, 0, sizeof(settings_));
    memset(root_, 0, sizeof(root_));
    g_mqtt_router.handler = &MqttClient::onRouterMessage;
    g_mqtt_router.ctx = this;
}

MqttClient::~MqttClient() {
    if (client_ != nullptr) {
        delete client_;
        client_ = nullptr;
    }
    if (g_mqtt_router.ctx == this) {
        g_mqtt_router.handler = nullptr;
        g_mqtt_router.ctx = nullptr;
    }
}

void MqttClient::configure(const SettingsV2& settings) {
    settings_ = settings;
    connect_fail_streak_ = 0;
    suspended_until_ms_ = 0;
    last_reconnect_attempt_ms_ = 0;
    q_head_ = 0;
    q_tail_ = 0;

    if (!isDeviceIdTopicSafe(settings_.device_id, sizeof(settings_.device_id) - 1)) {
        char normalized_id[sizeof(settings_.device_id)] = {0};
        normalizeDeviceId(settings_.device_id, normalized_id, sizeof(normalized_id));
        safeCopy(settings_.device_id, sizeof(settings_.device_id), normalized_id);
    }
    if (isTopicRootSafe(settings_.mqtt_topic_root, sizeof(settings_.mqtt_topic_root))) {
        safeCopy(root_, sizeof(root_), settings_.mqtt_topic_root);
    } else {
        buildDefaultTopicRoot(settings_.device_id, root_, sizeof(root_));
    }

    if (client_ != nullptr) {
        delete client_;
        client_ = nullptr;
    }

    enabled_ = (settings_.mqtt_enabled != 0) && (strlen(settings_.mqtt_host) > 0);
    if (enabled_) {
        client_ = new MQTT(settings_.mqtt_host, settings_.mqtt_port, mqttCallbackBridge);
    }
}

void MqttClient::setCommandSink(CommandSink sink, void* ctx) {
    sink_ = sink;
    sink_ctx_ = ctx;
}

void MqttClient::tick(uint32_t now_ms, DeviceState& state) {
    if (!enabled_ || client_ == nullptr || !state.wifi_ready) {
        state.mqtt_connected = false;
        return;
    }

    if (!client_->isConnected()) {
        state.mqtt_connected = false;

        if (suspended_until_ms_ != 0 &&
            static_cast<int32_t>(now_ms - suspended_until_ms_) < 0) {
            return;
        }

        uint32_t retry_interval_ms = kReconnectBaseIntervalMs;
        uint8_t backoff_shift = (connect_fail_streak_ > kReconnectBackoffMaxShift)
                                    ? kReconnectBackoffMaxShift
                                    : connect_fail_streak_;
        retry_interval_ms <<= backoff_shift;

        if (now_ms - last_reconnect_attempt_ms_ >= retry_interval_ms) {
            last_reconnect_attempt_ms_ = now_ms;
            bool ok = client_->connect(settings_.device_id, settings_.mqtt_user, settings_.mqtt_pass);
            if (ok && client_->isConnected()) {
                connect_fail_streak_ = 0;
                suspended_until_ms_ = 0;
                subscribeTopics();
            } else {
                if (connect_fail_streak_ < 255) {
                    connect_fail_streak_ += 1;
                }
                if (connect_fail_streak_ >= kSuspendAfterFailStreak) {
                    suspended_until_ms_ = now_ms + kSuspendDurationMs;
                    connect_fail_streak_ = 0;
                }
            }
            state.mqtt_reconnect_count += 1;
        }
        return;
    }

    state.mqtt_connected = true;
    connect_fail_streak_ = 0;
    suspended_until_ms_ = 0;
    client_->loop();

    if (now_ms - last_publish_ms_ < 25) {
        return;
    }

    PublishMessage msg;
    if (queuePop(msg)) {
        client_->publish(msg.topic, msg.payload);
        last_publish_ms_ = now_ms;
    }
}

void MqttClient::enqueueStatePublish(const char* key, int value) {
    if (!enabled_) {
        return;
    }
    char topic[128];
    char payload[24];
    snprintf(topic, sizeof(topic), "%s/%s", root_, key);
    snprintf(payload, sizeof(payload), "%d", value);
    queuePush(topic, payload);
}

uint32_t MqttClient::publishDropCount() const {
    return publish_drop_count_;
}

void MqttClient::subscribeTopics() {
    char topic[128];

    snprintf(topic, sizeof(topic), "%s/cmd/fan_percent", root_);
    client_->subscribe(topic);

    snprintf(topic, sizeof(topic), "%s/cmd/lights", root_);
    client_->subscribe(topic);

    snprintf(topic, sizeof(topic), "%s/cmd/screen_light", root_);
    client_->subscribe(topic);
}

void MqttClient::onMessage(char* topic, uint8_t* payload, unsigned int length) {
    if (sink_ == nullptr || topic == nullptr || payload == nullptr || length == 0) {
        return;
    }

    char payload_buf[48];
    unsigned int copy_len = length;
    if (copy_len >= sizeof(payload_buf)) {
        copy_len = sizeof(payload_buf) - 1;
    }
    memcpy(payload_buf, payload, copy_len);
    payload_buf[copy_len] = '\0';

    Command cmd;
    if (!parseCommand(topic, payload_buf, cmd)) {
        return;
    }
    sink_(cmd, sink_ctx_);
}

bool MqttClient::parseCommand(const char* topic, const char* payload, Command& out) const {
    char fan_topic[128];
    char lights_topic[128];
    char screen_light_topic[128];
    snprintf(fan_topic, sizeof(fan_topic), "%s/cmd/fan_percent", root_);
    snprintf(lights_topic, sizeof(lights_topic), "%s/cmd/lights", root_);
    snprintf(screen_light_topic, sizeof(screen_light_topic), "%s/cmd/screen_light", root_);

    out.source = CommandSource::Mqtt;
    out.value = 0;

    if (strcmp(topic, fan_topic) == 0) {
        int fan_percent = 0;
        if (!parseIntStrict(payload, 0, 100, fan_percent)) {
            return false;
        }
        out.type = CommandType::SetFanPercent;
        out.value = fan_percent;
        return true;
    }
    if (strcmp(topic, lights_topic) == 0) {
        int lights = 0;
        if (!parseIntStrict(payload, 0, 1, lights)) {
            return false;
        }
        out.type = CommandType::SetLights;
        out.value = lights;
        return true;
    }
    if (strcmp(topic, screen_light_topic) == 0) {
        int screen_light = 0;
        if (!parseIntStrict(payload, 0, 1, screen_light)) {
            return false;
        }
        out.type = CommandType::SetScreenLight;
        out.value = screen_light;
        return true;
    }

    return false;
}

bool MqttClient::queuePush(const char* topic, const char* payload) {
    uint8_t next = static_cast<uint8_t>((q_tail_ + 1) % kQueueSize);
    if (next == q_head_) {
        publish_drop_count_ += 1;
        return false;
    }

    strncpy(queue_[q_tail_].topic, topic, sizeof(queue_[q_tail_].topic) - 1);
    queue_[q_tail_].topic[sizeof(queue_[q_tail_].topic) - 1] = '\0';
    strncpy(queue_[q_tail_].payload, payload, sizeof(queue_[q_tail_].payload) - 1);
    queue_[q_tail_].payload[sizeof(queue_[q_tail_].payload) - 1] = '\0';
    q_tail_ = next;
    return true;
}

bool MqttClient::queuePop(PublishMessage& out) {
    if (q_head_ == q_tail_) {
        return false;
    }

    out = queue_[q_head_];
    q_head_ = static_cast<uint8_t>((q_head_ + 1) % kQueueSize);
    return true;
}

void MqttClient::onRouterMessage(void* ctx, char* topic, uint8_t* payload, unsigned int length) {
    if (ctx == nullptr) {
        return;
    }
    MqttClient* client = static_cast<MqttClient*>(ctx);
    client->onMessage(topic, payload, length);
}
