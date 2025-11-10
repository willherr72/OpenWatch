/**
 * @file lvgl_touch.h
 * @brief LVGL touch input driver for CST816S
 */

#pragma once

#include <Arduino.h>
#include <lvgl.h>

/**
 * @brief Initialize the LVGL touch input driver
 */
void lvgl_touch_init();

/**
 * @brief Get the LVGL input device
 * @return Pointer to lv_indev_t
 */
lv_indev_t* lvgl_get_touch_indev();

