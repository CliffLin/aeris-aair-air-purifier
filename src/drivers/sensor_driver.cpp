#include "sensor_driver.h"

namespace {
const uint8_t CMD_HPMA_START[] = {0x68, 0x01, 0x01, 0x96};
const uint8_t CMD_PMS_WAKE[] = {0x42, 0x4D, 0xE4, 0x00, 0x01, 0x01, 0x74};
const uint8_t CMD_PMS_ACTIVE[] = {0x42, 0x4D, 0xE1, 0x00, 0x01, 0x01, 0x71};
const uint32_t kSensorWatchdogMs = 10000;
const uint32_t kWakeStepGapMs = 60;
}

SensorDriver::SensorDriver(int pin_sensor_tx)
    : pin_sensor_tx_(pin_sensor_tx),
      frame_index_(0),
      in_frame_(false),
      header_prev_(0),
      last_rx_ms_(0),
      wake_step_(0),
      next_wake_ms_(0) {}

void SensorDriver::init() {
    pinMode(pin_sensor_tx_, OUTPUT);
    digitalWrite(pin_sensor_tx_, HIGH);
    Serial1.begin(9600);
    last_rx_ms_ = millis();
}

void SensorDriver::tick(uint32_t now_ms, DeviceState& state) {
    while (Serial1.available()) {
        uint8_t b = static_cast<uint8_t>(Serial1.read());
        last_rx_ms_ = now_ms;
        parseByte(b, state, now_ms);
    }

    if ((now_ms - last_rx_ms_ > kSensorWatchdogMs) && wake_step_ == 0) {
        wake_step_ = 1;
        next_wake_ms_ = now_ms;
    }

    if (wake_step_ != 0 && now_ms >= next_wake_ms_) {
        if (wake_step_ == 1) {
            sendRawByte(CMD_HPMA_START, sizeof(CMD_HPMA_START));
        } else if (wake_step_ == 2) {
            sendRawByte(CMD_PMS_WAKE, sizeof(CMD_PMS_WAKE));
        } else if (wake_step_ == 3) {
            sendRawByte(CMD_PMS_ACTIVE, sizeof(CMD_PMS_ACTIVE));
        }
        wake_step_ += 1;
        next_wake_ms_ = now_ms + kWakeStepGapMs;
        if (wake_step_ > 3) {
            wake_step_ = 0;
            last_rx_ms_ = now_ms;
        }
    }
}

void SensorDriver::parseByte(uint8_t b, DeviceState& state, uint32_t now_ms) {
    if (!in_frame_) {
        if (header_prev_ == 0x32 && b == 0x3D) {
            in_frame_ = true;
            frame_index_ = 2;
            frame_[0] = 0x32;
            frame_[1] = 0x3D;
        }
        header_prev_ = b;
        return;
    }

    if (frame_index_ < kFrameSize) {
        frame_[frame_index_++] = b;
    }

    if (frame_index_ < kFrameSize) {
        return;
    }

    if (validChecksum(frame_)) {
        state.pm25_raw = (frame_[12] << 8) + frame_[13];
        state.pm10_raw = (frame_[14] << 8) + frame_[15];
        state.last_sensor_packet_ms = now_ms;
    } else {
        state.sensor_parse_errors += 1;
    }

    in_frame_ = false;
    frame_index_ = 0;
    header_prev_ = 0;
}

bool SensorDriver::validChecksum(const uint8_t* frame) const {
    int calc = 0;
    for (int i = 0; i < 30; ++i) {
        calc += frame[i];
    }
    int sent = (frame[30] << 8) + frame[31];
    return calc == sent;
}

void SensorDriver::sendRawByte(const uint8_t* data, int len) {
    for (int i = 0; i < len; i++) {
        uint8_t b = data[i];
        digitalWrite(pin_sensor_tx_, LOW);
        delayMicroseconds(104);
        for (int j = 0; j < 8; j++) {
            digitalWrite(pin_sensor_tx_, (b & 1) ? HIGH : LOW);
            delayMicroseconds(104);
            b >>= 1;
        }
        digitalWrite(pin_sensor_tx_, HIGH);
        delayMicroseconds(104);
    }
}
