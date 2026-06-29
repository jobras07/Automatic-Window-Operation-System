#include "motor.h"
#include <Arduino.h>

void motor_init() {
    ledcAttach(PIN_MOTOR_RPWM, MOTOR_PWM_FREQ, MOTOR_PWM_RESOLUTION);
    ledcAttach(PIN_MOTOR_LPWM, MOTOR_PWM_FREQ, MOTOR_PWM_RESOLUTION);
    ledcWrite(PIN_MOTOR_RPWM, 0);
    ledcWrite(PIN_MOTOR_LPWM, 0);
}

void motor_open(int speed) {
    //  Serial.printf("[MOTOR] ledcWrite RPWM=%d speed=%d\n", PIN_MOTOR_RPWM, speed); // DEBUG LINE - REMOVE WHEN DONE
    ledcWrite(PIN_MOTOR_RPWM, 0);
    ledcWrite(PIN_MOTOR_LPWM, speed);
}

void motor_close(int speed) {
    // Serial.printf("[MOTOR] Close speed: %d, LPWM pin: %d\n", speed, PIN_MOTOR_LPWM);
    ledcWrite(PIN_MOTOR_LPWM, 0);
    ledcWrite(PIN_MOTOR_RPWM, speed);
}

void motor_stop() {
    ledcWrite(PIN_MOTOR_RPWM, 0);
    ledcWrite(PIN_MOTOR_LPWM, 0);
}