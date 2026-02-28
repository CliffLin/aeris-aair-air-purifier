#pragma once

#include "Particle.h"
#include <MQTT.h>

#include "../app/command.h"
#include "../core/device_state.h"
#include "../core/settings_store.h"

class MqttClient {
public:
    typedef bool (*CommandSink)(const Command& cmd, void* ctx);

    MqttClient();
    ~MqttClient();

    void configure(const SettingsV2& settings);
    void setCommandSink(CommandSink sink, void* ctx);
    void tick(uint32_t now_ms, DeviceState& state);

    void enqueueStatePublish(const char* key, int value);
    uint32_t publishDropCount() const;

private:
    struct PublishMessage {
        char topic[128];
        char payload[24];
    };

    static const uint8_t kQueueSize = 10;

    MQTT* client_;
    SettingsV2 settings_;
    char root_[96];

    CommandSink sink_;
    void* sink_ctx_;

    PublishMessage queue_[kQueueSize];
    uint8_t q_head_;
    uint8_t q_tail_;
    uint32_t last_reconnect_attempt_ms_;
    uint32_t last_publish_ms_;
    uint32_t publish_drop_count_;
    uint8_t connect_fail_streak_;
    uint32_t suspended_until_ms_;
    bool enabled_;

    void subscribeTopics();
    void onMessage(char* topic, uint8_t* payload, unsigned int length);
    bool parseCommand(const char* topic, const char* payload, Command& out) const;

    bool queuePush(const char* topic, const char* payload);
    bool queuePop(PublishMessage& out);

    static void onRouterMessage(void* ctx, char* topic, uint8_t* payload, unsigned int length);
};
