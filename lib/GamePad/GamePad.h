// Bluepad32 wrapper: exposes a small aggregated gamepad state

#pragma once

#include <stdbool.h>

namespace BtInput
{

  struct GamepadState
  {
    bool y = false; // PS Triangle
    bool a = false; // PS Cross
    bool b = false; // PS Circle
    bool x = false; // PS Square
    bool dpadUp = false;
    bool dpadDown = false;
    bool dpadLeft = false;
    bool dpadRight = false;
    bool r1 = false;
    bool l1 = false;
    bool r2 = false;
    bool l2 = false;
  };

  // Initializes Bluepad32 and starts scanning for controllers.
  void begin(bool startScanning = true);

  // Must be called frequently from loop(). Updates controller state.
  void update();

  // Aggregated state across all connected controllers.
  const GamepadState &state();

} // namespace BtInput
