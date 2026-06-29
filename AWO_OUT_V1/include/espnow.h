#pragma once
#include "config.h"
#include "sensors.h"

#define INDOOR_MAC {0xE8, 0x06, 0x90, 0x98, 0x5E, 0x64}  // replace with your MAC

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
void espnow_update(OutdoorData& data);