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
 * @brief Set WiFi connection status
 * @param connected true if WiFi is connected
 */
void watch_face_set_wifi_status(bool connected);

/**
 * @brief Set time sync status
 * @param synced true if time is synced via NTP
 */
void watch_face_set_time_synced(bool synced);

/**
 * @brief Get the watch face screen object
 * @return Pointer to lv_obj_t screen
 */
lv_obj_t* watch_face_get_screen();

