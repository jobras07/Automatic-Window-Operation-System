#include "display.h"
#include "config.h"
#include <LiquidCrystal_I2C.h>
#include <Wire.h>
#include <Arduino.h>
#include <WiFi.h>

// ================================================================
// LCD OBJECT — hidden from other files
// ================================================================
static LiquidCrystal_I2C lcd(0x27, 16, 2);

// ================================================================
// BLINK STATE — hidden from other files
// ================================================================
static bool          blinkVisible = true;
static unsigned long lastBlinkMs  = 0;
#define BLINK_INTERVAL_MS 500

// ================================================================
// LAST DRAWN SNAPSHOT
// only redraws when something changes
// ================================================================
struct DisplaySnapshot {
    UIState    uiState;
    SystemMode mode;
    int        position;
    float      tempTarget;
    float      tempRange;
    bool       blink;
    bool       confirmed;
};

static DisplaySnapshot lastDrawn = { (UIState)-1, MODE_MANUAL, -999, -1, -1, true, false };

// ================================================================
// HELPERS
// ================================================================
static void lcdRow(int row, String text) {
    while (text.length() < 16) text += " ";
    lcd.setCursor(0, row);
    lcd.print(text);
}

static String modeName(SystemMode mode) {
    switch (mode) {
        case MODE_MANUAL:    return "MANUAL";
        case MODE_AUTOMATIC: return "AUTOMATIC";
        case MODE_LOCK:      return "LOCK";
        case MODE_IP:               return "IP ADDRESS";
        default:             return "";
    }
}

// EASY MESSAGE FUNCTION
void display_showMessage(const char* line1, const char* line2) {
    lcdRow(0, line1);
    lcdRow(1, line2);
}

// ================================================================
// REDRAW — writes to LCD based on current ctx
// ================================================================
static void redraw(SystemContext& ctx) {
    switch (ctx.uiState) {

        case UI_MANUAL:
            lcdRow(0, "MANUAL   C:" + String(ctx.indoor.temperature, 1) + "C");
            if (ctx.windowConfirmed) {
                lcdRow(1, "Open: " + String(ctx.currentPosition) + "%");
                Serial.println(String(ctx.currentPosition));  // TEMP DEBUGGING)
            } else {
                if (blinkVisible) lcdRow(1, "Open: " + String(ctx.manualTargetPreview) + "%");
                else              lcdRow(1, "Open:");
            }
            break;

        case UI_SELECT_MODE:
            lcdRow(0, "SELECT   C:" + String(ctx.indoor.temperature, 1) + "C");
            if (blinkVisible) lcdRow(1, modeName(ctx.selectedMode));
            else              lcdRow(1, "");
            break;

        case UI_SET_TEMP_TARGET:
            lcdRow(0, "Set Target Temp");
            if (blinkVisible) lcdRow(1, "Temp: " + String(ctx.setTargetTemp, 1) + " C");
            else              lcdRow(1, "Temp:      C");
            break;

        case UI_SET_TEMP_RANGE:
            lcdRow(0, "Set Temp Range");
            if (blinkVisible) lcdRow(1, "Range: +/-" + String(ctx.setTempRange, 1) + " C");
            else              lcdRow(1, "Range: +/-    C");
            break;

        case UI_AUTOMATIC:
            lcdRow(0, "AUTO     C:" + String(ctx.indoor.temperature, 1) + "C");
            lcdRow(1, "T:" + String(ctx.setTargetTemp, 1) + " R:" + String(ctx.setTempRange, 1));
            break;

        case UI_LOCK:
            lcdRow(0, "LOCK     C:" + String(ctx.indoor.temperature, 1) + "C");
            lcdRow(1, "Window Closed");
            break;
        
        case UI_EMERGENCY_STOP:
            lcdRow(0, "!! EMERGENCY  !!");
            lcdRow(1, "!!    STOP    !!");
            break;  

        case UI_OVERCURRENT:
            lcdRow(0, "! OVERCURRENT  !");
            lcdRow(1, "!   DETECTED   !");
            break;

        case UI_RAIN_WIND_ALERT:
            lcdRow(0, "!! RAIN/WIND  !!");
            lcdRow(1, "!!   ALERT    !!");
            break;

        case UI_IP_ADDRESS:
            lcdRow(0, "IP Address:");
            lcdRow(1, WiFi.localIP().toString().c_str());
            break;
    }
}

// ================================================================
// PUBLIC FUNCTIONS
// ================================================================
void display_init() {
    // Serial.printf("[DISPLAY] SDA: %d, SCL: %d\n", PIN_SDA, PIN_SCL);
    // Wire.begin(PIN_SDA, PIN_SCL);
    lcd.init();
    lcd.backlight();
    lcd.noCursor();
    lcd.noBlink();
}

void display_update(SystemContext& ctx) {
    // update blink timer
    if (millis() - lastBlinkMs > BLINK_INTERVAL_MS) {
        blinkVisible = !blinkVisible;
        lastBlinkMs  = millis();
    }

    // snapshot current state
    DisplaySnapshot current = {
        ctx.uiState,
        ctx.selectedMode,
        ctx.currentPosition,
        ctx.setTargetTemp,
        ctx.setTempRange,
        blinkVisible,
        ctx.windowConfirmed
    };

    // only redraw if something changed
    bool changed = (current.uiState    != lastDrawn.uiState)
                || (current.mode       != lastDrawn.mode)
                || (current.position   != lastDrawn.position)
                || (current.tempTarget != lastDrawn.tempTarget)
                || (current.tempRange  != lastDrawn.tempRange)
                || (current.blink      != lastDrawn.blink)
                || (current.confirmed  != lastDrawn.confirmed);

    if (!changed) return;

    redraw(ctx);
    lastDrawn = current;
}