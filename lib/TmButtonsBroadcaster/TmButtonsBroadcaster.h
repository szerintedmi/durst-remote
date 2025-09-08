// Minimal broadcaster for TM1638 button state on slave
// Mirrors DisplayMux broadcast throttling behaviour (immediate on change, keepalive resend)

#pragma once

#include <stdint.h>

class TM1638plusWrapper;

namespace TmBroadcast
{
  constexpr uint32_t RESEND_SAME_MS = 250;        // resend throttle
  constexpr uint32_t MIN_RESEND_INTERVAL_MS = 10; // rate limit

  class Broadcaster
  {
  public:
    explicit Broadcaster(TM1638plusWrapper *tm) : tm_(tm) {}
    void begin();
    void update();

  private:
    TM1638plusWrapper *tm_ = nullptr;
    uint8_t lastRaw_ = 0;
    uint32_t lastSendMs_ = 0;
    uint32_t seq_ = 0;
  };
} // namespace TmBroadcast
