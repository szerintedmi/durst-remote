#include "EspNowMesh.h"

#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

#include "DurstProto.h" // for BROADCAST_MAC constant

namespace
{
  bool s_inited = false;

  bool addBroadcastPeer(wifi_interface_t ifx)
  {
    esp_now_peer_info_t peer{};
    memcpy(peer.peer_addr, DurstProto::BROADCAST_MAC, 6);
    peer.ifidx = ifx;
    peer.channel = 0;   // follow current primary channel
    peer.encrypt = false;

    esp_err_t err = esp_now_add_peer(&peer);
    if (err == ESP_OK)
      return true;

    // If peer already exists, treat as success for idempotency
    if (err == ESP_ERR_ESPNOW_EXIST)
      return true;

    Serial.printf("EspNowMesh: esp_now_add_peer ifx=%d failed: %d\n", (int)ifx, (int)err);
    return false;
  }
} // namespace

namespace EspNowMesh
{
  bool begin()
  {
    if (!s_inited)
    {
      esp_err_t err = esp_now_init();
      if (err != ESP_OK)
      {
        Serial.printf("EspNowMesh: esp_now_init failed: %d\n", (int)err);
        return false;
      }
      s_inited = true;
    }

    // Add broadcast peers based on current WiFi mode
    wifi_mode_t mode = WiFi.getMode();
    bool ok = true;
    if (mode == WIFI_MODE_AP || mode == WIFI_MODE_APSTA)
      ok &= addBroadcastPeer(WIFI_IF_AP);
    if (mode == WIFI_MODE_STA || mode == WIFI_MODE_APSTA)
      ok &= addBroadcastPeer(WIFI_IF_STA);

    uint8_t pri = 0; wifi_second_chan_t sec = WIFI_SECOND_CHAN_NONE;
    esp_wifi_get_channel(&pri, &sec);
    Serial.printf("EspNowMesh: init ok=%s mode=%d ch=%u sec=%d\n", ok?"true":"false", (int)mode, pri, (int)sec);

    return ok;
  }
}

