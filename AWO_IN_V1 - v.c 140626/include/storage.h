#pragma once
#include <Arduino.h>
#include "config.h"

void     storage_init();

// WiFi credentials
void     storage_saveWiFi(const char* ssid, const char* password);
bool     storage_loadWiFi(char* ssid, char* password);
void     storage_clearWiFi();

// Passwords
void     storage_saveUserPassword(const char* password);
void     storage_saveTeacherPassword(const char* password);
void     storage_loadUserPassword(char* password);
void     storage_loadTeacherPassword(char* password);

// Settings
void  storage_saveFloat(const char* key, float value);
float storage_loadFloat(const char* key, float defaultVal);