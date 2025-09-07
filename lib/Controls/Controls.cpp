// Controls: implementation

#include "Controls.h"
#include "GamePad.h"
#include "TM1638plusWrapper.h"

namespace
{
  ControlsState s_state{};
  ControlsState s_prevState{}; // snapshot from previous update
  TM1638plusWrapper *tmPanel_ = nullptr;

  struct TMState
  {
    uint8_t raw = 0; // TM1638 buttons
    bool S1 = false;
    bool S2 = false;
    bool S3 = false;
    bool S4 = false;
    bool S5 = false;
    bool S6 = false;
    bool S7 = false;
    bool S8 = false;
  };

  static void fillDerivedAndConflicts(ControlsState &cs)
  {
    cs.m1Dir = (cs.M1Down ? 1 : 0) - (cs.M1Up ? 1 : 0);
    cs.m2Dir = (cs.M2Down ? 1 : 0) - (cs.M2Up ? 1 : 0);
    cs.m1Conflict = cs.M1Down && cs.M1Up;
    cs.m2Conflict = cs.M2Down && cs.M2Up;
    cs.anyDirectionConflict = cs.m1Conflict || cs.m2Conflict;
  }

} // namespace

namespace Controls
{

  void begin(TM1638plusWrapper *tmPanel)
  {
    tmPanel_ = tmPanel;
    BtInput::begin();
  }

  static const TMState getTmState(const uint8_t rawButtons)
  {
    TMState tms{};
    tms.raw = rawButtons;
    tms.S1 = tms.raw & TM1638plusWrapper::S1;
    tms.S2 = tms.raw & TM1638plusWrapper::S2;
    tms.S3 = tms.raw & TM1638plusWrapper::S3;
    tms.S4 = tms.raw & TM1638plusWrapper::S4;
    tms.S5 = tms.raw & TM1638plusWrapper::S5;
    tms.S6 = tms.raw & TM1638plusWrapper::S6;
    tms.S7 = tms.raw & TM1638plusWrapper::S7;
    tms.S8 = tms.raw & TM1638plusWrapper::S8;
    return tms;
  }

  void update()
  {
    s_prevState = s_state; // Snapshot to enable edge detection after this update

    BtInput::update(); // Update BT state from bluepad32

    TMState tmState = {};
    if (tmPanel_ != nullptr)
      tmState = getTmState(tmPanel_->readButtons());

    const auto &gamePadsState = BtInput::state(); // Aggregate BT state (in case of multiple controllers)

    // Merge: OR per control flag
    s_state.Fast = tmState.S1 || gamePadsState.l2 || gamePadsState.r2;
    s_state.Insane = gamePadsState.l1;
    s_state.M1Down = tmState.S4 || gamePadsState.a;
    s_state.M1Up = tmState.S5 || gamePadsState.y;
    s_state.M2Down = tmState.S6 || gamePadsState.x;
    s_state.M2Up = tmState.S7 || gamePadsState.b;
    s_state.Brightness = gamePadsState.r1;
    s_state.toggleLamp = tmState.S1 && tmState.S8 || gamePadsState.dpadLeft;
    s_state.StartTimer = tmState.S8 && !tmState.S1 || gamePadsState.dpadRight;
    s_state.increaseTimer = tmState.S3 || gamePadsState.dpadUp;
    s_state.decreaseTimer = tmState.S2 || gamePadsState.dpadDown;

    // Build merged buttons mask used for LEDs

    s_state.buttonsMask = tmState.raw;

    // Derived + conflicts
    fillDerivedAndConflicts(s_state);
  }

  const ControlsState &state() { return s_state; }

  bool rising(bool ControlsState::*field)
  {
    return (s_state.*field) && !(s_prevState.*field);
  }

  bool falling(bool ControlsState::*field)
  {
    return !(s_state.*field) && (s_prevState.*field);
  }

  bool changed(bool ControlsState::*field)
  {
    return (s_state.*field) != (s_prevState.*field);
  }

} // namespace Controls
