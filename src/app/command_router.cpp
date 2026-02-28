#include "command_router.h"

namespace {
int clampPercent(int value) {
    if (value < 0) {
        return 0;
    }
    if (value > 100) {
        return 100;
    }
    return value;
}
}

bool CommandRouter::apply(const Command& cmd, DeviceState& state) {
    switch (cmd.type) {
        case CommandType::SetFanPercent: {
            int fan = clampPercent(cmd.value);
            state.fan_percent = fan;
            if (fan > 0) {
                state.saved_fan_percent = fan;
                state.lights_on = true;
            } else {
                state.lights_on = false;
            }
            break;
        }
        case CommandType::AdjustFanPercent: {
            int next = state.fan_percent + cmd.value;
            if (state.fan_percent == 0 && cmd.value > 0) {
                next = 5;
                state.lights_on = true;
            }
            next = clampPercent(next);
            state.fan_percent = next;
            if (next > 0) {
                state.saved_fan_percent = next;
            } else {
                state.lights_on = false;
            }
            break;
        }
        case CommandType::SetLights:
            state.lights_on = (cmd.value != 0);
            break;
        case CommandType::ToggleLights:
            state.lights_on = !state.lights_on;
            break;
        case CommandType::TogglePower:
            if (state.fan_percent > 0) {
                state.saved_fan_percent = state.fan_percent;
                state.fan_percent = 0;
                state.lights_on = false;
            } else {
                if (state.saved_fan_percent < 10) {
                    state.saved_fan_percent = 10;
                }
                state.fan_percent = state.saved_fan_percent;
                state.lights_on = true;
            }
            break;
        case CommandType::Reboot:
            System.reset();
            return false;
        case CommandType::EnterDfu:
            System.dfu(false);
            return false;
        default:
            return false;
    }

    state.dirty_display = true;
    state.dirty_publish = true;
    return true;
}
