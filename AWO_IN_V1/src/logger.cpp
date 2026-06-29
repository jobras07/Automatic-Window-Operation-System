#include "logger.h"
#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include <time.h>

static bool sdReady = false;

// ================================================================
// TIME HELPERS
// ================================================================
static String getTimeString() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) return "00:00:00";
    char buf[10];
    strftime(buf, sizeof(buf), "%H:%M:%S", &timeinfo);
    return String(buf);
}

static String getDateFilename() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) return "/logs/unknown.csv";
    char buf[32];
    strftime(buf, sizeof(buf), "/logs/%Y-%m-%d.csv", &timeinfo);
    return String(buf);
}

// ================================================================
// HELPERS
// ================================================================
static String stateName(SystemState state) {
    switch (state) {
        case STATE_IDLE:           return "IDLE";
        case STATE_ADJUSTING:      return "ADJUSTING";
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

    // create /logs/ directory if it doesn't exist
    if (!SD.exists("/logs")) {
        SD.mkdir("/logs");
        Serial.println("[LOGGER] Created /logs directory");
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

    // get current date filename
    String filename = getDateFilename();

    // write header if new file
    if (!SD.exists(filename.c_str())) {
        File f = SD.open(filename.c_str(), FILE_WRITE);
        if (f) {
            f.println("time,indoor_temp,humidity,outdoor_temp,wind_speed,rain,position,state,mode");
            f.close();
        }
    }

    File f = SD.open(filename.c_str(), FILE_APPEND);
    if (!f) {
        Serial.println("[LOGGER] Failed to open log file");
        return;
    }

    String row = "";
    row += getTimeString()                      + ",";
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