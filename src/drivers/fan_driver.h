#pragma once

#include "Particle.h"

class FanDriver {
public:
    explicit FanDriver(int pin_fan) : pin_fan_(pin_fan) {}

    void init();
    void setPercent(int fan_percent);

private:
    int pin_fan_;
};
