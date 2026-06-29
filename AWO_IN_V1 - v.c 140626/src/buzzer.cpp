#include "buzzer.h"
#include <Arduino.h>

// ================================================================
// INTERNAL STATE
// ================================================================
static unsigned long buzzerStartMs = 0;
static bool          buzzerActive  = false;
#define BUZZER_DURATION_MS  2000

// previous state tracking — so we only trigger on state CHANGE
static bool          lastEstop        = false;
static bool          lastNaturalEvent = false;
static SystemMode    lastMode         = MODE_MANUAL;

static void buzzerOn() {
    digitalWrite(PIN_BUZZER, HIGH);
    buzzerStartMs = millis();
    buzzerActive  = true;
}

static void buzzerOff() {
    digitalWrite(PIN_BUZZER, LOW);
    buzzerActive = false;
}

// ================================================================
// INIT
// ================================================================
void buzzer_init() {
    pinMode(PIN_BUZZER, OUTPUT);
    digitalWrite(PIN_BUZZER, LOW);
}

// ================================================================
// UPDATE
// ================================================================
void buzzer_update(SystemContext& ctx) {

    // ── TRIGGER ON STATE CHANGES ─────────────────────────────────

    // e-stop triggered
    if (ctx.estopPressed && !lastEstop) {
        buzzerOn();
    }

    // natural event detected (rain or wind)
    if (ctx.naturalEventActive && !lastNaturalEvent) {
        buzzerOn();
    }

    // mode changed to LOCK
    if (ctx.mode == MODE_LOCK && lastMode != MODE_LOCK) {
        buzzerOn();
    }

    // ── AUTO OFF AFTER DURATION ──────────────────────────────────
    if (buzzerActive && millis() - buzzerStartMs >= BUZZER_DURATION_MS) {
        buzzerOff();
    }

    // overcurrent trigger
    if (ctx.buzzerTrigger) {
        ctx.buzzerTrigger = false;
        buzzerOn();
    }

    // ── SAVE PREVIOUS STATE ──────────────────────────────────────
    lastEstop        = ctx.estopPressed;
    lastNaturalEvent = ctx.naturalEventActive;
    lastMode         = ctx.mode;
}