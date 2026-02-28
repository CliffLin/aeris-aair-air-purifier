#pragma once

#include "Particle.h"

#include "command.h"
#include "command_router.h"
#include "../core/device_state.h"
#include "../core/settings_store.h"
#include "../drivers/button_driver.h"
#include "../drivers/display_driver.h"
#include "../drivers/fan_driver.h"
#include "../drivers/sensor_driver.h"
#include "../net/mqtt_client.h"
#include "../net/web_config_server.h"
#include "../net/wifi_manager.h"
#include "../util/moving_average.h"

class AppController {
public:
    AppController();

    void init();
    void tick();
    WebConfigServer* webServer();

    bool enqueueCommand(const Command& cmd);
    bool enqueueCommands(const Command* cmds, size_t count);
    static bool enqueueFromModule(const Command& cmd, void* ctx);
    static bool enqueueBatchFromModule(const Command* cmds, size_t count, void* ctx);

private:
    static const uint8_t kCommandQueueSize = 16;

    SettingsStore settings_store_;
    SettingsV2 settings_;
    DeviceState state_;

    FanDriver fan_;
    DisplayDriver display_;
    ButtonDriver buttons_;
    SensorDriver sensor_;

    WifiManager wifi_;
    MqttClient mqtt_;
    WebConfigServer web_;
    CommandRouter command_router_;

    MovingAverage<int, 10> pm25_avg_;
    MovingAverage<int, 10> pm10_avg_;

    Command queue_[kCommandQueueSize];
    uint8_t q_head_;
    uint8_t q_tail_;

    bool setup_mode_;
    bool display_reinit_pending_;
    uint32_t display_reinit_at_ms_;
    int last_applied_fan_;
    bool last_applied_lights_;
    bool force_apply_lights_;
    uint32_t last_report_ms_;
    uint32_t last_health_publish_ms_;
    uint32_t last_sensor_sample_ms_;
    uint32_t wifi_ip_visible_until_ms_;

    void tickDisplay(uint32_t now_ms);
    void tickInput(uint32_t now_ms);
    void tickSensor(uint32_t now_ms);
    void tickNetwork(uint32_t now_ms);
    void tickReport(uint32_t now_ms);
    void tickHealthPublish(uint32_t now_ms);

    uint8_t queueFreeSlots() const;
    void recordCommandDrop(CommandSource source, uint32_t count = 1);
    bool popCommand(Command& out);
    void processCommands();
    void applyCommand(const Command& cmd);
    void applyOutputs();
    void queueStatePublish();
};

AppController& appController();
