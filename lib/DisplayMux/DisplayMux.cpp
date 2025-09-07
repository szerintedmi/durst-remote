#include "DisplayMux.h"

#include <string.h>
#include <Arduino.h>
#include <rgb_lcd.h>
#include <esp_now.h>
#include <Wire.h>
#include <esp_log.h>

#include "TM1638plusWrapper.h"
#include "DurstProto.h"

// ---- Ctors ----
DisplayMux::DisplayMux() = default;
DisplayMux::DisplayMux(TM1638plusWrapper *seg) : seg_(seg) {}
DisplayMux::DisplayMux(rgb_lcd *lcd) : lcd_(lcd) {}
DisplayMux::DisplayMux(TM1638plusWrapper *seg, rgb_lcd *lcd) : seg_(seg), lcd_(lcd) {}

// ---- Setup / attach lcd and TM1638 ----
void DisplayMux::begin(uint8_t segBrightness)
{
  if (begun_)
    return;

  if (seg_) // ---- TM1638 Display ----
  {
    seg_->displayBegin(); // TM1638 panel init
    // Apply cached brightness on boot (255=off is honored by wrapper)
    seg_->brightness(segBrightness);
    segBrightness_ = segBrightness;
  }

  if (lcd_) // ---- LCD Display ----
  {
    // Probe I2C to avoid log spam if LCD absent
    lcdPresent_ = probeLcd_();
    if (lcdPresent_)
    {
      esp_log_level_set("i2c.master", ESP_LOG_NONE); // suppress alerts during init
      lcd_->begin(16, 2);
      esp_log_level_set("i2c.master", ESP_LOG_WARN);
    }
  }

  begun_ = true;
}

void DisplayMux::attachTM1638(TM1638plusWrapper *seg)
{
  seg_ = seg;
  if (begun_ && seg_)
  {
    seg_->displayBegin();
    seg_->brightness(segBrightness_);
  }
}

void DisplayMux::attachRgbLcd(rgb_lcd *lcd)
{
  lcd_ = lcd;
  // Force a re-probe on next use
  lcdPresent_ = false;
}

bool DisplayMux::hasSegment() const { return seg_ != nullptr; }
bool DisplayMux::hasLcd() const { return lcd_ != nullptr; }

// ---- TM1638 ops ----
void DisplayMux::segSetBrightness(uint8_t b)
{
  segBrightness_ = b;
  if (seg_)
    seg_->brightness(b);

  broadcastIfDue(); // Broadcast change if enabled
}

void DisplayMux::segSetLEDs(uint8_t mask)
{
  if (seg_)
    seg_->setLEDs(mask);
}

// Sets text for all displays, brightness for TM1638 then broadcasts it via ESP-NOW.
//  segText (max 8 chars + max 2 decimal points for 2x7 seg display (TM1638)
//  lcdLine1 & lcdLine2 (max 16 chars), lcdLine2 (max 16 chars) for 2x16 LCD display
void DisplayMux::displayAndBroadCastTexts(uint8_t brightness, const char segText[11], const char lcdLine1[17], const char lcdLine2[17])
{
  if (lcd_)
  {
    if (lcdPresent_)
    {
      lcd_->setCursor(0, 0);
      lcd_->print(lcdLine1);

      lcd_->setCursor(0, 1);
      lcd_->print(lcdLine2);
    }
  }

  if (seg_)
  {
    seg_->brightness(brightness);
    seg_->displayText(segText);
  }

  DisplayMux::segBrightness_ = brightness;
  copy_cstr(DisplayMux::segText_, segText);
  copy_cstr(DisplayMux::lcdLine1_, lcdLine1);
  copy_cstr(DisplayMux::lcdLine2_, lcdLine2);

  broadcastIfDue();
}

// Broadcast attempt of current display state respecting MIN_RESEND_INTERVAL_MS throttle and RESEND_SAME_MS
// Needs to be called regularly to resend if first attempt was throttled
// and for regular resends for newly joined/rejoined slaves
void DisplayMux::broadcastIfDue()
{
  if (!broadcastEnabled_)
    return;

  const uint32_t now = millis();

  if (now - lastBroadcastMs_ < MIN_RESEND_INTERVAL_MS)
    return; // rate limit, check again at next call

  const bool hasAnyChanged =
      (segBrightness_ != lastBroadcastedSegBrightness_) ||
      (memcmp(segText_, lastBroadcastedSegText_, sizeof(segText_)) != 0) ||
      (memcmp(lcdLine1_, lastBroadcastedLcdLine1_, sizeof(lcdLine1_)) != 0) ||
      (memcmp(lcdLine2_, lastBroadcastedLcdLine2_, sizeof(lcdLine2_)) != 0);

  if (!hasAnyChanged && (now - lastBroadcastMs_ < RESEND_SAME_MS))
  {
    return; // unchanged within throttle
  }

  lastBroadcastMs_ = now;

  MsgV1 msg{};
  msg.magic = PROTO_MAGIC;
  msg.version = 1;
  msg.cmd = CMD_DISPLAY_TEXT;
  msg.flags = 0;
  msg.seq = ++broadcastSeq_;
  msg.brightness = segBrightness_;
  copy_cstr(msg.lcdLine1, lcdLine1_);
  copy_cstr(msg.lcdLine2, lcdLine2_);
  copy_cstr(msg.segText, segText_);

  lastBroadcastMs_ = now;
  bool ok = DurstProto::broadcastDisplayText(msg);

  lastBroadcastedSegBrightness_ = segBrightness_;
  copy_cstr(lastBroadcastedSegText_, segText_);
  copy_cstr(lastBroadcastedLcdLine1_, lcdLine1_);
  copy_cstr(lastBroadcastedLcdLine2_, lcdLine2_);

  if (!ok)
  {
    static uint32_t lastErrMs = 0;
    if (now - lastErrMs > 2000)
    {
      Serial.printf("DisplayMux: ESP-NOW broadcast error\n");
      lastErrMs = now;
    }
  }
  else if (hasAnyChanged)
  {
    Serial.printf("DisplayMux: broadcast seq=%lu brightness=%u segText='%s' lcdLine1='%s' lcdLine2='%s\n",
                  (unsigned long)msg.seq, (unsigned)msg.brightness,
                  msg.segText, msg.lcdLine1, msg.lcdLine2);
  }
}

// ---- Private helpers ----
bool DisplayMux::probeLcd_()
{
  // Suppress I2C NACK logs during probe
  esp_log_level_set("i2c.master", ESP_LOG_NONE);
  // Ensure I2C bus is initialized (idempotent on ESP32 Arduino)
  Wire.begin();

  // Grove LCD uses 0x3E (text) and 0x62 (RGB). Presence of either is good enough
  auto probe = [](uint8_t addr) -> bool
  {
    Wire.beginTransmission(addr);
    uint8_t err = Wire.endTransmission();
    return err == 0;
  };

  bool ok = probe(0x3E) || probe(0x62);
  esp_log_level_set("i2c.master", ESP_LOG_WARN);
  return ok;
}
