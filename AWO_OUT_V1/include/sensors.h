#pragma once
#include "config.h"
#include "DHT_Async.h"

struct OutdoorData {
    float temperature;
    float humidity;
    bool rainDetected;
    float windSpeed;
    float lightLevel;
};

void sensors_init();
void sensors_update(OutdoorData &data);