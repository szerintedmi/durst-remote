// Minimal slave node: joins same WiFi/ESP-NOW mesh and prints display messages
#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_now.h>
#include <rgb_lcd.h>
#include <esp_log.h>

// For detecting IDF version to select correct ESP-NOW callback signature
#if __has_include(<esp_idf_version.h>)
#include <esp_idf_version.h>
#else
// Fallback if header not available (treat as older core)
#define ESP_IDF_VERSION_VAL(major, minor, patch) 0
#define ESP_IDF_VERSION 0
#endif

#include "DurstProto.h"
#include "TM1638plusWrapper.h"
#include "DisplayMux.h"

// TM1638 pins (same defaults as master)
constexpr uint8_t TM1638_CLK_PIN = 32;
constexpr uint8_t TM1638_DIO_PIN = 14;
constexpr uint8_t TM1638_STB_PIN = 13;
constexpr bool IS_TM1638_HIGH_FREQ = true;

// ---- TM1638 Display and/or Grove LCD Display ----
// Same default pins as master; harmless if no board attached
TM1638plusWrapper tm(TM1638_STB_PIN, TM1638_CLK_PIN, TM1638_DIO_PIN, IS_TM1638_HIGH_FREQ);
rgb_lcd lcd;
DisplayMux displays(&tm, &lcd); // not broadcasting (disbaled by default)

static uint32_t lastBroadcastedSeq = 0;
static uint32_t lastBroadcastReceivedMs = 0;
static bool isConnected = false;
static uint32_t broadcastLostCount = 0;

// TODO: same vars in DisplayMux just not maintained when not broadcasting. Consider using those and remove these
uint8_t lastBroadcastedSegBrightness_ = DEFAULT_SEG_BRIGHTNESS;
char lastBroadcastedSegText_[11] = "";
char lastBroadcastedLcdLine1_[17] = "";
char lastBroadcastedLcdLine2_[17] = "";

// Helper to get debug line for display
const char *getDebugLine()
{
  static char line1OverrideForDebug[17] = {0};
  snprintf(line1OverrideForDebug, sizeof(line1OverrideForDebug), "%5d  lost:%4d", lastBroadcastedSeq, broadcastLostCount);
  return line1OverrideForDebug;
}

// ESP-NOW recv callback signature changed in IDF 5/Arduino-ESP32 v3.x
// - New: onEspNowRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len)
// - Old: onEspNowRecv(const uint8_t *mac_addr, const uint8_t *data, int len)
static void onDisplayBroadcast(const MsgV1 &m)
{
  const MsgV1 *msg = &m;

  // TODO: hasAnyChanged here is only for debugging also duplicate of logic in DisplayMux::broadcastIfDue
  const bool hasAnyChanged =
      (msg->brightness != lastBroadcastedSegBrightness_) ||
      (memcmp(msg->segText, lastBroadcastedSegText_, sizeof(msg->segText)) != 0) ||
      (memcmp(msg->lcdLine1, lastBroadcastedLcdLine1_, sizeof(msg->lcdLine1)) != 0) ||
      (memcmp(msg->lcdLine2, lastBroadcastedLcdLine2_, sizeof(msg->lcdLine2)) != 0);

  if (lastBroadcastedSeq > msg->seq)
  {
    Serial.printf("onEspNowRecv: Likely master restart - out of order seq: %d last_broadcast_seq: %d\n", msg->seq, lastBroadcastedSeq);
    lastBroadcastedSeq = 0;
    broadcastLostCount = 0;
  }
  else if (lastBroadcastedSeq != 0 && msg->seq != lastBroadcastedSeq + 1)
  {
    uint32_t lost_count = msg->seq - lastBroadcastedSeq - 1;
    broadcastLostCount += lost_count;
    Serial.printf("onEspNowRecv: lost %d messages. lastBroadcastedSeq: %d current seq: %d Total lost: %d\n",
                  lost_count, lastBroadcastedSeq, msg->seq, broadcastLostCount);
  }

  displays.displayAndBroadCastTexts(msg->brightness, msg->segText, getDebugLine(), msg->lcdLine2);

  lastBroadcastedSeq = msg->seq;
  lastBroadcastedSegBrightness_ = msg->brightness;
  lastBroadcastReceivedMs = millis();
  isConnected = true;
  memcpy(lastBroadcastedSegText_, msg->segText, sizeof(msg->segText));
  memcpy(lastBroadcastedLcdLine1_, msg->lcdLine1, sizeof(msg->lcdLine1));
  memcpy(lastBroadcastedLcdLine2_, msg->lcdLine2, sizeof(msg->lcdLine2));

  displays.displayAndBroadCastTexts(msg->brightness, msg->segText, getDebugLine(), msg->lcdLine2);

  if (hasAnyChanged)
  {
    Serial.printf("onEspNowRecv: text / brightness changed seq=%lu brightness=%d segText='%s' lcdLine1='%s' lcdLine2='%s' lcdLine1OverrideForDebug='%s'\n",
                  (unsigned long)msg->seq, msg->brightness, msg->segText, msg->lcdLine1, msg->lcdLine2, getDebugLine());
  }
}

void onConnectionLost()
{
  isConnected = false;
  displays.displayAndBroadCastTexts(lastBroadcastedSegBrightness_, "conn.err ",
                                    getDebugLine(), "Connection lost ");

  Serial.println("mainSlave.onConnectionLost: Connection lost to master");
}

void setup()
{
  Serial.begin(115200);

  displays.begin();

  Serial.println("[SLAVE] Joining mesh...");
  displays.displayAndBroadCastTexts(lastBroadcastedSegBrightness_, "BOOTING ",
                                    getDebugLine(), "Booting...   ");

  // Configure STA
  WiFi.mode(WIFI_STA);
  esp_wifi_set_ps(WIFI_PS_NONE);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(false);
  // WiFi.begin(MESH_SSID, MESH_PASS); don't need for mesh mode (using onyl softAP) and connecting to AP is slow to connect and would require to manage channel changes

  // Init ESP-NOW (will be ready regardless of STA state)
  if (esp_now_init() == ESP_OK)
  {
    uint8_t pri = 0;
    wifi_second_chan_t sec = WIFI_SECOND_CHAN_NONE;
    esp_wifi_get_channel(&pri, &sec);
    Serial.printf("[SLAVE] STA ESP NOW INIT SUCCESS. Wifi Channel: %d. esp channel: %d\n", WiFi.channel(), pri);
    DurstProto::setOnDisplayBroadcast(&onDisplayBroadcast);
    displays.displayAndBroadCastTexts(lastBroadcastedSegBrightness_, "CONNECT ",
                                      getDebugLine(), "Connecting...   ");
  }
  else
  {
    displays.displayAndBroadCastTexts(lastBroadcastedSegBrightness_, "ESP-ERR ",
                                      getDebugLine(), "ESPNow init fail");
    Serial.println("[SLAVE] ESP-NOW init failed");
  }
}

void loop()
{
  if (millis() - lastBroadcastReceivedMs > 2000 && isConnected)
    onConnectionLost();

  delay(300); // long delay is fine until we only check for lost connection in loop
}
