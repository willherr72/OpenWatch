/**
 * @file lvgl_display.h
 * @brief LVGL display driver for GC9A01A using Adafruit library
 */

#pragma once

#include <Arduino.h>
#include <lvgl.h>
#include <Adafruit_GC9A01A.h>
#include <Adafruit_GFX.h>

/* Display dimensions */
#define SCREEN_WIDTH  240
#define SCREEN_HEIGHT 240

/* Buffer size for LVGL - reduced to fit in internal RAM */
#define LVGL_BUFFER_SIZE (SCREEN_WIDTH * 20)  // 20 lines buffer = 9.6KB

/**
 * @brief Initialize the LVGL display driver
 * @param tft Pointer to Adafruit_GC9A01A instance
 */
void lvgl_display_init(Adafruit_GC9A01A *tft);

/**
 * @brief Get the LVGL display object
 * @return Pointer to lv_disp_t
 */
lv_disp_t* lvgl_get_display();

