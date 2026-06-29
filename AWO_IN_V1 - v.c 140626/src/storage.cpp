#include "storage.h"
#include <Preferences.h>

static Preferences prefs;

void storage_init() {
    // load default passwords if not set yet
    prefs.begin("awo", false);
    if (!prefs.isKey("userPass")) {
        prefs.putString("userPass", DEFAULT_USER_PASSWORD);
    }
    if (!prefs.isKey("teachPass")) {
        prefs.putString("teachPass", DEFAULT_TEACHER_PASSWORD);
    }
    prefs.end();
    Serial.println("[STORAGE] Ready");
}

// ── WIFI ─────────────────────────────────────────────────────────
void storage_saveWiFi(const char* ssid, const char* password) {
    prefs.begin("awo", false);
    prefs.putString("ssid", ssid);
    prefs.putString("wifiPass", password);
    prefs.end();
    Serial.println("[STORAGE] WiFi credentials saved");
}

bool storage_loadWiFi(char* ssid, char* password) {
    prefs.begin("awo", true);
    if (!prefs.isKey("ssid")) {
        prefs.end();
        return false;
    }
    prefs.getString("ssid", ssid, 64);
    prefs.getString("wifiPass", password, 64);
    prefs.end();
    return true;
}

void storage_clearWiFi() {
    prefs.begin("awo", false);
    prefs.remove("ssid");
    prefs.remove("wifiPass");
    prefs.end();
    Serial.println("[STORAGE] WiFi credentials cleared");
}

// ── PASSWORDS ────────────────────────────────────────────────────
void storage_saveUserPassword(const char* password) {
    prefs.begin("awo", false);
    prefs.putString("userPass", password);
    prefs.end();
}

void storage_saveTeacherPassword(const char* password) {
    prefs.begin("awo", false);
    prefs.putString("teachPass", password);
    prefs.end();
}

void storage_loadUserPassword(char* password) {
    prefs.begin("awo", true);
    prefs.getString("userPass", password, 32);
    prefs.end();
}

void storage_loadTeacherPassword(char* password) {
    prefs.begin("awo", true);
    prefs.getString("teachPass", password, 32);
    prefs.end();
}

// ── SETTINGS ─────────────────────────────────────────────────────
void storage_saveFloat(const char* key, float value) {
    prefs.begin("awo", false);
    prefs.putFloat(key, value);
    prefs.end();
}

float storage_loadFloat(const char* key, float defaultVal) {
    prefs.begin("awo", true);
    float val = prefs.getFloat(key, defaultVal);
    prefs.end();
    return val;
}