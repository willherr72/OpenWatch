/**
 * @file sensor_handler.h
 * @brief Sensor management for smartwatch (BMP280, BNO055, MAX30102, GPS)
 */

#pragma once

#include <Arduino.h>

/**
 * @brief Sensor data structure for BMP280
 */
struct BMP280Data {
    float temperature;      // Temperature in Celsius
    float pressure;         // Pressure in hPa
    bool valid;             // True if data is valid
};

/**
 * @brief Sensor data structure for BNO055
 */
struct BNO055Data {
    // Raw sensor data
    float accel_x, accel_y, accel_z;     // Accelerometer (m/s^2)
    float gyro_x, gyro_y, gyro_z;        // Gyroscope (rad/s)
    float mag_x, mag_y, mag_z;           // Magnetometer (uT)
    
    // Calculated orientation (NDOF fusion)
    float pitch;            // Inclination (degrees)
    float roll;             // Toolface (degrees)
    float yaw;              // Azimuth (degrees)
    
    bool valid;             // True if data is valid
};

/**
 * @brief Sensor data structure for MAX30102
 */
struct MAX30102Data {
    int heartRate;          // Heart rate in BPM
    int spo2;               // SpO2 percentage
    bool valid;             // True if data is valid
};

/**
 * @brief GPS data structure
 */
struct GPSData {
    double latitude;        // Latitude
    double longitude;       // Longitude
    float altitude;         // Altitude in meters
    float speed;            // Speed in km/h
    int satellites;         // Number of satellites
    bool valid;             // True if data is valid
};

/**
 * @brief Initialize all sensors
 * @return true if at least one sensor initialized successfully
 */
bool sensor_handler_init();

/**
 * @brief Check if sensor streaming is enabled
 * @return true if streaming is enabled
 */
bool sensor_handler_is_streaming();

/**
 * @brief Enable or disable sensor streaming
 * @param enabled true to enable, false to disable
 */
void sensor_handler_set_streaming(bool enabled);

/**
 * @brief Update sensor readings (call periodically at 200ms intervals)
 */
void sensor_handler_update();

/**
 * @brief Get BMP280 data
 * @return BMP280Data structure
 */
BMP280Data sensor_handler_get_bmp280();

/**
 * @brief Get BNO055 data
 * @return BNO055Data structure
 */
BNO055Data sensor_handler_get_bno055();

/**
 * @brief Get MAX30102 data
 * @return MAX30102Data structure
 */
MAX30102Data sensor_handler_get_max30102();

/**
 * @brief Get GPS data
 * @return GPSData structure
 */
GPSData sensor_handler_get_gps();

/**
 * @brief Print all sensor data to serial console
 */
void sensor_handler_print_data();

/**
 * @brief Check if BMP280 is available
 * @return true if sensor is responding
 */
bool sensor_handler_bmp280_available();

/**
 * @brief Check if BNO055 is available
 * @return true if sensor is responding
 */
bool sensor_handler_bno055_available();

/**
 * @brief Check if MAX30102 is available
 * @return true if sensor is responding
 */
bool sensor_handler_max30102_available();

/**
 * @brief Check if GPS is available
 * @return true if GPS is responding
 */
bool sensor_handler_gps_available();

