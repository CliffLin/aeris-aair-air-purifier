#include "fan_driver.h"

void FanDriver::init() {
    pinMode(pin_fan_, OUTPUT);
}

void FanDriver::setPercent(int fan_percent) {
    if (fan_percent < 0) {
        fan_percent = 0;
    }
    if (fan_percent > 100) {
        fan_percent = 100;
    }
    analogWrite(pin_fan_, map(fan_percent, 0, 100, 0, 255));
}
