#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_now.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <rgb_lcd.h>

#include "SimpleTimer.h"
#include "WifiPortal.h"
#include "EspNowMesh.h"
#include "TM1638plusWrapper.h"
#include "DisplayMux.h"
#include "DRV8874.h"
#include "Buzzer.h"
#include "Controls.h"
#include "SimpleRelay.h"

Preferences prefs;

SimpleTimer timer(9000);

// Lamp SSR relay (declare before onTimerDone)
constexpr uint8_t LAMP_RELAY_PIN = 33;
SimpleRelay lamp(LAMP_RELAY_PIN);

static void onTimerDone(void *ctx)
{
  lamp.off();
  Serial.println("Timer finished!");
}

// ================= TM1638 setup =================
constexpr uint8_t TM1638_CLK_PIN = 32;
constexpr uint8_t TM1638_DIO_PIN = 14;
constexpr uint8_t TM1638_STB_PIN = 13;
constexpr bool IS_TM1638_HIGH_FREQ = true; // default false,, If using a high freq CPU > ~100 MHZ set to true.

// TM1638
uint8_t brightness = 0;
TM1638plusWrapper tm(TM1638_STB_PIN, TM1638_CLK_PIN, TM1638_DIO_PIN, IS_TM1638_HIGH_FREQ);

// Grove LCD Display , defaults to PIN 21 SDA and 22 SCL
rgb_lcd lcd;
DisplayMux displays(&tm, &lcd);

// Lamp SSR relay handled above

// ================= Buzzer =================
constexpr uint8_t BUZZER_PIN = 16; // your buzzer GPIO
constexpr uint8_t LEDC_CH = 4;     // LEDC channel you want to use
constexpr int8_t LED_PIN = -1;     // optional LED; or -1 to disable

Buzzer buzz(BUZZER_PIN, LEDC_CH, LED_PIN);

// ================= TWO motors =================

// We assume PWM (IN/IN) mode , i.e. PMODE is tied HIGH on the hardware
constexpr uint8_t M1_IN1 = 25;
constexpr uint8_t M1_IN2 = 26;
constexpr uint8_t M1_SLEEP = 0; // if hard-wired HIGH, set to 0
constexpr uint8_t M1_FAULT = 27;
constexpr uint8_t M1_CS = 35;
constexpr uint8_t M1_CH1 = 0;
constexpr uint8_t M1_CH2 = 1;

constexpr uint8_t M2_IN1 = 18;
constexpr uint8_t M2_IN2 = 19;
constexpr uint8_t M2_SLEEP = 0; // if hard-wired HIGH, set to 0
constexpr uint8_t M2_FAULT = 22;
constexpr uint8_t M2_CS = 34;
constexpr uint8_t M2_CH1 = 2;
constexpr uint8_t M2_CH2 = 3;

// Motor minDuty set to result a duty where the motor moving when speedPt set to 10%
// Lens motor min duty down 613 , up 640. w/ 10% speed minDutyPos = 568,  minDutyNeg = 598
DRV8874 motor1(M1_IN1, M1_IN2, M1_CS, M1_SLEEP, M1_CH1, M1_CH2,
               568 /* minDutyPos */, 598 /* minDutyNeg */);
// Head motor min duty down 628, up 623. w/ 10% speed minDutyPos = 584,  minDutyNeg = 579
DRV8874 motor2(M2_IN1, M2_IN2, M2_CS, M2_SLEEP, M2_CH1, M2_CH2,
               584 /* minDutyPos */, 579 /* minDutyNeg */);

void updateDisplay(const uint8_t brightness, const bool lampState, const DRV8874 &m1, const DRV8874 &m2, const SimpleTimer &timer,
                   const bool anyDirectionConflict, const bool m1Fault, const bool m2Fault)
{
  char segText[11] = "";                  // 2x4 chars + optional 2 x 1 decimal point + NUL for TM1638plus::displayText
  char lcdLine1[17] = "I'm the master!!"; // 16 chars + NUL. Slave ignores lcdLine1 and overrides with debug info
  char lcdLine2[17] = "";

  const int32_t m1Signed = m1.getDutyCmd() * m1.getDirection();
  const int32_t m2Signed = m2.getDutyCmd() * m2.getDirection();
  const uint32_t timerMs = static_cast<uint32_t>(timer.remainingMs());
  uint16_t tenths = (timerMs + 50) / 100; // round to 0.1s
  uint16_t whole = tenths / 10;
  uint8_t frac = tenths % 10;

  // Choose which value to display: prefer M1 unless it is zero
  int32_t toDisplay = (m1Signed == 0 ? m2Signed : m1Signed);
  int32_t toDisplayClamped = toDisplay < -999 ? -999 : toDisplay; // we only have 4 digits on the TM1638

  int8_t errorCode = m1Fault ? 1 : m2Fault            ? 2
                               : anyDirectionConflict ? 3
                                                      : 0;

  int firstSeg = errorCode ? errorCode : (int)toDisplayClamped;
  const char lampStateChar = lampState ? 'L' : ' ';

  snprintf(segText, sizeof(segText),
           errorCode ? "ERR%1d%3u.%u" : "%4d%1c%2u.%u",
           firstSeg, lampStateChar, (unsigned)whole, (unsigned)frac);

  snprintf(lcdLine2, sizeof(lcdLine2),
           errorCode ? "ERROR:%1d  %4u.%us" : "%5d    %1c%3u.%us",
           errorCode ? errorCode : (int)toDisplay, lampStateChar, (unsigned int)whole, frac);

  displays.displayAndBroadCastTexts(brightness, segText, lcdLine1, lcdLine2);
}

// Controls handled by Controls module

volatile bool faultM1 = false;
void IRAM_ATTR onM1Fault() { faultM1 = true; }
volatile bool faultM2 = false;
void IRAM_ATTR onM2Fault() { faultM2 = true; }

// ================= Web server & WifiPortal =================

AsyncWebServer webServer(80);
WifiPortal wifiPortal(MESH_SSID, MESH_PASS);

static void attachRoutes()
{
  if (!(LittleFS.begin(false) || LittleFS.begin(true))) // idempotent
  {
    Serial.println("LittleFS mount failed");
    return;
  }

  webServer.serveStatic("/", LittleFS, "/")
      .setDefaultFile("index.html")
      .setFilter([](AsyncWebServerRequest *r)
                 {
                const String& u = r->url();
                return !(u.startsWith("/wifi/api/") || u == "/wifi/api"); });

  ;
}

// ================= Setup =================
void setup()
{
  Serial.begin(115200);

  lamp.begin(); // default initial state: OFF

  attachRoutes();
  wifiPortal.beginAndConnect(webServer, /*staTimeoutMs=*/10000);
  // Start ESP-NOW mesh on the active WiFi interface(s)
  EspNowMesh::begin();
  webServer.begin();

  buzz.begin();

  prefs.begin("durst", false);

  timer.setDurationMs(prefs.getULong("duration", 9)); // set last used duration

  // ---- TM1638 and Grove LCD Display init (optional hardware) ----
  // Same default pins as slave; disable
  brightness = prefs.getUChar("brightness", 7);
  displays.begin(brightness); // initializes TM1638 if present
  displays.setBroadcastEnabled(true);

  Serial.printf("Setup(): brightness=%d\n", brightness);

  analogReadResolution(12);
  // Set default attenuation for all ADC reads. Per-pin attenuation before
  // first read can log errors on ESP32 Arduino v3.x, so avoid that here.
  analogSetAttenuation(CS_ADC_ATTENUATION);

  motor1.begin();
  pinMode(M1_FAULT, INPUT_PULLUP); // or INPUT with external 10k
  attachInterrupt(digitalPinToInterrupt(M1_FAULT), onM1Fault, FALLING);

  motor2.begin();
  pinMode(M2_FAULT, INPUT_PULLUP); // or INPUT with external 10k
  attachInterrupt(digitalPinToInterrupt(M2_FAULT), onM2Fault, FALLING);

  // ---- Controls (TM1638 + controller using BluePad32) ----
  Controls::begin(&tm);

  Serial.println("Setup: done");
}

void loop()
{
  // unsigned motorspeed in % of MAX_DUTY
  constexpr uint8_t SLOW_PT = 10;
  constexpr uint8_t FAST_PT = 70;
  constexpr uint8_t INSANE_PT = 100;

  // Update controls (merges TM1638 + BT) and fetch state
  Controls::update();
  const auto &cs = Controls::state();
  // if (motor1.getDutyCmd() > 0 || motor2.getDutyCmd() > 0)
  // Serial.printf(">m1DutyCmd:%d,m2DutyCmd:%d,m1A:%.2f,m2A:%.2f,\r\n",
  //               motor1.getDutyCmd(), motor2.getDutyCmd(), motor1.getCurrentmA() / 1000.0f, motor2.getCurrentmA() / 1000.0f);

  uint8_t speedControlPt = cs.Fast ? FAST_PT : cs.Insane ? INSANE_PT
                                                         : SLOW_PT;

  // TODO: race conditions? also, do we need debounce?
  bool debouncedFaultM1 = false;
  bool debouncedFaultM2 = false;
  if (faultM1)
  {
    delayMicroseconds(200);
    if (digitalRead(M1_FAULT) == LOW) // small debounce/filter in case of brief pulses
      debouncedFaultM1 = true;
  }

  if (faultM2)
  {
    delayMicroseconds(200);
    if (digitalRead(M2_FAULT) == LOW) // small debounce/filter in case of brief pulses
      debouncedFaultM2 = true;
  }

  // Motor 1 command
  if (cs.m1Conflict || debouncedFaultM1)
  {
    motor1.coast();
    buzz.buzz(200, 255, 80);
  }
  else if (cs.m1Dir == 0 && motor1.getSpeed() != 0)
    // motor1.coast();
    motor1.brake();
  else if (motor1.getSpeed() != speedControlPt * cs.m1Dir)
  {
    motor1.run(speedControlPt * cs.m1Dir);
  }

  // Motor 2 command
  if (cs.m2Conflict || debouncedFaultM2)
  {
    motor2.coast();
    buzz.buzz(200, 255, 80);
  }
  else if (cs.m2Dir == 0 && motor2.getSpeed() != 0)
    // motor2.coast();
    motor2.brake();
  else if (motor2.getSpeed() != speedControlPt * cs.m2Dir)
  {
    motor2.run(speedControlPt * cs.m2Dir);
  }

  displays.segSetLEDs(cs.buttonsMask);

  if (Controls::rising(&ControlsState::Brightness))
  {
    brightness = TM1638plusWrapper::getNextBrightness(brightness); // updateDisplay will take care of it
    prefs.putUChar("brightness", brightness);
    Serial.printf("mainMaster: new brightness=%d\n", brightness);
  }

  // Start timer on rising edge only (avoid restart every loop while pressed)
  if (Controls::rising(&ControlsState::StartTimer))
  {
    prefs.putULong("duration", timer.getDurationMs()); // save last used duration for next boot
    timer.start(onTimerDone);
    lamp.on();
    Serial.printf("Timer started timer.remainingMs()=%d timer.isRunning()=%d timer.getDurationMs()=%d\n",
                  (int)timer.remainingMs(), (int)timer.isRunning(), (int)timer.getDurationMs());
  }

  if (Controls::rising(&ControlsState::toggleLamp))
  {
    lamp.toggle();
    Serial.printf("toggleLamp: lamp is now %s\n", lamp.isOn() ? "ON" : "OFF");
  }

  uint16_t timerStep = 100;
  if (cs.Fast || cs.Insane)
    timerStep = 1000;

  // Apply timer +/- no faster than every repeatMs while button is held
  static uint32_t nextTimerAdjustMs = 0; // 0 means immediate on next press
  const uint16_t repeatMs = 200;         // adjust rate while holding (ms)
  const uint32_t nowMs = millis();
  const bool anyAdjust = cs.decreaseTimer || cs.increaseTimer;

  if (anyAdjust && (nowMs >= nextTimerAdjustMs))
  {
    uint32_t newDuration = timer.getDurationMs();

    if (cs.decreaseTimer)
    {
      if (newDuration <= timerStep)
        newDuration = 100ULL;
      else
        newDuration -= timerStep;
    }
    else if (cs.increaseTimer)
    {
      if (newDuration + timerStep > 9999000ULL)
        newDuration = 9999000ULL;
      else
        newDuration += timerStep;
    }
    Serial.printf("Adjusting timer. Current: %lu ms  New: %lu ms\n", timer.getDurationMs(), newDuration);
    timer.setDurationMs(newDuration);
    nextTimerAdjustMs = nowMs + repeatMs; // schedule next repeat
  }
  else if (!anyAdjust)
  {
    nextTimerAdjustMs = 0; // reset so next press acts immediately
  }

  updateDisplay(brightness, lamp.isOn(), motor1, motor2, timer, cs.anyDirectionConflict, debouncedFaultM1, debouncedFaultM2);

  delay(10);
}
