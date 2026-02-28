#pragma once

#include "Particle.h"

enum class CommandSource : uint8_t {
    Button = 0,
    Mqtt = 1,
    Web = 2,
};

enum class CommandType : uint8_t {
    SetFanPercent = 0,
    AdjustFanPercent,
    SetLights,
    SetScreenLight,
    ToggleLights,
    TogglePower,
    ToggleWifi,
    ResetWifiSettings,
    Reboot,
    EnterDfu,
};

struct Command {
    CommandType type;
    int value;
    CommandSource source;
};
