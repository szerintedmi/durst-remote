// Generic display multiplexer to unify TM1638 (7-seg) and 16x2 RGB LCD usage
// optional broadcast of displays state via ESP-NOW.

#pragma once

#include <stdint.h>
#include <Arduino.h>

constexpr uint8_t DEFAULT_SEG_BRIGHTNESS = 7;
constexpr uint8_t BROADCAST_MAC[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
constexpr uint32_t RESEND_SAME_MS = 300;        // unchanged text resend throttle
constexpr uint32_t MIN_RESEND_INTERVAL_MS = 40; // global rate limit

// Forward declarations to avoid heavy headers in this interface
class TM1638plusWrapper;
class rgb_lcd;

class DisplayMux
{
public:
  // Constructors: attach none, one, or both devices
  DisplayMux();
  explicit DisplayMux(TM1638plusWrapper *seg);
  explicit DisplayMux(rgb_lcd *lcd);
  DisplayMux(TM1638plusWrapper *seg, rgb_lcd *lcd);

  // Non-copyable (owns no resources but avoids accidental copies)
  DisplayMux(const DisplayMux &) = delete;
  DisplayMux &operator=(const DisplayMux &) = delete;

  // Setup: initialize attached devices (idempotent)
  void begin(uint8_t segBrightness = DEFAULT_SEG_BRIGHTNESS);

  // Attach devices after construction (safe to call before/after begin())
  void attachTM1638(TM1638plusWrapper *seg);
  void attachRgbLcd(rgb_lcd *lcd);

  // Capability queries
  bool hasSegment() const;
  bool hasLcd() const;

  // Update displays and broadcast state via ESP-NOW if broadcast enabled
  // Needs to be called in loop to broadcast state if first attemp was throttled and
  // to send regular updates even if state unchanged to new slaves
  void displayAndBroadCastTexts(uint8_t brightness, const char segText[11], const char lcdLine1[17], const char lcdLine2[17]);

  // Segment (TM1638) APIs
  // - Brightness: 0..7 = ON levels, 255 = OFF (per TM1638plusWrapper)
  void segSetBrightness(uint8_t b);
  // - Set TM1638 LEDs by bit mask (S1..S8 positions as per TM1638plusWrapper)
  void segSetLEDs(uint8_t mask);

  // Broadcast control (ESP-NOW)
  void setBroadcastEnabled(bool enabled) { broadcastEnabled_ = enabled; }
  bool isBroadcastEnabled() const { return broadcastEnabled_; }

private:
  TM1638plusWrapper *seg_ = nullptr;
  rgb_lcd *lcd_ = nullptr;
  bool begun_ = false;
  bool lcdPresent_ = false;     // true if LCD I2C device is detected

  void broadcastIfDue();
  bool probeLcd_();

  // Cached state for TM1638 + broadcast
  uint8_t segBrightness_ = 0xFF; // 255=OFF by wrapper semantics
  char segText_[11] = {0};
  char lcdLine1_[17] = {0};
  char lcdLine2_[17] = {0};

  // Broadcast settings/state
  bool broadcastEnabled_ = false;
  uint32_t lastBroadcastMs_ = 0;
  uint32_t broadcastSeq_ = 0;

  uint8_t lastBroadcastedSegBrightness_ = 0xFF;
  char lastBroadcastedSegText_[11] = "";
  char lastBroadcastedLcdLine1_[17] = "";
  char lastBroadcastedLcdLine2_[17] = "";
};
