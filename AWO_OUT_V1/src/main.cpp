#include <Arduino.h>
#include "sensors.h"
#include "espnow.h"

OutdoorData outdoorData;

void setup() {
    Serial.begin(115200);
    sensors_init();
    espnow_init();
    Serial.println("[MAIN] Outdoor unit ready");
}

void loop() {
    sensors_update(outdoorData);
    espnow_update(outdoorData);

    static unsigned long lastPrintMs = 0;
    if (millis() - lastPrintMs > 2000) {
        lastPrintMs = millis();
        int rainValue = analogRead(PIN_RAIN_SENSOR);
        Serial.printf("[MAIN] Temp: %.1f C, Humidity: %.1f %%, Rain: %d, Light: %.1f, Wind: %.1f km/h\n",
            outdoorData.temperature,
            outdoorData.humidity,
            outdoorData.rainDetected,
            outdoorData.lightLevel,
            outdoorData.windSpeed);
    }
}