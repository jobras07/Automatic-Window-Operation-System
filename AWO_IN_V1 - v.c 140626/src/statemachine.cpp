#include "statemachine.h"
#include "motor.h"
#include "sensors.h"
#include <Arduino.h>

// ================================================================
// INIT
// ================================================================
void stateMachine_init(SystemContext& ctx) {
    ctx.state                   = STATE_IDLE;
    ctx.mode                    = MODE_MANUAL;  // always boot into MANUAL for safety
    ctx.modeBeforeForceClose    = MODE_MANUAL;
    ctx.currentPosition         = 0;
    ctx.targetPosition          = 0;
    ctx.estopPressed            = false;
    ctx.limitOpenHit            = false;
    ctx.limitClosedHit          = false;
    ctx.naturalEventActive      = false;
    ctx.setTargetTemp           = TEMP_TARGET;
    ctx.setTempRange            = TEMP_RANGE;
    ctx.lastSensorPollMs        = 0;
    ctx.lastLogMs               = 0;
    ctx.lastEspNowRxMs          = 0;
    ctx.estopResetHoldStartMs   = 0;

    // outdoor data starts as stale until first ESP-NOW packet
    ctx.outdoor.isStale         = true;
    ctx.outdoor.rainDetected    = false;
    ctx.outdoor.windSpeed       = 0.0f;
    ctx.outdoor.temperature     = 0.0f;

    // UI STATES
    ctx.uiState                 = UI_MANUAL;
    ctx.selectedMode            = MODE_MANUAL;
    ctx.windowConfirmed         = false;
    ctx.autoConfirmed           = false;

    // OVERCURRENT PROTECTION
    ctx.overcurrentDetected = false;
    ctx.overcurrentRetried  = false;
    ctx.overcurrentEnabled = false;
    ctx.overcurrentTimeMs   = 0;

    // Website
    ctx.teacherOverride   = false;
    ctx.webUserLoggedIn   = false;
    ctx.windThreshold = WIND_SPEED_MAX;
}

// ================================================================
// HELPERS
// ================================================================
bool naturalEventActive(const SystemContext& ctx) {
    if (ctx.outdoor.isStale) return false;   // no data, don't assume danger
    return ctx.outdoor.rainDetected || (ctx.outdoor.windSpeed > ctx.windThreshold);
}

int calculateTargetPosition(const SystemContext& ctx) {
    // if outdoor data is stale, don't open window
    if (ctx.outdoor.isStale) return 0;

    float indoorTemp    = ctx.indoor.temperature;
    float outdoorTemp   = ctx.outdoor.temperature;
    float upper         = ctx.setTargetTemp + ctx.setTempRange;
    float lower         = ctx.setTargetTemp - ctx.setTempRange;

    // Serial.printf("[AUTO] indoor: %.1f, outdoor: %.1f, upper: %.1f, lower: %.1f\n", // DEBUG LINE - REMOVE WHEN DONE
    //     indoorTemp, outdoorTemp, upper, lower);                                     // DEBUG LINE - REMOVE WHEN DONE

    // // outdoor must be meaningfully cooler than indoor to open
    bool outdoorIsCooler = (outdoorTemp < indoorTemp - TEMP_OUTDOOR_DIFF);
    //     Serial.printf("[AUTO] outdoorIsCooler: %d\n", outdoorIsCooler); // DEBUG LINE - REMOVE WHEN DONE

if (indoorTemp > upper && outdoorIsCooler) {
    float excess = indoorTemp - upper;

    // don't react if excess is less than 2.5°C
    if (excess < 2.5f) {
        return 0;   // close window
    }

    // 2.5°C = 50%, 5°C+ = 100%
    float ratio   = constrain((excess - 2.5f) / 2.5f, 0.0f, 1.0f);
    int newTarget = (int)(ratio * WINDOW_POS_MAX);
    newTarget     = constrain(newTarget + 50, 50, WINDOW_POS_MAX);

    if (abs(newTarget - ctx.currentPosition) < 3) {
        return ctx.currentPosition;
    }
    return newTarget;
}
    else if (indoorTemp < lower) {
        // indoor is cool enough, close window
        return 0;
    }
    else {
        // within range, hold current position
        return ctx.currentPosition;
    }
}

// ================================================================
// STATE MACHINE UPDATE — call every loop()
// ================================================================
void stateMachine_update(SystemContext& ctx) {
        static SystemState lastState = STATE_IDLE;                              // DEBUG LINE - REMOVE WHEN DONE
    if (ctx.state != lastState) {                                               // DEBUG LINE - REMOVE WHEN DONE
        Serial.printf("STATE CHANGED: %d -> %d\n", lastState, ctx.state);       // DEBUG LINE - REMOVE WHEN DONE
        lastState = ctx.state;                                                  // DEBUG LINE - REMOVE WHEN DONE
    }
    // ── EMERGENCY STOP — highest priority, checked first ────────
    if (ctx.estopPressed) {
        if (ctx.state != STATE_EMERGENCY_STOP) {
            motor_stop();           // cut motor immediately
            ctx.state = STATE_EMERGENCY_STOP;
        
        }
        ctx.uiState = UI_EMERGENCY_STOP; // force E-STOP screen
        return;                     // nothing else runs while e-stop active
    }

    // ── OVERCURRENT — second priority after e-stop ───────────────
    if (ctx.overcurrentDetected && ctx.state == STATE_ADJUSTING) {
        motor_stop();
        ctx.overcurrentDetected = false;

        if (!ctx.overcurrentRetried) {
            // first time — wait 5 seconds then retry
            ctx.overcurrentRetried  = true;
            ctx.overcurrentTimeMs   = millis();
            ctx.state               = STATE_IDLE;
            // trigger buzzer
            ctx.buzzerTrigger       = true;
        } else {
            // already retried once — give up, go to idle
            ctx.overcurrentRetried  = false;
            ctx.targetPosition      = ctx.currentPosition;  // stay where we are
            ctx.windowConfirmed     = true;
            ctx.state               = STATE_IDLE;
            ctx.buzzerTrigger       = true;
        }
    }

    // handle retry timer
    if (ctx.overcurrentRetried && ctx.state == STATE_IDLE) {
            Serial.printf("[OC] retried: %d, time since OC: %lu\n", // DEBUG LINE - REMOVE WHEN DONE 
        ctx.overcurrentRetried, millis() - ctx.overcurrentTimeMs); // DEBUG LINE - REMOVE WHEN DONE
        if (millis() - ctx.overcurrentTimeMs > OVERCURRENT_RETRY_MS) {
            // retry — re-trigger adjusting
            ctx.overcurrentEnabled = false;          // ← disable for grace period
            ctx.overcurrentTimeMs  = millis();        // ← reset grace period timer
            ctx.state = STATE_ADJUSTING;
            if (ctx.targetPosition > ctx.currentPosition) motor_open(MOTOR_MAX_PWM);
            else                                           motor_close(MOTOR_MAX_PWM);
        }
    }
    

    // ── NATURAL EVENT — third priority ─────────────────────────
    ctx.naturalEventActive = naturalEventActive(ctx);

    if (ctx.naturalEventActive) {
        if (ctx.state != STATE_FORCE_CLOSING && 
            ctx.state != STATE_FULLY_CLOSED  &&
            ctx.state != STATE_EMERGENCY_STOP) {
            ctx.modeBeforeForceClose = ctx.mode;
            ctx.targetPosition       = WINDOW_POS_MIN;
            ctx.state                = STATE_FORCE_CLOSING;
            motor_close(MOTOR_MAX_PWM);
        }
    }

    // ── STATE LOGIC ──────────────────────────────────────────────
    switch (ctx.state) {

case STATE_IDLE:
    // Serial.printf("state: %d, mode: %d, target: %d, current: %d, confirmed: %d\n", // DEBUG LINE - REMOVE WHEN DONE
    //     ctx.state, ctx.mode, ctx.targetPosition, ctx.currentPosition, ctx.windowConfirmed); // DEBUG LINE - REMOVE WHEN DONE
    if (ctx.mode == MODE_AUTOMATIC && ctx.autoConfirmed) {
            ctx.targetPosition = calculateTargetPosition(ctx);
            if (abs(ctx.targetPosition - ctx.currentPosition) > 1) {
                ctx.state = STATE_ADJUSTING;
                ctx.overcurrentEnabled = false;        // start grace period
                ctx.overcurrentTimeMs  = millis();     // start grace period timer
                if (ctx.targetPosition > ctx.currentPosition) motor_open(MOTOR_MAX_PWM);
                else                                           motor_close(MOTOR_MAX_PWM);
            }
    }
    else if (ctx.mode == MODE_MANUAL) {
        if (ctx.windowConfirmed && abs(ctx.targetPosition - ctx.currentPosition) > 1) {
            // Serial.println("[STATE] Triggering ADJUSTING"); // DEBUG LINE - REMOVE WHEN DONE
            ctx.state = STATE_ADJUSTING;
            ctx.overcurrentEnabled = false;        // start grace period
            ctx.overcurrentTimeMs  = millis();
            if (ctx.targetPosition > ctx.currentPosition) motor_open(MOTOR_MAX_PWM);
            else                                           motor_close(MOTOR_MAX_PWM);
        } else {
            // Serial.printf("[STATE] Not triggering: confirmed=%d, diff=%d\n",
            //     ctx.windowConfirmed, ctx.targetPosition - ctx.currentPosition);
        }
    }
    else if (ctx.mode == MODE_LOCK) {
        if (ctx.currentPosition != WINDOW_POS_MIN) {
            ctx.targetPosition = WINDOW_POS_MIN;
            ctx.state          = STATE_ADJUSTING;
            motor_close(MOTOR_MAX_PWM);
        }
    }
    break;

        case STATE_ADJUSTING:
            // grace period — disable overcurrent for 500ms after motor starts
            if (!ctx.overcurrentEnabled) {
                if (millis() - ctx.overcurrentTimeMs > 500) {
                    ctx.overcurrentEnabled = true;
                }
            }

            if (abs(ctx.currentPosition - ctx.targetPosition) <= 1) {
                motor_stop();
                ctx.overcurrentRetried = false;
                ctx.overcurrentEnabled = false;
                ctx.state              = STATE_IDLE;
            }
            break;

        case STATE_FULLY_OPEN:
            if (ctx.mode == MODE_AUTOMATIC) {
                ctx.targetPosition = calculateTargetPosition(ctx);
                if (ctx.targetPosition < WINDOW_POS_MAX) {
                    ctx.state = STATE_ADJUSTING;
                    ctx.overcurrentEnabled = false;        // start grace period
                    ctx.overcurrentTimeMs  = millis();
                    motor_close(MOTOR_MAX_PWM);
                }
            }
            break;

        case STATE_FULLY_CLOSED:
            // natural event clears — return to previous mode
            if (!ctx.naturalEventActive && ctx.state == STATE_FULLY_CLOSED) {
                ctx.mode  = ctx.modeBeforeForceClose;
                ctx.state = STATE_IDLE;
            }
            if (ctx.mode == MODE_AUTOMATIC) {
                ctx.targetPosition = calculateTargetPosition(ctx);
                if (ctx.targetPosition > WINDOW_POS_MIN) {
                    ctx.state = STATE_ADJUSTING;
                    ctx.overcurrentEnabled = false;        // start grace period
                    ctx.overcurrentTimeMs  = millis();
                    motor_open(MOTOR_MAX_PWM);
                }
            }
            break;

        case STATE_FORCE_CLOSING:
            if (ctx.limitClosedHit) {
                motor_stop();
                ctx.currentPosition = WINDOW_POS_MIN;
                ctx.state           = STATE_FULLY_CLOSED;
            }
            // if natural event clears before fully closed, go to idle
            if (!ctx.naturalEventActive) {
                motor_stop();
                ctx.mode  = ctx.modeBeforeForceClose;
                ctx.state = STATE_IDLE;
            }
            break;

        case STATE_EMERGENCY_STOP:
            // handled at top of function
            // exit only via encoder module setting estopPressed = false
            break;
    }
    // Serial.printf("[NATURE] After Running All: rain: %d, wind: %.1f, naturalEvent: %d, state: %d\n", // DEBUG LINE - REMOVE WHEN DONE
    // ctx.outdoor.rainDetected,                                                                        // DEBUG LINE - REMOVE WHEN DONE
    // ctx.outdoor.windSpeed,                                                                           // DEBUG LINE - REMOVE WHEN DONE                                        
    // ctx.naturalEventActive,                                                                          // DEBUG LINE - REMOVE WHEN DONE
    // ctx.state);                                                                                      // DEBUG LINE - REMOVE WHEN DONE                                           
}