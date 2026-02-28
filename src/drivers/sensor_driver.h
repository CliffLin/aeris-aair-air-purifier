#pragma once

#include "Particle.h"
#include "../core/device_state.h"

class SensorDriver {
public:
    explicit SensorDriver(int pin_sensor_tx);

    void init();
    void tick(uint32_t now_ms, DeviceState& state);

private:
    static const int kFrameSize = 32;

    int pin_sensor_tx_;

    uint8_t frame_[kFrameSize];
    int frame_index_;
    bool in_frame_;

    uint8_t header_prev_;

    uint32_t last_rx_ms_;
    uint8_t wake_step_;
    uint32_t next_wake_ms_;

    void sendRawByte(const uint8_t* data, int len);
    void parseByte(uint8_t b, DeviceState& state, uint32_t now_ms);
    bool validChecksum(const uint8_t* frame) const;
};
