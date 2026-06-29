#include "sensors.h"
#include <Arduino.h>

// ================================================
// DHT
// ================================================

static DHT_Async dht(PIN_DHT22, DHT_TYPE_22);


// ================================================
// ANEMOMETER
// ================================================
static volatile unsigned int pulseCount = 0;
static unsigned long lastWindCalcMs = 0;            
#define WIND_CALC_INTERVAL_MS 1000 // calculate wind speed every 1 second
#define KMH_PER_PULSE_PER_SEC 2.4f // conversion factor: 1 pulse/sec = 2.4 km/h

void IRAM_ATTR anemometerISR() {
    static unsigned long lastPulseMs = 0;
    unsigned long now = millis();
    if (now - lastPulseMs > 10) {  // 10ms debounce
        pulseCount++;
        lastPulseMs = now;
    }
}

// ================================================================
// INIT
// ================================================================
void sensors_init() {
    pinMode(PIN_RAIN_SENSOR,  INPUT);
    pinMode(PIN_ANEMOMETER,   INPUT);
    pinMode(PIN_LIGHT_SENSOR, INPUT);

    attachInterrupt(digitalPinToInterrupt(PIN_ANEMOMETER), anemometerISR, RISING);
}

// ================================================================
// UPDATE
// ================================================================
void sensors_update(OutdoorData& data) {

    // --- DHT22 (non blocking) ---
    float temperature, humidity;
    if (dht.measure(&temperature, &humidity)) {
        data.temperature = temperature;
        data.humidity    = humidity;
    }

    // --- Rain sensor ---
    data.rainDetected = analogRead(PIN_RAIN_SENSOR) < RAIN_THRESHOLD;

    // --- Light sensor ---
    data.lightLevel = analogRead(PIN_LIGHT_SENSOR);

    // --- Anemometer (calculate every second) ---
    unsigned long now = millis();
    if (now - lastWindCalcMs >= WIND_CALC_INTERVAL_MS) {
        // disable interrupt briefly to safely read pulseCount
        detachInterrupt(digitalPinToInterrupt(PIN_ANEMOMETER));
        unsigned int pulses = pulseCount;
        pulseCount          = 0;
        attachInterrupt(digitalPinToInterrupt(PIN_ANEMOMETER), anemometerISR, RISING);

        // Calculate wind speed in km/h
        float pulsesPerSecond = pulses / (WIND_CALC_INTERVAL_MS / 1000.0f);
        data.windSpeed = pulsesPerSecond * KMH_PER_PULSE_PER_SEC;

        lastWindCalcMs = now;
    }
}