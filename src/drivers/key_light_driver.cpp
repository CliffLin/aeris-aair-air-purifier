#include "key_light_driver.h"

KeyLightDriver::KeyLightDriver(int data_pin, int clock_pin, int latch_pin, int handshake_pin)
    : data_pin_(data_pin),
      clock_pin_(clock_pin),
      latch_pin_(latch_pin),
      handshake_pin_(handshake_pin),
      pattern_(0),
      brightness_(4),
      blink_ms_(0),
      phase_(0),
      last_written_(0xff) {}

void KeyLightDriver::init() {
    // Stock boot handshake on A4 (high-low-high pulse, then released) before
    // the register pins come up; without it the aux circuitry may stay dark
    // or ignore the first latch.
    pinMode(handshake_pin_, OUTPUT);
    digitalWrite(handshake_pin_, HIGH);
    delay(5);
    digitalWrite(handshake_pin_, LOW);
    delay(5);
    digitalWrite(handshake_pin_, HIGH);
    pinMode(handshake_pin_, INPUT);
    delay(50);

    pinMode(clock_pin_, OUTPUT);
    pinMode(data_pin_, OUTPUT);
    pinMode(latch_pin_, OUTPUT);
    digitalWrite(latch_pin_, HIGH);
    writeRegister(0);
}

void KeyLightDriver::set_pattern(uint8_t pattern) {
    pattern_ = pattern;
}

void KeyLightDriver::set_brightness(uint8_t level) {
    brightness_ = (level > 4) ? 4 : level;
}

void KeyLightDriver::set_blink_ms(uint16_t half_period_ms) {
    blink_ms_ = half_period_ms;
}

void KeyLightDriver::tick() {
    bool visible = true;
    uint16_t blink = blink_ms_;
    if (blink != 0) {
        visible = ((millis() / blink) & 1) == 0;
    }
    uint8_t out = (visible && brightness_ > phase_) ? pattern_ : 0;
    phase_ = (phase_ + 1) & 3;
    if (out != last_written_) {
        writeRegister(out);
    }
}

void KeyLightDriver::writeRegister(uint8_t value) {
    last_written_ = value;
    digitalWrite(latch_pin_, LOW);
    shiftOut(data_pin_, clock_pin_, LSBFIRST, value);
    digitalWrite(latch_pin_, HIGH);
}

uint8_t KeyLightDriver::pattern() const {
    return pattern_;
}

uint8_t KeyLightDriver::brightness() const {
    return brightness_;
}

uint16_t KeyLightDriver::blink_ms() const {
    return blink_ms_;
}
