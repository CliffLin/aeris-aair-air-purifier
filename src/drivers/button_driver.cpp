#include "button_driver.h"

namespace {
const uint32_t kDebounceMs = 40;
const uint32_t kMiddleLongPressMs = 3000;
const uint32_t kPowerResetLongPressMs = 8000;
}

ButtonDriver::ButtonDriver(int btn_up, int btn_down, int btn_extra, int btn_power) {
    entries_[0] = {btn_up, LOW, LOW, 0, 0, false};
    entries_[1] = {btn_down, LOW, LOW, 0, 0, false};
    entries_[2] = {btn_extra, LOW, LOW, 0, 0, false};
    entries_[3] = {btn_power, LOW, LOW, 0, 0, false};
}

void ButtonDriver::init() {
    for (int i = 0; i < kButtonCount; ++i) {
        pinMode(entries_[i].pin, INPUT_PULLDOWN);
        entries_[i].idle_state = digitalRead(entries_[i].pin);
        entries_[i].last_state = entries_[i].idle_state;
        entries_[i].last_change_ms = millis();
        entries_[i].press_start_ms = entries_[i].last_change_ms;
        entries_[i].long_press_fired = false;
    }
}

bool ButtonDriver::poll(Command& out, uint32_t now_ms) {
    for (int i = 0; i < kButtonCount; ++i) {
        int reading = digitalRead(entries_[i].pin);
        bool raw_pressed = (reading != entries_[i].idle_state);
        bool debounced_pressed = (entries_[i].last_state != entries_[i].idle_state);

        if ((i == 2 || i == 3) &&
            raw_pressed &&
            !debounced_pressed &&
            entries_[i].press_start_ms <= entries_[i].last_change_ms) {
            // Start long-press timer on first observed pressed sample.
            entries_[i].press_start_ms = now_ms;
        }

        if (i == 2 &&
            raw_pressed &&
            !entries_[i].long_press_fired &&
            now_ms - entries_[i].press_start_ms >= kMiddleLongPressMs) {
            entries_[i].long_press_fired = true;
            out.source = CommandSource::Button;
            out.type = CommandType::ToggleWifi;
            out.value = 0;
            return true;
        }

        if (i == 3 &&
            raw_pressed &&
            !entries_[i].long_press_fired &&
            now_ms - entries_[i].press_start_ms >= kPowerResetLongPressMs) {
            entries_[i].long_press_fired = true;
            out.source = CommandSource::Button;
            out.type = CommandType::ResetWifiSettings;
            out.value = 0;
            return true;
        }

        if (reading != entries_[i].last_state) {
            if (now_ms - entries_[i].last_change_ms < kDebounceMs) {
                continue;
            }
            entries_[i].last_change_ms = now_ms;
            entries_[i].last_state = reading;
            bool pressed_now = (reading != entries_[i].idle_state);

            if (i == 2) {
                if (pressed_now) {
                    entries_[i].press_start_ms = now_ms;
                    entries_[i].long_press_fired = false;
                    continue;
                }
                if (!entries_[i].long_press_fired) {
                    out.source = CommandSource::Button;
                    out.type = CommandType::ToggleLights;
                    out.value = 0;
                    return true;
                }
                continue;
            }

            if (i == 3) {
                if (pressed_now) {
                    entries_[i].press_start_ms = now_ms;
                    entries_[i].long_press_fired = false;
                    continue;
                }
                if (!entries_[i].long_press_fired) {
                    out.source = CommandSource::Button;
                    out.type = CommandType::TogglePower;
                    out.value = 0;
                    return true;
                }
                continue;
            }

            if (!pressed_now) {
                continue;
            }

            out.source = CommandSource::Button;
            out.value = 0;
            if (i == 0) {
                out.type = CommandType::AdjustFanPercent;
                out.value = 5;
            } else if (i == 1) {
                out.type = CommandType::AdjustFanPercent;
                out.value = -5;
            } else {
                continue;
            }
            return true;
        }
    }
    return false;
}
