/**
 * @file ble_handler.h
 * @brief BLE connection handler for smartwatch
 * 
 * Provides BLE connectivity as an alternative to WiFi for:
 * - Time synchronization from phone
 * - Weather data from phone
 * - Future: Notifications
 */

#pragma once

#include <Arduino.h>

/**
 * @brief Connection mode enum
 */
enum class ConnectionMode {
    WIFI,  // Default mode - WiFi for internet connectivity
    BLE    // BLE mode - connect to phone for data
};

/**
 * @brief Weather data structure for BLE transfer
 */
struct BLEWeatherData {
    float temperature;      // Temperature in Fahrenheit
    char condition[64];     // Weather condition string
    char location[64];      // Location name
    char icon;              // Weather icon character
    bool valid;             // Data validity flag
};

/**
 * @brief Initialize BLE handler
 */
void ble_handler_init();

/**
 * @brief Start BLE advertising and services
 */
void ble_handler_start();

/**
 * @brief Stop BLE and free resources
 */
void ble_handler_stop();

/**
 * @brief Update BLE (call in main loop)
 */
void ble_handler_update();

/**
 * @brief Check if BLE is connected to phone
 * @return true if connected
 */
bool ble_handler_is_connected();

/**
 * @brief Get current connection mode
 * @return Current mode (WIFI or BLE)
 */
ConnectionMode ble_handler_get_mode();

/**
 * @brief Set connection mode and switch accordingly
 * @param mode New connection mode
 */
void ble_handler_set_mode(ConnectionMode mode);

/**
 * @brief Get time sync status from BLE
 * @return true if time has been synced via BLE
 */
bool ble_handler_time_synced();

/**
 * @brief Get weather data received via BLE
 * @return Weather data structure
 */
BLEWeatherData ble_handler_get_weather();

/**
 * @brief Check if new weather data is available
 * @return true if weather data was updated
 */
bool ble_handler_weather_updated();

