/**
 * @file lvgl_touch.cpp
 * @brief LVGL touch input driver implementation for CST816S
 */

#include "lvgl_touch.h"
#include "touch_input.h"

/* Static variables */
static lv_indev_t *touch_indev = nullptr;

/**
 * @brief Touch read callback for LVGL
 */
static void lvgl_touch_read_cb(lv_indev_t *indev, lv_indev_data_t *data) {
    TouchPoint touch;
    bool touched = touchRead(touch);
    
    if (touched && touch.touching) {
        data->state = LV_INDEV_STATE_PRESSED;
        data->point.x = touch.x;
        data->point.y = touch.y;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

/**
 * @brief Initialize LVGL touch input
 */
void lvgl_touch_init() {
    /* Initialize the CST816S touch controller (non-blocking if it fails) */
    Serial.println("[Touch] Initializing CST816S (will continue even if it fails)...");
    touchInit();
    
    /* Create LVGL input device even if touch controller failed */
    /* The driver will handle missing hardware gracefully */
    touch_indev = lv_indev_create();
    lv_indev_set_type(touch_indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(touch_indev, lvgl_touch_read_cb);
    
    Serial.println("[Touch] LVGL touch input initialized (may work if hardware is present)");
}

/**
 * @brief Get LVGL touch input device
 */
lv_indev_t* lvgl_get_touch_indev() {
    return touch_indev;
}

