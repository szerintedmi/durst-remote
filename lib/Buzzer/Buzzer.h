#pragma once
#include <Arduino.h>
#include "esp_timer.h"

// Small helper for beeps/tones using LEDC + a one-shot esp_timer.
class Buzzer
{
public:
    // ledPin is optional: pass -1 if you don't want an indicator LED toggled.
    Buzzer(uint8_t buzzerPin, uint8_t ledcChannel, int8_t ledPin = -1);

    // Configure LEDC (base freq/resolution) and attach pin to channel.
    // Note: 'resolutionBits' typically 8 for 0..255 "volume" via ledcWrite().
    void begin(uint32_t basePwmFreq = 2000, uint8_t resolutionBits = 8);

    // Start a tone for tone_ms milliseconds at 'freq' (Hz) and 'volume' (0..2^res-1).
    // Non-blocking; a one-shot timer will stop it automatically.
    void buzz(uint16_t tone_ms, uint16_t volume, uint32_t freq);

    // Stop any ongoing tone immediately.
    void stop();

    // Is a tone currently active?
    bool isActive() const { return _active; }

    // (Optional) change the LED pin at runtime; use -1 to disable LED feedback.
    void setLedPin(int8_t ledPin) { _ledPin = ledPin; }

private:
    static void timerStopCb(void *arg); // static trampoline for esp_timer
    void handleStop();                  // instance handler

    uint8_t _pin;
    uint8_t _ch;
    int8_t _ledPin;
    esp_timer_handle_t _timer = nullptr;
    bool _active = false;
};