#pragma once

#include "Particle.h"
#include "../app/command.h"

class ButtonDriver {
public:
    static const int kButtonCount = 4;

    ButtonDriver(int btn_up, int btn_down, int btn_extra, int btn_power);

    void init();
    bool poll(Command& out, uint32_t now_ms);

private:
    struct Entry {
        int pin;
        int idle_state;
        int last_state;
        uint32_t last_change_ms;
        uint32_t press_start_ms;
        bool long_press_fired;
    };

    Entry entries_[kButtonCount];
};
