/**
 * @file vibration.cpp
 * @brief Vibration motor control implementation
 */

#include "vibration.h"

/* Vibration state */
static bool vibration_active = false;
static unsigned long vibration_start_time = 0;
static unsigned long vibration_duration = 0;

/**
 * @brief Initialize vibration motor
 */
void vibration_init() {
    pinMode(VIBRATION_PIN, OUTPUT);
    digitalWrite(VIBRATION_PIN, LOW);
    Serial.println("[Vibration] Motor initialized on GPIO6");
}

/**
 * @brief Turn vibration on
 */
void vibration_on() {
    digitalWrite(VIBRATION_PIN, HIGH);
    vibration_active = true;
    Serial.println("[Vibration] Motor ON (GPIO6 HIGH)");
}

/**
 * @brief Turn vibration off
 */
void vibration_off() {
    digitalWrite(VIBRATION_PIN, LOW);
    vibration_active = false;
    Serial.println("[Vibration] Motor OFF (GPIO6 LOW)");
}

/**
 * @brief Pulse vibration motor (non-blocking)
 */
void vibration_pulse(unsigned long duration_ms) {
    Serial.printf("[Vibration] Starting pulse: %lu ms\n", duration_ms);
    vibration_on();
    vibration_start_time = millis();
    vibration_duration = duration_ms;
}

/**
 * @brief Pattern vibration (short-long-short for timer alert)
 */
void vibration_pattern() {
    // This will be handled by repeated pulses from the timer code
    vibration_pulse(200);  // Short pulse
}

/**
 * @brief Update vibration (call in main loop)
 */
void vibration_update() {
    if (vibration_active && vibration_duration > 0) {
        unsigned long elapsed = millis() - vibration_start_time;
        if (elapsed >= vibration_duration) {
            Serial.printf("[Vibration] Pulse complete after %lu ms\n", elapsed);
            vibration_off();
            vibration_duration = 0;
        }
    }
}

