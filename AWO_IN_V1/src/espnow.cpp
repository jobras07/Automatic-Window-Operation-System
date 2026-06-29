#include "espnow.h"
#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

#define PACKET_TYPE_PING  0x01
#define PACKET_TYPE_PONG  0x02
#define PACKET_TYPE_DATA  0x03

static OutdoorPacket latestPacket;
static bool          newDataAvailable = false;
static unsigned long lastReceiveMs    = 0;

// ================================================================
// RECEIVE CALLBACK
// ================================================================
static void onDataReceive(const esp_now_recv_info_t* info,
                          const uint8_t* data, int len) {
    if (len != sizeof(OutdoorPacket)) return;

    OutdoorPacket* pkt = (OutdoorPacket*)data;

    if (pkt->packetType == PACKET_TYPE_PING) {
        // reply with PONG to the sender
        Serial.println("[ESPNOW] PING received, sending PONG");

        // register sender as peer temporarily
        esp_now_peer_info_t peer = {};
        memcpy(peer.peer_addr, info->src_addr, 6);
        peer.channel = 0;
        peer.encrypt = false;
        if (!esp_now_is_peer_exist(info->src_addr)) {
            esp_now_add_peer(&peer);
        }

        OutdoorPacket pong = {};
        pong.packetType = PACKET_TYPE_PONG;
        esp_now_send(info->src_addr, (uint8_t*)&pong, sizeof(OutdoorPacket));

    } else if (pkt->packetType == PACKET_TYPE_DATA) {
        memcpy(&latestPacket, data, sizeof(OutdoorPacket));
        newDataAvailable = true;
        lastReceiveMs    = millis();
    }
}

// ================================================================
// INIT
// ================================================================
void espnow_init() {
    // WiFi already connected at this point
    esp_wifi_set_ps(WIFI_PS_NONE);

    if (esp_now_init() != ESP_OK) {
        Serial.println("[ESPNOW] Init failed");
        return;
    }

    esp_now_register_recv_cb(onDataReceive);

    uint8_t channel;
    wifi_second_chan_t second;
    esp_wifi_get_channel(&channel, &second);
    Serial.printf("[ESPNOW] Ready on channel %d\n", channel);
}

// ================================================================
// UPDATE
// ================================================================
void espnow_update(SystemContext& ctx) {
    if (millis() - lastReceiveMs > ESPNOW_TIMEOUT_MS) {
        ctx.outdoor.isStale = true;
    }

    if (!newDataAvailable) return;
    newDataAvailable = false;

    ctx.outdoor.temperature  = latestPacket.temperature;
    ctx.outdoor.humidity     = latestPacket.humidity;
    ctx.outdoor.windSpeed    = latestPacket.windSpeed;
    ctx.outdoor.rainDetected = latestPacket.rainDetected;
    ctx.outdoor.lightLevel   = latestPacket.lightLevel;
    ctx.outdoor.isStale      = false;
    ctx.lastEspNowRxMs       = millis();
}