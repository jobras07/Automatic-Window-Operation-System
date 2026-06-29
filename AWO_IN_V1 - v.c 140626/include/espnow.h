#pragma once
#include "config.h"
#include <Arduino.h>
#include "statemachine.h"

#define PACKET_TYPE_PING  0x01
#define PACKET_TYPE_PONG  0x02
#define PACKET_TYPE_DATA  0x03

struct OutdoorPacket {
    uint8_t packetType;
    float   temperature;
    float   humidity;
    float   windSpeed;
    bool    rainDetected;
    float   lightLevel;
};

void espnow_init();
void espnow_update(SystemContext& ctx);