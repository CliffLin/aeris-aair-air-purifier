#pragma once

#include "command.h"
#include "../core/device_state.h"

class CommandRouter {
public:
    bool apply(const Command& cmd, DeviceState& state);
};
