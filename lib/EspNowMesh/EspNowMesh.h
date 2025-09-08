// Minimal ESP-NOW mesh bootstrapper: initialize ESP-NOW and ensure broadcast peers
// for active WiFi interfaces (AP / STA) without coupling to web or portal logic.

#pragma once

#include <Arduino.h>

namespace EspNowMesh
{
  // Initialize ESP-NOW (idempotent) and add FF:FF:FF:FF:FF:FF broadcast peers
  // for each active interface depending on current WiFi mode (AP, STA, AP_STA).
  // Returns true on success.
  bool begin();
}

