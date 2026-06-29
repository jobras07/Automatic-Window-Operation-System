#pragma once

#include "config.h"

// ================================================================
// OUTDOOR SENSOR DATA (received via ESP-NOW)
// ================================================================
struct OutdoorData {
    float temperature;
    float humidity;
    float windSpeed;
    float lightLevel;
    bool rainDetected;
    bool isStale;           // true if data hasn't been received within ESPNOW_TIMEOUT_MS
};

// ================================================================
// INDOOR SENSOR DATA
// ================================================================
struct IndoorData {
    float temperature;
    float humidity;
};

// ================================================================
// SYSTEM CONTEXT
// — single struct passed around everywhere so all modules
//   have access to current system status
// ================================================================
struct SystemContext {
    // current state and mode
    SystemState state;
    SystemMode  mode;
    SystemMode  modeBeforeForceClose;   // remembered when FORCE_CLOSING kicks in


    // UI state for LCD menu
    UIState    uiState;
    SystemMode selectedMode;
    bool       windowConfirmed;
    bool autoConfirmed;    // true only after user confirms settings and enters AUTO

    // window position
    int currentPosition;    // 0-100%
    int targetPosition;     // 0-100%
    int manualTargetPreview; // for showing target positio in manual mode (display)

    // sensor data
    IndoorData  indoor;
    OutdoorData outdoor;

    // flags
    bool estopPressed;
    bool limitOpenHit;
    bool limitClosedHit;
    bool naturalEventActive;    // true if rain or wind over threshold

    // settings (loaded from SD, configurable via menu)
    float setTargetTemp;
    float setTempRange;

    // timing
    unsigned long lastSensorPollMs;
    unsigned long lastLogMs;
    unsigned long lastEspNowRxMs;
    unsigned long estopResetHoldStartMs;

    // current protection
    bool overcurrentDetected;
    bool overcurrentRetried;
    bool overcurrentEnabled;   
    unsigned long overcurrentTimeMs;

    // buzzer trigger
    bool buzzerTrigger;

    // web control
    bool teacherOverride;
    bool webUserLoggedIn;
    char userPassword[32];
    char teacherPassword[32];

    // IP show on screen
    bool showingIPScreen;
    
    // wind threshold (configurable via web)
    float windThreshold;
};

// ================================================================
// FUNCTION DECLARATIONS
// ================================================================

// call once in setup()
void stateMachine_init(SystemContext& ctx);

// call every loop() iteration — drives all state transitions
void stateMachine_update(SystemContext& ctx);

// helpers — can be called from other modules
bool naturalEventActive(const SystemContext& ctx);
int  calculateTargetPosition(const SystemContext& ctx);