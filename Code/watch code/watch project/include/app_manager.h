/**
 * @file app_manager.h
 * @brief App manager for multi-screen smartwatch navigation
 */

#pragma once

#include <Arduino.h>
#include <lvgl.h>

/**
 * @brief App/Screen types
 */
enum class AppType {
    WATCH_FACE,
    FITNESS,
    MUSIC,
    WEATHER,
    SETTINGS,
    HEART_RATE,
    MORE_SETTINGS,
    TIME_SETTING,
    POWER_MENU
};

/**
 * @brief Swipe direction
 */
enum class SwipeDirection {
    NONE,
    UP,
    DOWN,
    LEFT,
    RIGHT
};

/**
 * @brief Initialize the app manager
 */
void app_manager_init();

/**
 * @brief Handle swipe gestures
 * @param dir Swipe direction
 */
void app_manager_handle_swipe(SwipeDirection dir);

/**
 * @brief Navigate to specific app
 * @param app App to navigate to
 */
void app_manager_navigate_to(AppType app);

/**
 * @brief Get current app
 * @return Current app type
 */
AppType app_manager_get_current();

/**
 * @brief Return to watch face
 */
void app_manager_return_home();

/**
 * @brief Update current app (for animations, data updates, etc.)
 */
void app_manager_update();

