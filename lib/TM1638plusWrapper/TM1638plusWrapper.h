// This is a small wrapper to expose true display ON/OFF control for TM1638
#pragma once
#include <TM1638plus.h>

// Small wrapper to expose true display ON/OFF control for TM1638
class TM1638plusWrapper : public TM1638plus
{
public:
    using TM1638plus::TM1638plus;

    // Generic TM1638 button bit positions (matches readButtons bitmask)
    static constexpr uint8_t S1 = 0x01; // Button 1
    static constexpr uint8_t S2 = 0x02; // Button 2
    static constexpr uint8_t S3 = 0x04; // Button 3
    static constexpr uint8_t S4 = 0x08; // Button 4
    static constexpr uint8_t S5 = 0x10; // Button 5
    static constexpr uint8_t S6 = 0x20; // Button 6
    static constexpr uint8_t S7 = 0x40; // Button 7
    static constexpr uint8_t S8 = 0x80; // Button 8

    // Override brightness() so 255 = OFF, 0..7 = ON
    void brightness(uint8_t b);

    // Turns scanning completely OFF (all digits/LEDs off, chip idle)
    void displayOff();

    // Turns display ON and sets brightness 0..7
    void displayOn(uint8_t brightness);

    // Set leds by mask
    void setLEDs(uint8_t ledMask);

    // Helper: cycle brightness values 0 → 1 → 2 → 7 → 255 (OFF) → 0. Not using 3-6 levels as they look the same as 7
    static uint8_t getNextBrightness(uint8_t b);
};
