#include "logger.h"
#include <Arduino.h>
#include <SD.h>
#include <SPI.h>

#define LOG_FILENAME "/log.csv"

static bool sdReady = false;

// ================================================================
// HELPERS
// ================================================================
static String stateName(SystemState state) {
    switch (state) {
        case STATE_IDLE:           return "IDLE";
        case STATE_ADJUSTING:      return "ADJUSTING";
        case STATE_FULLY_OPEN:     return "FULLY_OPEN";
        case STATE_FULLY_CLOSED:   return "FULLY_CLOSED";
        case STATE_FORCE_CLOSING:  return "FORCE_CLOSING";
        case STATE_EMERGENCY_STOP: return "EMERGENCY_STOP";
        default:                   return "UNKNOWN";
    }
}

static String modeName(SystemMode mode) {
    switch (mode) {
        case MODE_MANUAL:    return "MANUAL";
        case MODE_AUTOMATIC: return "AUTOMATIC";
        case MODE_LOCK:      return "LOCK";
        default:             return "UNKNOWN";
    }
}

// ================================================================
// INIT
// ================================================================
void logger_init() {
    SPI.begin(PIN_SD_SCK, PIN_SD_MISO, PIN_SD_MOSI, PIN_SD_CS);
    if (!SD.begin(PIN_SD_CS)) {
        Serial.println("[LOGGER] SD card init failed");
        sdReady = false;
        return;
    }

    sdReady = true;
    Serial.println("[LOGGER] SD card ready");

    // write header if file doesn't exist yet
    if (!SD.exists(LOG_FILENAME)) {
        File f = SD.open(LOG_FILENAME, FILE_WRITE);
        if (f) {
            f.println("timestamp_ms,indoor_temp,humidity,outdoor_temp,wind_speed,rain,position,state,mode");
            f.close();
        }
    }
}

// ================================================================
// UPDATE — call every loop(), writes on interval
// ================================================================
void logger_update(SystemContext& ctx) {
    if (!sdReady) return;

    unsigned long now = millis();
    if (now - ctx.lastLogMs < LOG_INTERVAL_MS) return;
    ctx.lastLogMs = now;

    File f = SD.open(LOG_FILENAME, FILE_APPEND);
    if (!f) {
        Serial.println("[LOGGER] Failed to open log file");
        return;
    }

    String row = "";
    row += String(now)                          + ",";
    row += String(ctx.indoor.temperature, 1)    + ",";
    row += String(ctx.indoor.humidity, 1)       + ",";
    row += String(ctx.outdoor.temperature, 1)   + ",";
    row += String(ctx.outdoor.windSpeed, 1)     + ",";
    row += String(ctx.outdoor.rainDetected)     + ",";
    row += String(ctx.currentPosition)          + ",";
    row += stateName(ctx.state)                 + ",";
    row += modeName(ctx.mode);

    f.println(row);
    f.close();
}