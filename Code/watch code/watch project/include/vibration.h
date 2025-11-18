/**
 * @file vibration.h
 * @brief Vibration motor control
 */

#pragma once

#include <Arduino.h>

#define VIBRATION_PIN 6  // GPIO6

/**
 * @brief Initialize vibration motor
 */
void vibration_init();

/**
 * @brief Turn vibration on
 */
void vibration_on();

/**
 * @brief Turn vibration off
 */
void vibration_off();

/**
 * @brief Pulse vibration motor (non-blocking)
 * @param duration_ms Duration in milliseconds
 */
void vibration_pulse(unsigned long duration_ms);

/**
 * @brief Pattern vibration (short-long-short)
 */
void vibration_pattern();

/**
 * @brief Update vibration (call in main loop for non-blocking pulses)
 */
void vibration_update();

