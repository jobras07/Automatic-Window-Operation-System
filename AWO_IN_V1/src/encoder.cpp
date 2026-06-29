#include "encoder.h"
#include "driver/gpio.h"
#include <ESP32Encoder.h>
#include <Arduino.h>

// ================================================================
// ENCODER OBJECT — hidden from other files
// ================================================================
static ESP32Encoder encoder;

// ================================================================
// TIMING
// ================================================================
#define DOUBLE_PRESS_MS 350

// ================================================================
// INTERNAL STATE
// ================================================================
static bool          lastSwState   = HIGH;
static unsigned long lastPressMs   = 0;
static bool          buttonHandled = false;
static int           pressCount    = 0;
static long          lastEncoderRaw = 0;
static long          ClickRemainder = 0;

// ================================================================
// HELPER — enter a new UI state and reset encoder delta
// ================================================================
static void enterState(SystemContext& ctx, UIState newState) {
    ctx.uiState    = newState;
    lastEncoderRaw = -(encoder.getCount());
}

// ================================================================
// PUBLIC FUNCTIONS
// ================================================================
void encoder_init() {
    Serial.println("[ENCODER] Init called");
    
    // force all three pins back to GPIO mode
    gpio_reset_pin((gpio_num_t)PIN_ENC_CLK);
    gpio_reset_pin((gpio_num_t)PIN_ENC_DT);
    gpio_reset_pin((gpio_num_t)PIN_ENC_SW);

    pinMode(PIN_ENC_SW, INPUT);

    ESP32Encoder::useInternalWeakPullResistors = puType::none;
    encoder.attachFullQuad(PIN_ENC_CLK, PIN_ENC_DT);
    encoder.setCount(0);

    Serial.println("[ENCODER] Init done");
}

void encoder_update(SystemContext& ctx) {

        // block all physical controls during teacher override
    if (ctx.teacherOverride) return;

    // Serial.printf("uiState: %d, encRaw: %ld\n", ctx.uiState, -(encoder.getCount()));  // TEMP DEBUGGING - REMOVE WHEN DONE
    // ── ENCODER DELTA ────────────────────────────────────────────

    long currentRaw = -(encoder.getCount());
    long diff      = currentRaw - lastEncoderRaw;
    lastEncoderRaw  = currentRaw;

    ClickRemainder += diff;
    long delta = ClickRemainder / 4; // 4 encoder clicks per detent
    ClickRemainder %= 4; // keep the remainder for the next update

    if (delta != 0) {
        switch (ctx.uiState) {

            case UI_MANUAL:
                // Serial.printf("delta: %ld, currentPos: %d, target: %d\n", // FOR DEBUGGING -REMOVE WHEN DONE
                //     delta, ctx.currentPosition, ctx.targetPosition);       // FOR DEBUGGING -REMOVE WHEN DONE
                ctx.manualTargetPreview = constrain(
                    ctx.manualTargetPreview + (int)(delta * 5), WINDOW_POS_MIN, WINDOW_POS_MAX
                );
                ctx.windowConfirmed = false;
                break;

            case UI_SELECT_MODE:
                ctx.selectedMode = (SystemMode)(
                    ((ctx.selectedMode + (int)delta) % 4 + 4) % 4
                );
                break;

            case UI_SET_TEMP_TARGET:
                ctx.setTargetTemp = constrain(
                    ctx.setTargetTemp + (float)delta * 1.0f, 16.0f, 32.0f
                );
                break;

            case UI_SET_TEMP_RANGE:
                ctx.setTempRange = constrain(
                    ctx.setTempRange + (float)delta * 0.5f, 0.5f, 5.0f
                );
                break;

            default:
                break;
        }
    }

    // ── BUTTON ───────────────────────────────────────────────────
    bool swState = digitalRead(PIN_ENC_SW);

    if (swState == HIGH && lastSwState == LOW) { // reversed because we removed INPUT_PULLUP and added external pull-up. Initially: swState == LOW when pressed, HIGH when released
        unsigned long now = millis();
        pressCount    = (now - lastPressMs < DOUBLE_PRESS_MS) ? 2 : 1;
        lastPressMs   = now;
        buttonHandled = false;
    }

    if (swState == HIGH && lastSwState == LOW && !buttonHandled) {

    if (pressCount == 2) {
            ctx.selectedMode = ctx.mode;
            enterState(ctx, UI_SELECT_MODE);
    } else if (pressCount == 1) {

            switch (ctx.uiState) {

                case UI_MANUAL:
                    ctx.windowConfirmed = true;
                    ctx.targetPosition  = ctx.manualTargetPreview;
                    break;

                case UI_SELECT_MODE:
                    if (ctx.selectedMode == MODE_IP) {
                        enterState(ctx, UI_IP_ADDRESS);
                    break;
                    } 
                    ctx.mode = ctx.selectedMode;
                    if (ctx.mode == MODE_MANUAL) {
                        ctx.autoConfirmed = false; // just in case, though it shouldn't matter since AUTO logic is skipped in MANUAL
                        ctx.windowConfirmed = false;
                        ctx.targetPosition = ctx.currentPosition; // prevent sudden movement when switching from auto
                        ctx.manualTargetPreview = ctx.currentPosition; // sync preview to current position
                        enterState(ctx, UI_MANUAL);
                    } else if (ctx.mode == MODE_AUTOMATIC) {
                        ctx.autoConfirmed = false; // force re-confirmation to apply new settings
                        ctx.targetPosition = ctx.currentPosition; // prevent sudden movement when switching from manual
                        enterState(ctx, UI_SET_TEMP_TARGET);
                    } else if (ctx.mode == MODE_LOCK) {
                        ctx.targetPosition = WINDOW_POS_MIN;
                        ctx.autoConfirmed = false; // just in case, though it shouldn't matter since AUTO logic is skipped in LOCK  
                        enterState(ctx, UI_LOCK);
                    } 
                    break;

                case UI_SET_TEMP_TARGET:
                    enterState(ctx, UI_SET_TEMP_RANGE);
                    break;

                case UI_SET_TEMP_RANGE:
                    ctx.autoConfirmed = true; 
                    enterState(ctx, UI_AUTOMATIC);
                    break;

                case UI_EMERGENCY_STOP:
                    // hold detection handled separately below
                    break;

                default:
                    break;
            }
        }

        buttonHandled = true;
    }

    lastSwState = swState;

    // ── EMERGENCY STOP RESET (hold button) ───────────────────────
if (ctx.state == STATE_EMERGENCY_STOP) {
    // only allow reset if e-stop pin is physically unlatched first
    bool estopPinReleased = (digitalRead(PIN_ESTOP) == HIGH);
    
    if (estopPinReleased && swState == HIGH) {
        if (ctx.estopResetHoldStartMs == 0) {
            ctx.estopResetHoldStartMs = millis();
        } else if (millis() - ctx.estopResetHoldStartMs > ESTOP_RESET_HOLD_MS) {
            ctx.estopPressed          = false;
            ctx.estopResetHoldStartMs = 0;
            ctx.mode                  = MODE_MANUAL;
            ctx.state                 = STATE_IDLE;
            ctx.uiState               = UI_MANUAL;
            enterState(ctx, UI_MANUAL);
        }
    } else {
        // either still latched or button not held — reset hold timer
        ctx.estopResetHoldStartMs = 0;
    }
}

// ── OVERCURRENT RESET (hold button 3 seconds) ─────────────────
if (ctx.uiState == UI_OVERCURRENT) {
    if (swState == HIGH) {
        if (ctx.overcurrentResetHoldStartMs == 0) {
            ctx.overcurrentResetHoldStartMs = millis();
        } else if (millis() - ctx.overcurrentResetHoldStartMs > 3000) {
            ctx.overcurrentResetHoldStartMs = 0;
            if (ctx.mode == MODE_MANUAL)         ctx.uiState = UI_MANUAL;
            else if (ctx.mode == MODE_AUTOMATIC) ctx.uiState = UI_AUTOMATIC;
            else if (ctx.mode == MODE_LOCK)      ctx.uiState = UI_LOCK;
        }
    } else {
        ctx.overcurrentResetHoldStartMs = 0;
    }
}

}