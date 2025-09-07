// Simple compatibility layer for ESP32 Arduino LEDC API changes between 2.x and 3.x
// - 3.x introduced pin-based APIs like ledcAttachChannel(), ledcWriteChannel(), ledcWrite(pin,...)
// - 2.x uses channel-based APIs: ledcSetup(), ledcAttachPin(), ledcWrite(channel,...)
//
// Use ARDUINO_ESP32_HAS_LEDC_ATTACH_CHANNEL to branch in code.

#pragma once
#include <Arduino.h>

#if defined(ESP_ARDUINO_VERSION_MAJOR)
#  define ARDUINO_ESP32_HAS_LEDC_ATTACH_CHANNEL (ESP_ARDUINO_VERSION_MAJOR >= 3)
#elif defined(ESP_ARDUINO_VERSION) && defined(ESP_ARDUINO_VERSION_VAL)
#  define ARDUINO_ESP32_HAS_LEDC_ATTACH_CHANNEL (ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0))
#else
#  define ARDUINO_ESP32_HAS_LEDC_ATTACH_CHANNEL 0
#endif

