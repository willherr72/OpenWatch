/**
 * @file watch_face.h
 * @brief Modern watch face UI using LVGL
 */

#pragma once

#include <Arduino.h>
#include <lvgl.h>

/**
 * @brief Initialize and create the watch face UI
 */
void watch_face_init();

/**
 * @brief Update the watch face with current time
 * Should be called periodically (every second)
 */
void watch_face_update();

/**
 * @brief Set connectivity status (WiFi or BLE)
 * @param connected true if connected
 * @param is_ble true for BLE mode, false for WiFi mode
 */
void watch_face_set_connectivity_status(bool connected, bool is_ble);

/**
 * @brief Set time sync status
 * @param synced true if time is synced via NTP
 */
void watch_face_set_time_synced(bool synced);

/**
 * @brief Set battery level (0-100)
 * @param level Battery percentage
 * @param charging true if battery is charging
 */
void watch_face_set_battery(int level, bool charging);

/**
 * @brief Set step count
 * @param steps Number of steps today
 */
void watch_face_set_steps(int steps);

/**
 * @brief Set heart rate
 * @param bpm Beats per minute (0 = no data)
 */
void watch_face_set_heart_rate(int bpm);

/**
 * @brief Set weather display
 * @param temp Temperature in Fahrenheit
 * @param icon_text Icon character to display
 */
void watch_face_set_weather(float temp, const char* icon_text);

/**
 * @brief Set steps display
 * @param steps Current step count
 */
void watch_face_set_steps(int steps);

/**
 * @brief Set timer display
 * @param minutes Remaining minutes
 * @param seconds Remaining seconds
 * @param running True if timer is running
 */
void watch_face_set_timer(int minutes, int seconds, bool running);

/**
 * @brief Get the watch face screen object
 * @return Pointer to lv_obj_t screen
 */
lv_obj_t* watch_face_get_screen();

/**
 * @brief Manually set the time
 * @param hour Hour (0-23)
 * @param minute Minute (0-59)
 */
void watch_face_set_time_manually(int hour, int minute);

