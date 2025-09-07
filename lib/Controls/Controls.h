// Controls: unified input state from TM1638 and Bluetooth gamepad(s)
// Keeps mainMaster lean by exposing a single merged ControlsState.

#pragma once

#include <stdint.h>

struct ControlsState
{

  uint8_t buttonsMask = 0; // S1-S8 on tm for LED

  bool Fast = false;          // S1 or BT L2
  bool Insane = false;        // BT L1
  bool M1Down = false;        // S4 or BT a Cross (×)
  bool M1Up = false;          // S5 or BT y Triangle (△)
  bool M2Down = false;        // S6 or BT b Square (□)
  bool M2Up = false;          // S7 or BT x Circle (○)
  bool Brightness = false;    // BT R2
  bool StartTimer = false;    // S8 BT Pad Right
  bool toggleLamp = false;    // S1+S8 or BT D-pad Left
  bool decreaseTimer = false; // S2 BT D-pad Down
  bool increaseTimer = false; // S3 BT D-pad Up

  // Derived directions (-1,0,+1)
  int8_t m1Dir = 0; // -1=down, +1=up
  int8_t m2Dir = 0;

  // Conflicts
  bool m1Conflict = false;           // M1Up & M1Down both pressed
  bool m2Conflict = false;           // M2Up & M2Down both pressed
  bool anyDirectionConflict = false; // m1Conflict || m2Conflict
};

// Forward declare TM1638plusWrapper from global namespace to avoid header dependency
class TM1638plusWrapper;

namespace Controls
{

  // Optionally TM1638 panel in one call.
  void begin(TM1638plusWrapper *tmPanel = nullptr);

  // Update internal state: reads TM (if present) + BT and merges them.
  void update();

  // Access the current merged controls state.
  const ControlsState &state();

  // Edge helpers (true for one update cycle only)
  bool rising(bool ControlsState::*field);  // false->true
  bool falling(bool ControlsState::*field); // true->false
  bool changed(bool ControlsState::*field); // any toggle

} // namespace Controls
