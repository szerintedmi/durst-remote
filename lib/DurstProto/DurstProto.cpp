#include "DurstProto.h"

#include <Arduino.h>

// For detecting IDF version to select correct ESP-NOW callback signature
#if __has_include(<esp_idf_version.h>)
#include <esp_idf_version.h>
#else
#define ESP_IDF_VERSION_VAL(major, minor, patch) 0
#define ESP_IDF_VERSION 0
#endif

namespace
{
  DurstProto::DisplayBroadcastHandler s_onDisplayBroadcastHandler = nullptr;
  DurstProto::TmStateBroadcastHandler s_onTmStateBroadcastHandler = nullptr;
  bool s_recvCbAttached = false;

  // Minimal header view for quick checks (matches start of MsgV1)
  struct __attribute__((packed)) HeaderView
  {
    uint8_t magic;
    uint8_t version;
    uint8_t cmd;
    uint8_t flags;
    uint32_t seq;
  };

#if defined(ESP_IDF_VERSION) && (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0))
  void onEspNowRecvGeneric(const esp_now_recv_info_t *info, const uint8_t *data, int len)
  {
    (void)info; // silence if unused on new core
#else
  void onEspNowRecvGeneric(const uint8_t *mac_addr, const uint8_t *data, int len)
  {
    (void)mac_addr; // silence if unused on old core
#endif

    if (len < (int)sizeof(HeaderView))
    {
      Serial.printf("DurstProto: invalid header size received. Actual: %d Expected: %d\n",
                    len, (int)sizeof(HeaderView));
      return;
    }

    const HeaderView *hdr = reinterpret_cast<const HeaderView *>(data);
    if (hdr->magic != PROTO_MAGIC || hdr->version != PROTO_VERSION)
    {
      Serial.printf("DurstProto: bad magic/version. Actual: magic=0x%02X version=%u. Expected: magic=%u version=%d \n",
                    (unsigned)hdr->magic, (unsigned)hdr->version, PROTO_MAGIC, PROTO_VERSION);
      return;
    }

    switch (hdr->cmd)
    {
    case CMD_DISPLAY_TEXT:
      if (len != (int)sizeof(MsgV1))
      {
        Serial.printf("DurstProto: DISPLAY_TEXT bad length. Expected: %d Actual: %d\n", sizeof(MsgV1), len);
        return;
      }
      if (s_onDisplayBroadcastHandler)
      {
        const MsgV1 *msg = reinterpret_cast<const MsgV1 *>(data);
        s_onDisplayBroadcastHandler(*msg);
      }
      break;
    case CMD_TM_STATE:
      if (len != (int)sizeof(MsgTmStateV1))
      {
        Serial.printf("DurstProto: TM_STATE bad length. Expected: %d Actual: %d\n", sizeof(MsgTmStateV1), len);
        return;
      }
      if (s_onTmStateBroadcastHandler)
      {
        const MsgTmStateV1 *msg = reinterpret_cast<const MsgTmStateV1 *>(data);
        s_onTmStateBroadcastHandler(*msg);
      }
      break;

    default:
      Serial.printf("DurstProto: unknown cmd received: %u\n", (unsigned)hdr->cmd);
      break;
    }
  }
} // namespace

namespace DurstProto
{
  void setOnDisplayBroadcast(DisplayBroadcastHandler h)
  {
    s_onDisplayBroadcastHandler = h;
    if (!s_recvCbAttached)
    {
      esp_now_register_recv_cb(&onEspNowRecvGeneric);
      s_recvCbAttached = true;
    }
  }

  void setOnTmStateBroadcast(TmStateBroadcastHandler h)
  {
    s_onTmStateBroadcastHandler = h;
    if (!s_recvCbAttached)
    {
      esp_now_register_recv_cb(&onEspNowRecvGeneric);
      s_recvCbAttached = true;
    }
  }

  bool sendTo(const uint8_t mac[6], const void *data, size_t len)
  {
    esp_err_t err = esp_now_send(mac, reinterpret_cast<const uint8_t *>(data), len);
    if (err != ESP_OK)
      Serial.printf("DurstProto: esp_now_send failed: %d\n", err);
    return err == ESP_OK;
  }

  bool broadcastDisplayText(const MsgV1 &msg)
  {
    return sendTo(BROADCAST_MAC, &msg, sizeof(msg));
  }

  bool broadcastTmState(const MsgTmStateV1 &msg)
  {
    bool ok = sendTo(BROADCAST_MAC, &msg, sizeof(msg));
    return ok;
  }
} // namespace DurstProto
