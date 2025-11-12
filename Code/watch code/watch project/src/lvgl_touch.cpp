/**
 * @file lvgl_touch.cpp
 * @brief LVGL touch input driver implementation for CST816S
 */

#include "lvgl_touch.h"
#include "app_manager.h"
#include <Wire.h>
#include "../lib/cst816s_driver/src/CST816S.h"

/* Static variables */
static lv_indev_t *touch_indev = nullptr;
static lv_indev_drv_t indev_drv;
static CST816SDriver touchDriver;
static bool touchInitialized = false;
static bool verboseLogging = true;  // Detailed logging on first attempt only

/* Gesture detection variables */
static bool gesture_in_progress = false;
static int16_t gesture_start_x = 0;
static int16_t gesture_start_y = 0;
static int16_t gesture_last_x = 0;
static int16_t gesture_last_y = 0;
static uint32_t gesture_start_time = 0;

#define SWIPE_MIN_DISTANCE 50  // Reduced from 60 for easier detection
#define SWIPE_MAX_TIME_MS 700   // Increased from 500 for more relaxed timing

/**
 * @brief Touch read callback for LVGL
 */
static void lvgl_touch_read_cb(lv_indev_drv_t *indev_drv, lv_indev_data_t *data) {
    if (!touchInitialized || !touchDriver.isReady()) {
        data->state = LV_INDEV_STATE_RELEASED;
        
        /* End gesture if one was in progress */
        if (gesture_in_progress) {
            Serial.println("[Touch] Touch not ready, ending gesture");
            gesture_in_progress = false;
        }
        return;
    }
    
    uint16_t rawX, rawY;
    bool touching;
    
    if (touchDriver.readRaw(rawX, rawY, touching) && touching) {
        data->state = LV_INDEV_STATE_PRESSED;
        data->point.x = rawX;
        data->point.y = rawY;
        
        /* Track gesture */
        if (!gesture_in_progress) {
            gesture_in_progress = true;
            gesture_start_x = rawX;
            gesture_start_y = rawY;
            gesture_start_time = millis();
            Serial.printf("[Touch] PRESSED at (%d, %d)\n", rawX, rawY);
        }
        gesture_last_x = rawX;
        gesture_last_y = rawY;
        
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
        
        /* End gesture and detect swipe */
        if (gesture_in_progress) {
            int16_t delta_x = gesture_last_x - gesture_start_x;
            int16_t delta_y = gesture_last_y - gesture_start_y;
            uint32_t duration = millis() - gesture_start_time;
            
            Serial.printf("[Touch] RELEASED - Start: (%d, %d) End: (%d, %d) Delta: (%d, %d) Duration: %dms\n",
                gesture_start_x, gesture_start_y, gesture_last_x, gesture_last_y, 
                delta_x, delta_y, duration);
            
            /* Check if LVGL is handling scrolling (e.g., on a roller) */
            lv_obj_t *pressed_obj = lv_indev_get_obj_act();
            bool lvgl_scrolling = false;
            if (pressed_obj) {
                /* Check if the object or its parent is scrollable */
                lv_obj_t *obj = pressed_obj;
                while (obj != NULL) {
                    if (lv_obj_has_flag(obj, LV_OBJ_FLAG_SCROLLABLE)) {
                        lvgl_scrolling = true;
                        Serial.println("[Touch] LVGL handling scroll - ignoring swipe");
                        break;
                    }
                    obj = lv_obj_get_parent(obj);
                }
            }
            
            /* Check if it's a valid swipe (and not being handled by LVGL) */
            if (!lvgl_scrolling && duration < SWIPE_MAX_TIME_MS) {
                if (abs(delta_x) > abs(delta_y) && abs(delta_x) > SWIPE_MIN_DISTANCE) {
                    /* Horizontal swipe */
                    Serial.printf("[Touch] Horizontal swipe detected! delta_x=%d (threshold=%d)\n", 
                        delta_x, SWIPE_MIN_DISTANCE);
                    if (delta_x > 0) {
                        app_manager_handle_swipe(SwipeDirection::RIGHT);
                    } else {
                        app_manager_handle_swipe(SwipeDirection::LEFT);
                    }
                } else if (abs(delta_y) > abs(delta_x) && abs(delta_y) > SWIPE_MIN_DISTANCE) {
                    /* Vertical swipe */
                    Serial.printf("[Touch] Vertical swipe detected! delta_y=%d (threshold=%d)\n", 
                        delta_y, SWIPE_MIN_DISTANCE);
                    if (delta_y > 0) {
                        app_manager_handle_swipe(SwipeDirection::DOWN);
                    } else {
                        app_manager_handle_swipe(SwipeDirection::UP);
                    }
                } else {
                    Serial.printf("[Touch] Not a swipe - abs(dx)=%d abs(dy)=%d (threshold=%d)\n",
                        abs(delta_x), abs(delta_y), SWIPE_MIN_DISTANCE);
                }
            } else if (lvgl_scrolling) {
                Serial.println("[Touch] Swipe ignored - LVGL scrolling active");
            } else if (duration >= SWIPE_MAX_TIME_MS) {
                Serial.printf("[Touch] Gesture too slow - %dms (max=%dms)\n", 
                    duration, SWIPE_MAX_TIME_MS);
            }
            
            gesture_in_progress = false;
        }
    }
}

/**
 * @brief Initialize LVGL touch input
 */
void lvgl_touch_init() {
    /* Initialize the CST816S touch controller (non-blocking if it fails) */
    Serial.println("====================================");
    Serial.println("[Touch] Initializing CST816S...");
    Serial.printf("[Touch] Touch pins - SDA:%d SCL:%d RST:%d INT:%d\n", 
        TOUCH_SDA_PIN, TOUCH_SCL_PIN, TOUCH_RST_PIN, TOUCH_INT_PIN);
    
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
        Serial.println("[Touch] ✓ CST816S initialized successfully!");
        Serial.printf("[Touch] ✓ Chip ID: 0x%02X\n", touchDriver.readChipID());
        Serial.printf("[Touch] ✓ FW Version: 0x%02X\n", touchDriver.readRevision());
        Serial.printf("[Touch] ✓ I2C Address: 0x%02X\n", touchDriver.activeAddress());
        Serial.println("[Touch] ✓ Touch controller is ready");
        Serial.printf("[Touch] Gesture thresholds - Distance:%d px, Time:%d ms\n", 
            SWIPE_MIN_DISTANCE, SWIPE_MAX_TIME_MS);
    } else {
        Serial.println("[Touch] ✗ CST816S initialization failed!");
        Serial.println("[Touch] ✗ Touch input will not work!");
    }
    
    /* Create LVGL input device even if touch controller failed */
    /* The driver will handle missing hardware gracefully */
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = lvgl_touch_read_cb;
    touch_indev = lv_indev_drv_register(&indev_drv);
    
    Serial.println("[Touch] LVGL touch input driver registered");
    Serial.println("====================================");
}

/**
 * @brief Get LVGL touch input device
 */
lv_indev_t* lvgl_get_touch_indev() {
    return touch_indev;
}

/**
 * @brief Update gesture detection (called from main loop)
 */
void lvgl_touch_update_gestures() {
    /* Gesture detection now happens in the touch read callback */
    /* This function is kept for future enhancements */
}

/**
 * @brief Check if touch is initialized and working
 */
bool lvgl_touch_is_ready() {
    return touchInitialized && touchDriver.isReady();
}

