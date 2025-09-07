// Lightweight protocol helper for ESP-NOW messages.
// Step 1: only handles display text message (MsgV1) to keep behavior unchanged.

#pragma once

#include <stdint.h>
#include <esp_now.h>

#include "DurstProtoTypes.h"

namespace DurstProto
{
  // Broadcast MAC (kept local to avoid heavy deps)
  constexpr uint8_t BROADCAST_MAC[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

  // Handler type for display text broadcast messages
  using DisplayBroadcastHandler = void (*)(const MsgV1 &msg);

  // Register display broadcast handler and ensure ESP-NOW recv callback is attached
  // (idempotent). Keeps slave/master setup to a single call.
  void setOnDisplayBroadcast(DisplayBroadcastHandler h);

  // Send current display message to broadcast MAC
  bool broadcastDisplayText(const MsgV1 &msg);

  // Low-level send to a specific MAC
  bool sendTo(const uint8_t mac[6], const void *data, size_t len);
}
