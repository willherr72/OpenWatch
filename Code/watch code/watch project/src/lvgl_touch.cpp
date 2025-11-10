/**
 * @file lvgl_touch.cpp
 * @brief LVGL touch input driver implementation for CST816S
 */

#include "lvgl_touch.h"
#include <Wire.h>
#include "../lib/cst816s_driver/src/CST816S.h"

/* Static variables */
static lv_indev_t *touch_indev = nullptr;
static lv_indev_drv_t indev_drv;
static CST816SDriver touchDriver;
static bool touchInitialized = false;

/**
 * @brief Touch read callback for LVGL
 */
static void lvgl_touch_read_cb(lv_indev_drv_t *indev_drv, lv_indev_data_t *data) {
    if (!touchInitialized || !touchDriver.isReady()) {
        data->state = LV_INDEV_STATE_RELEASED;
        return;
    }
    
    uint16_t rawX, rawY;
    bool touching;
    
    if (touchDriver.readRaw(rawX, rawY, touching) && touching) {
        data->state = LV_INDEV_STATE_PRESSED;
        data->point.x = rawX;
        data->point.y = rawY;
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
    
    /* Wire (I2C) should already be initialized in main.cpp */
    touchInitialized = touchDriver.begin(
        Wire,
        TOUCH_SDA_PIN,
        TOUCH_SCL_PIN,
        TOUCH_RST_PIN,
        TOUCH_INT_PIN,
        CST816_MODE_POINT
    );
    
    if (touchInitialized) {
        Serial.println("[Touch] CST816S initialized successfully");
    } else {
        Serial.println("[Touch] CST816S initialization failed (will continue without touch)");
    }
    
    /* Create LVGL input device even if touch controller failed */
    /* The driver will handle missing hardware gracefully */
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = lvgl_touch_read_cb;
    touch_indev = lv_indev_drv_register(&indev_drv);
    
    Serial.println("[Touch] LVGL touch input initialized");
}

/**
 * @brief Get LVGL touch input device
 */
lv_indev_t* lvgl_get_touch_indev() {
    return touch_indev;
}

