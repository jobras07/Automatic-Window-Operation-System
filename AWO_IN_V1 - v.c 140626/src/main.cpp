#include <Arduino.h>
#include <WiFi.h>
#include <Wire.h>
#include "config.h"
#include "statemachine.h"
#include "motor.h"
#include "sensors.h"
#include "encoder.h"
#include "display.h"
#include "logger.h"
#include "espnow.h"
#include "buzzer.h"
#include "storage.h"
#include "portal.h"
#include "webserver.h"


SystemContext sysCtx;


// This connects to WiFi using saved credentials, returns true if successful
bool connectToWiFi() {
    char ssid[64], password[64];
    if (!storage_loadWiFi(ssid, password)) {
        Serial.println("[WIFI] No credentials saved");
        return false;
    }

    Serial.printf("[WIFI] Connecting to %s...\n", ssid);
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);

    int timeout = 0;
    while (WiFi.status() != WL_CONNECTED && timeout < 20) {
        delay(500);
        timeout++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("[WIFI] Connected! IP: %s\n", WiFi.localIP().toString().c_str());
        return true;
    }

    Serial.println("[WIFI] Failed to connect");
    return false;
}

void setup() {
    Serial.begin(115200);
    delay(500);

Wire.begin(PIN_SDA, PIN_SCL);
delay(100);

storage_init();
storage_loadUserPassword(sysCtx.userPassword);
storage_loadTeacherPassword(sysCtx.teacherPassword);
sysCtx.setTargetTemp  = storage_loadFloat("targetTemp", TEMP_TARGET);
sysCtx.setTempRange   = storage_loadFloat("tempRange",  TEMP_RANGE);
sysCtx.windThreshold  = storage_loadFloat("windThresh", WIND_SPEED_MAX);
stateMachine_init(sysCtx);
motor_init();               
sensors_init();          
encoder_init(); 
buzzer_init();
display_init();
 // try connecting to saved WiFi
    bool wifiOk = connectToWiFi();

    if (!wifiOk) {
        // start captive portal
        portal_start();
        Serial.println("[MAIN] Waiting for WiFi setup...");

        // show on LCD
        // display will init after WiFi is sorted
        display_showMessage("Setup Wifi", "AWO_Setup");
        while (!portal_update(sysCtx)) { delay(10);}
        portal_stop();
        delay(500);
    }
    // WiFi connected — start ESP-NOW on same channel
espnow_init();  
logger_init();
webserver_init(sysCtx);


    sysCtx.currentPosition      = sensors_getPosition();
    sysCtx.manualTargetPreview  = sysCtx.currentPosition;
    sysCtx.targetPosition       = sysCtx.currentPosition;

    Serial.println("[MAIN] System ready");
}

void loop() {
    sensors_update(sysCtx);
    espnow_update(sysCtx);
    encoder_update(sysCtx);
    stateMachine_update(sysCtx);
    buzzer_update(sysCtx);
    display_update(sysCtx);
    logger_update(sysCtx);
    webserver_update(sysCtx);

        // temporary debug — remove later
    static unsigned long lastPrintMs = 0;
    if (millis() - lastPrintMs > 2000) {
        lastPrintMs = millis();
        Serial.print("[OUTDOOR] Temp: ");    Serial.println(sysCtx.outdoor.temperature);
        Serial.print("[OUTDOOR] Humidity: "); Serial.println(sysCtx.outdoor.humidity);
        Serial.print("[OUTDOOR] Wind: ");    Serial.println(sysCtx.outdoor.windSpeed);
        Serial.print("[OUTDOOR] Rain: ");    Serial.println(sysCtx.outdoor.rainDetected);
        Serial.print("[OUTDOOR] Light: ");   Serial.println(sysCtx.outdoor.lightLevel);
        Serial.print("[OUTDOOR] Stale: ");   Serial.println(sysCtx.outdoor.isStale);
        Serial.println("---");
    }

    static unsigned long lastPrintMs_1 = 0;
    if (millis() - lastPrintMs_1 > 2000) {
        lastPrintMs_1 = millis();
        Serial.print("[INDOOR] Temp: ");    Serial.println(sysCtx.indoor.temperature);
        Serial.print("[INDOOR] Humidity: "); Serial.println(sysCtx.indoor.humidity);
        Serial.println("---");
    }
}