#include "espnow.h"
#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

#define PACKET_TYPE_PING  0x01
#define PACKET_TYPE_PONG  0x02
#define PACKET_TYPE_DATA  0x03

static uint8_t indoorMac[]  = INDOOR_MAC;
static bool    espnowReady  = false;
static bool    channelFound = false;
static int     currentChannel = 1;

static volatile bool pongReceived = false;

// ================================================================
// CALLBACKS
// ================================================================
static void onDataSent(const uint8_t* mac, esp_now_send_status_t status) {
    if (status == ESP_NOW_SEND_SUCCESS) {
        Serial.println("[ESPNOW] Sent OK");
    } else {
        Serial.println("[ESPNOW] Send Failed");
    }
}

static void onDataReceive(const esp_now_recv_info_t* info,
                          const uint8_t* data, int len) {
    if (len == sizeof(OutdoorPacket)) {
        OutdoorPacket* pkt = (OutdoorPacket*)data;
        if (pkt->packetType == PACKET_TYPE_PONG) {
            pongReceived = true;
            Serial.println("[ESPNOW] PONG received!");
        }
    }
}

// ================================================================
// CHANNEL SCAN
// ================================================================
static bool tryChannel(int channel) {
    Serial.printf("[ESPNOW] Trying channel %d...\n", channel);
    
    esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
    delay(50);

    // remove old peer and re-add on new channel
    esp_now_del_peer(indoorMac);
    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, indoorMac, 6);
    peer.channel = channel;
    peer.encrypt = false;
    esp_now_add_peer(&peer);

    // send ping
    OutdoorPacket ping = {};
    ping.packetType = PACKET_TYPE_PING;
    pongReceived    = false;

    esp_now_send(indoorMac, (uint8_t*)&ping, sizeof(OutdoorPacket));

    // wait for pong
    unsigned long start = millis();
    while (millis() - start < 200) {
        if (pongReceived) return true;
        delay(10);
    }
    return false;
}

// ================================================================
// INIT
// ================================================================
void espnow_init() {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();

    if (esp_now_init() != ESP_OK) {
        Serial.println("[ESPNOW] Init failed");
        return;
    }

    esp_now_register_send_cb(onDataSent);
    esp_now_register_recv_cb(onDataReceive);

    // scan channels until indoor unit responds
    Serial.println("[ESPNOW] Scanning for indoor unit...");
    while (!channelFound) {
        for (int ch = 1; ch <= 13; ch++) {
            if (tryChannel(ch)) {
                currentChannel = ch;
                channelFound   = true;
                espnowReady    = true;
                Serial.printf("[ESPNOW] Indoor unit found on channel %d\n", ch);
                break;
            }
        }
        if (!channelFound) {
            Serial.println("[ESPNOW] No response, retrying...");
            delay(1000);
        }
    }
}

// ================================================================
// UPDATE
// ================================================================
#define SEND_INTERVAL_MS 2000
static unsigned long lastSendMs = 0;

void espnow_update(OutdoorData& data) {
    if (!espnowReady) return;

    unsigned long now = millis();
    if (now - lastSendMs < SEND_INTERVAL_MS) return;
    lastSendMs = now;

    OutdoorPacket packet;
    packet.packetType   = PACKET_TYPE_DATA;
    packet.temperature  = data.temperature;
    packet.humidity     = data.humidity;
    packet.windSpeed    = data.windSpeed;
    packet.rainDetected = data.rainDetected;
    packet.lightLevel   = data.lightLevel;

    esp_now_send(indoorMac, (uint8_t*)&packet, sizeof(OutdoorPacket));
}