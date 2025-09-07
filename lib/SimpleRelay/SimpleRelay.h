// SimpleRelay: minimal, header-only relay helper
// Usage:
//   SimpleRelay lamp(pin);
//   lamp.on(); lamp.off(); lamp.toggle();

#pragma once

#include <Arduino.h>

class SimpleRelay
{
public:
    explicit SimpleRelay(uint8_t pin)
        : pin_(pin) {}

    // Initialize hardware pin and set initial state (default OFF).
    void begin(bool initialOn = false)
    {
        pinMode(pin_, OUTPUT);
        if (initialOn)
            on();
        else
            off();
    }

    void on()
    {
        is_on_ = true;
        digitalWrite(pin_, HIGH);
    }

    void off()
    {
        is_on_ = false;
        digitalWrite(pin_, LOW);
    }

    bool toggle()
    {
        if (is_on_)
            off();
        else
            on();

        return is_on_;
    }

    bool isOn() const { return is_on_; }

private:
    uint8_t pin_;
    bool is_on_ = false;
};
