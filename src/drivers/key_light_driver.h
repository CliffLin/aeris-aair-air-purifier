#pragma once

#include "Particle.h"

// Key-light bank behind a bit-banged shift register on the stock board:
// D5 data, D6 clock, D7 latch, LSB-first. Bit map (verified on hardware):
// two LEDs per button — 0x03 power, 0x0C AirQ, 0x30 down, 0xC0 up; the outer
// ring glow is bleed from these through the light guide. Left unclocked the
// register free-runs with random power-up contents (the old constant glow).
//
// Dimming is software PWM: tick() must be called every 1 ms (timer thread);
// a 4-slot frame gives 5 duty levels at 250 Hz. Blink gates the whole frame
// with a millis()-based square wave. The register is only rewritten when the
// output byte actually changes.
//
// The MQTT topics and state fields keep their original `ring` name: that is
// the published interface, and the ring glow is what a user sees change.
class KeyLightDriver {
public:
    KeyLightDriver(int data_pin, int clock_pin, int latch_pin, int handshake_pin);

    void init();
    void set_pattern(uint8_t pattern);
    void set_brightness(uint8_t level);  // 0..4 (0=off, 4=full)
    void set_blink_ms(uint16_t half_period_ms);  // 0 = solid
    void tick();  // 1 ms cadence, safe from the software-timer thread

    uint8_t pattern() const;
    uint8_t brightness() const;
    uint16_t blink_ms() const;

private:
    void writeRegister(uint8_t value);

    int data_pin_;
    int clock_pin_;
    int latch_pin_;
    int handshake_pin_;
    volatile uint8_t pattern_;
    volatile uint8_t brightness_;
    volatile uint16_t blink_ms_;
    uint8_t phase_;
    uint8_t last_written_;
};
