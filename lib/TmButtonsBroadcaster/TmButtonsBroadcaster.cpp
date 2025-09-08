#include "TmButtonsBroadcaster.h"

#include <Arduino.h>

#include "TM1638plusWrapper.h"
#include "DurstProto.h"

using namespace TmBroadcast;

void Broadcaster::begin()
{
  lastRaw_ = 0;
  lastSendMs_ = 0;
  seq_ = 0;
}

void Broadcaster::update()
{
  if (!tm_)
    return;

  const uint32_t now = millis();
  const uint8_t raw = tm_->readButtons();

  const bool changed = raw != lastRaw_;

  if (now - lastSendMs_ < MIN_RESEND_INTERVAL_MS) // throttle
    return;

  if (!changed && (now - lastSendMs_ < RESEND_SAME_MS)) // resend same only every RESEND_SAME_MS
    return;

  MsgTmStateV1 msg{};
  msg.magic = PROTO_MAGIC;
  msg.version = 1;
  msg.cmd = CMD_TM_STATE;
  msg.flags = 0;
  msg.seq = ++seq_;
  msg.tmRaw = raw;

  lastSendMs_ = now;
  const bool ok = DurstProto::broadcastTmState(msg);
  lastRaw_ = raw;

  if (!ok)
  {
    static uint32_t lastErrMs = 0;
    if (now - lastErrMs > 2000)
    {
      Serial.printf("TmButtonsBroadcaster: ESP-NOW broadcast error\n");
      lastErrMs = now;
    }
  }
}
