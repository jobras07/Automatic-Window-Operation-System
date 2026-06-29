#include "sensors.h"
#include <Arduino.h>

static DHT_Async dht(PIN_DHT22, DHT_SENSOR_TYPE);

void sensors_init() {
    pinMode(PIN_ESTOP, INPUT);   // NC e-stop, pulled up externally, active LOW
    pinMode(PIN_LA_POT, INPUT);     // potentiometer is analog, just note the pin
}

void sensors_update(SystemContext& ctx) {

    // --- DHT read (non blocking) ---
    float temperature, humidity;
    if (dht.measure(&temperature, &humidity)) {
        ctx.indoor.temperature = temperature;
        ctx.indoor.humidity    = humidity;
    }

// only read pot when motor is moving
if (ctx.state == STATE_ADJUSTING || ctx.state == STATE_FORCE_CLOSING) {
        int newPos = sensors_getPosition();
    if (abs(newPos - ctx.currentPosition) > 1) {
        ctx.currentPosition = newPos;
    }
    // ctx.currentPosition = sensors_getPosition(); //original line
    // int raw = analogRead(PIN_LA_POT); // DEBUG LINE START
    // int pos = sensors_getPosition();
    // Serial.printf("POT raw: %d, pos: %d\n", raw, pos);
    // ctx.currentPosition = pos; // DEBUG LINE END
}

    // --- E-stop read ---
    bool pinState = digitalRead(PIN_ESTOP);
    ctx.estopPressed = ESTOP_ACTIVE_LOW ? !pinState : pinState;

    // read IS pins
    float rIS = (analogRead(PIN_MOTOR_R_IS) / 4095.0f) * 3.3f;
    float lIS = (analogRead(PIN_MOTOR_L_IS) / 4095.0f) * 3.3f;

    if (rIS > OVERCURRENT_THRESHOLD || lIS > OVERCURRENT_THRESHOLD) {
        ctx.overcurrentDetected = true;
    }

}

int sensors_getPosition() {
    // average 10 readings
    int sum = 0;
    for (int i = 0; i < 10; i++) {
        sum += analogRead(PIN_LA_POT);
    }
    int raw = sum / 10;

    // map raw ADC to physical range first
    int physicalPos;
    if (POT_REVERSE) {
        physicalPos = map(raw, POT_ADC_MAX, POT_ADC_MIN, 0, 100);
    } else {
        physicalPos = map(raw, POT_ADC_MIN, POT_ADC_MAX, 0, 100);
    }
    physicalPos = constrain(physicalPos, 0, 100);

    // remap physical limits to 0-100%
    int displayPos = map(physicalPos, WINDOW_PHYS_MIN, WINDOW_PHYS_MAX, 0, 100);
    return constrain(displayPos, 0, 100);
}