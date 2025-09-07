#include "TM1638plusWrapper.h"
#include "TM1638plus.h"

void TM1638plusWrapper::displayOff()
{
    // 0x80: display-control with OFF (bit3 = 0)
    sendCommand(static_cast<uint8_t>(TM_BRIGHT_ADR & ~0x08)); // -> 0x80
}

void TM1638plusWrapper::displayOn(uint8_t brightness)
{
    // Base 0x80 | ON bit (0x08) | brightness (0..7)
    const uint8_t base = static_cast<uint8_t>(TM_BRIGHT_ADR & ~0x08); // 0x80
    sendCommand(static_cast<uint8_t>(base | 0x08 | (brightness & TM_BRIGHT_MASK)));
}

// 0..7, 255-> OFF
void TM1638plusWrapper::brightness(uint8_t b)
{
    if (b == 255)
    {
        displayOff();
    }
    else
    {
        displayOn(b & TM_BRIGHT_MASK); // clamp to 0..7
    }
}

uint8_t TM1638plusWrapper::getNextBrightness(uint8_t b)
{
    static const uint8_t cycle[] = {0, 1, 2, 7, 255}; // 255 = OFF
    constexpr size_t N = sizeof(cycle) / sizeof(cycle[0]);
    size_t idx = std::find(std::begin(cycle), std::end(cycle), b) - std::begin(cycle);
    if (idx >= N - 1)
        return cycle[0]; // wrap around if at end or not found
    return cycle[idx + 1];
}

// Set leds by mask
void TM1638plusWrapper::setLEDs(uint8_t ledMask)
{
    for (uint8_t LEDposition = 0; LEDposition < 8; LEDposition++)
    {
        setLED(LEDposition, ledMask & 1);
        ledMask = ledMask >> 1;
    }
}