// Controls: implementation

#include "Controls.h"
#include "GamePad.h"
#include "TM1638plusWrapper.h"
#include "DurstProto.h"
#include <atomic>

namespace
{
  // Concurrency note:
  // ESP-NOW receive callback runs on a different task than loop().
  // Use atomics for shared fields to avoid UB and torn multi-reads.
  // Relaxed order is sufficient as we only need the latest value.
  ControlsState s_state{};
  ControlsState s_prevState{}; // snapshot from previous update
  TM1638plusWrapper *tmPanel_ = nullptr;

  // Remote TM (slave->master) state + TTL
  static std::atomic<uint8_t> s_remoteTmRaw{0};
  static std::atomic<uint32_t> s_lastRemoteSeq{0};
  static uint32_t s_lastProcessedSeq = 0; // accessed only from loop()
  static std::atomic<uint32_t> s_remoteTmLastMs{0};
  static std::atomic<uint32_t> s_radioDrops{0}; // incremented in RX callback
  constexpr uint32_t REMOTE_TM_TTL_MS = 600; // > keepalive (RESEND_SAME_MS in TmButtonsBroadcaster)

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

  // Handle TM-state broadcast ESP-NOW messages from remote so Controls can merge remote button state
  static void onTmStateBroadcast(const MsgTmStateV1 &msg)
  {
    // Track radio-level gaps (count only missing sequences between consecutive RX calls)
    static uint32_t s_lastSeqRx = 0; // callback-local state
    if (s_lastSeqRx != 0)
    {
      const uint32_t diff = msg.seq - s_lastSeqRx; // modulo arithmetic
      if (diff > 1)
        s_radioDrops.fetch_add(diff - 1, std::memory_order_relaxed);
    }
    s_lastSeqRx = msg.seq;

    // Publish latest values for the main loop
    s_remoteTmRaw.store(msg.tmRaw, std::memory_order_relaxed);
    s_lastRemoteSeq.store(msg.seq, std::memory_order_relaxed);
    s_remoteTmLastMs.store(millis(), std::memory_order_relaxed);
  }

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

    DurstProto::setOnTmStateBroadcast(onTmStateBroadcast);
  }

  static const TMState calculateTmState(const uint8_t rawButtons)
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
    s_prevState = s_state;                    // Snapshot to enable edge detection after this update
    const uint32_t lastSeq = s_lastRemoteSeq.load(std::memory_order_relaxed); // snapshot once
    if (lastSeq != 0)
    {
      // Compute modulo difference so wrap-around (0xFFFFFFFF -> 0) yields diff==1
      const uint32_t diff = lastSeq - s_lastProcessedSeq;
      if (diff > 1)
      {
        Serial.print("Controls: skipped since last update = ");
        Serial.print(diff - 1);
        Serial.print(" s_lastRemoteSeq="); Serial.print(lastSeq);
        Serial.print(" s_lastProcessedSeq="); Serial.print(s_lastProcessedSeq);
        Serial.print(" radio_drops_total="); Serial.println(s_radioDrops.load(std::memory_order_relaxed));
      }
    }
    s_lastProcessedSeq = lastSeq;

    BtInput::update(); // Update BT state from bluepad32

    // Local TM raw (if present)
    uint8_t localRaw = 0;
    if (tmPanel_ != nullptr)
      localRaw = tmPanel_->readButtons();

    // Merge remote TM if recent
    uint8_t effectiveRaw = localRaw;
    const uint32_t now = millis();
    const uint32_t remoteLastMs = s_remoteTmLastMs.load(std::memory_order_relaxed);
    if (now - remoteLastMs <= REMOTE_TM_TTL_MS)
      effectiveRaw |= s_remoteTmRaw.load(std::memory_order_relaxed);

    TMState tmState = calculateTmState(effectiveRaw);

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
