/**
 * @file watch_face.cpp
 * @brief Beautiful modern watch face implementation
 */

#include "watch_face.h"
#include <time.h>

/* UI Objects */
static lv_obj_t *screen = nullptr;
static lv_obj_t *time_label = nullptr;
static lv_obj_t *date_label = nullptr;
static lv_obj_t *day_label = nullptr;
static lv_obj_t *second_arc = nullptr;
static lv_obj_t *wifi_label = nullptr;
static lv_obj_t *status_container = nullptr;

/* Status flags */
static bool wifi_connected = false;
static bool time_synced = false;

/* Time cache to minimize updates */
static int last_hour = -1;
static int last_minute = -1;
static int last_second = -1;
static int last_day = -1;

/**
 * @brief Create the background with gradient
 */
static void create_background() {
    /* Set dark theme background - make sure it's opaque */
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_grad_color(screen, lv_color_hex(0x1a1a2e), 0);
    lv_obj_set_style_bg_grad_dir(screen, LV_GRAD_DIR_VER, 0);
}

/**
 * @brief Create circular second indicator arc
 */
static void create_second_arc() {
    second_arc = lv_arc_create(screen);
    lv_obj_set_size(second_arc, 220, 220);
    lv_obj_center(second_arc);
    
    /* Remove background arc, only show indicator */
    lv_obj_remove_style(second_arc, NULL, LV_PART_MAIN);
    lv_obj_set_style_arc_width(second_arc, 0, LV_PART_MAIN);
    
    /* Style the indicator (foreground arc) */
    lv_obj_set_style_arc_color(second_arc, lv_color_hex(0x00d9ff), LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(second_arc, 4, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(second_arc, true, LV_PART_INDICATOR);
    
    /* Remove knob */
    lv_obj_remove_style(second_arc, NULL, LV_PART_KNOB);
    lv_obj_set_style_opa(second_arc, 0, LV_PART_KNOB);
    
    /* Set arc range (0-60 seconds) */
    lv_arc_set_range(second_arc, 0, 60);
    lv_arc_set_value(second_arc, 0);
    lv_arc_set_bg_angles(second_arc, 0, 360);
    
    /* Rotate to start at top (12 o'clock) */
    lv_arc_set_rotation(second_arc, 270);
}

/**
 * @brief Create main time display
 */
static void create_time_display() {
    /* Container for time */
    lv_obj_t *time_container = lv_obj_create(screen);
    lv_obj_set_size(time_container, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_center(time_container);
    lv_obj_set_style_bg_opa(time_container, 0, 0);
    lv_obj_set_style_border_width(time_container, 0, 0);
    lv_obj_set_style_pad_all(time_container, 0, 0);
    lv_obj_clear_flag(time_container, LV_OBJ_FLAG_SCROLLABLE);
    
    /* Time label (HH:MM) - using large font */
    time_label = lv_label_create(time_container);
    lv_label_set_text(time_label, "12:00");
    lv_obj_set_style_text_font(time_label, &lv_font_montserrat_48, 0);  // Big, readable font!
    lv_obj_set_style_text_color(time_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(time_label, LV_ALIGN_CENTER, 0, 0);
    
    /* Add subtle glow effect to time */
    lv_obj_set_style_shadow_width(time_label, 20, 0);
    lv_obj_set_style_shadow_color(time_label, lv_color_hex(0x00d9ff), 0);
    lv_obj_set_style_shadow_opa(time_label, LV_OPA_30, 0);
    lv_obj_set_style_shadow_spread(time_label, 2, 0);
}

/**
 * @brief Create date display
 */
static void create_date_display() {
    /* Day of week */
    day_label = lv_label_create(screen);
    lv_label_set_text(day_label, "MONDAY");
    lv_obj_set_style_text_font(day_label, &lv_font_montserrat_16, 0);  // Slightly larger
    lv_obj_set_style_text_color(day_label, lv_color_hex(0x00d9ff), 0);
    lv_obj_align(day_label, LV_ALIGN_CENTER, 0, -80);
    
    /* Date (Month Day) */
    date_label = lv_label_create(screen);
    lv_label_set_text(date_label, "JAN 1");
    lv_obj_set_style_text_font(date_label, &lv_font_montserrat_20, 0);  // Larger date
    lv_obj_set_style_text_color(date_label, lv_color_hex(0x888888), 0);
    lv_obj_align(date_label, LV_ALIGN_CENTER, 0, 60);
}

/**
 * @brief Create status indicators
 */
static void create_status_indicators() {
    /* Status container at bottom */
    status_container = lv_obj_create(screen);
    lv_obj_set_size(status_container, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_align(status_container, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_set_style_bg_opa(status_container, 0, 0);
    lv_obj_set_style_border_width(status_container, 0, 0);
    lv_obj_set_style_pad_all(status_container, 5, 0);
    lv_obj_clear_flag(status_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(status_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(status_container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    
    /* WiFi status label */
    wifi_label = lv_label_create(status_container);
    lv_label_set_text(wifi_label, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_font(wifi_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(wifi_label, lv_color_hex(0x444444), 0);
    lv_obj_set_style_pad_right(wifi_label, 10, 0);
}

/**
 * @brief Update WiFi status indicator
 */
void watch_face_set_wifi_status(bool connected) {
    wifi_connected = connected;
    if (wifi_label) {
        if (connected) {
            lv_obj_set_style_text_color(wifi_label, lv_color_hex(0x00ff88), 0);
        } else {
            lv_obj_set_style_text_color(wifi_label, lv_color_hex(0x444444), 0);
        }
    }
}

/**
 * @brief Update time sync status
 */
void watch_face_set_time_synced(bool synced) {
    time_synced = synced;
}

/**
 * @brief Initialize watch face
 */
void watch_face_init() {
    Serial.println("[WatchFace] Creating screen object...");
    /* Create screen */
    screen = lv_obj_create(NULL);
    
    Serial.println("[WatchFace] Creating UI elements...");
    /* Create UI elements */
    create_background();
    create_second_arc();
    create_time_display();
    create_date_display();
    create_status_indicators();
    
    Serial.println("[WatchFace] Loading screen...");
    /* Load the screen */
    lv_scr_load(screen);
    
    Serial.println("[WatchFace] Forcing screen invalidation...");
    /* Force LVGL to redraw the entire screen */
    lv_obj_invalidate(screen);
    
    Serial.println("[WatchFace] Watch face UI created and loaded");
}

/**
 * @brief Update watch face with current time
 */
void watch_face_update() {
    static int updateCount = 0;
    if (updateCount < 3) {
        Serial.printf("[WatchFace] watch_face_update() called (count: %d)\n", updateCount);
        updateCount++;
    }
    
    if (!screen || !time_label || !date_label) {
        Serial.println("[WatchFace] ERROR: screen or labels not initialized!");
        return;
    }
    
    /* Get current time */
    time_t now;
    struct tm timeinfo;
    time(&now);
    
    /* Apply timezone offset for Central Time (UTC-5 for CDT) */
    now -= 18000;  // Subtract 5 hours in seconds
    localtime_r(&now, &timeinfo);
    
    int hour = timeinfo.tm_hour;
    int minute = timeinfo.tm_min;
    int second = timeinfo.tm_sec;
    int day = timeinfo.tm_mday;
    
    /* Update seconds arc (smooth circular indicator) */
    if (second_arc) {
        lv_arc_set_value(second_arc, second);
        
        /* Optional: Add smooth animation between seconds */
        static lv_anim_t arc_anim;
        lv_anim_init(&arc_anim);
        lv_anim_set_var(&arc_anim, second_arc);
        lv_anim_set_exec_cb(&arc_anim, (lv_anim_exec_xcb_t)lv_arc_set_value);
        lv_anim_set_values(&arc_anim, second, (second + 1) % 60);
        lv_anim_set_time(&arc_anim, 1000);  // 1 second smooth transition
        lv_anim_start(&arc_anim);
        
        last_second = second;
    }
    
    /* Update time display (HH:MM) */
    if (hour != last_hour || minute != last_minute) {
        /* Convert to 12-hour format */
        int hour12 = hour % 12;
        if (hour12 == 0) hour12 = 12;
        
        char time_str[16];
        snprintf(time_str, sizeof(time_str), "%2d:%02d", hour12, minute);
        lv_label_set_text(time_label, time_str);
        
        last_hour = hour;
        last_minute = minute;
    }
    
    /* Update date display */
    if (day != last_day) {
        /* Day of week */
        const char* days[] = {"SUNDAY", "MONDAY", "TUESDAY", "WEDNESDAY", "THURSDAY", "FRIDAY", "SATURDAY"};
        lv_label_set_text(day_label, days[timeinfo.tm_wday]);
        
        /* Month and day */
        const char* months[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
        char date_str[16];
        snprintf(date_str, sizeof(date_str), "%s %d", months[timeinfo.tm_mon], timeinfo.tm_mday);
        lv_label_set_text(date_label, date_str);
        
        last_day = day;
    }
    
    /* Show "no sync" indicator if time not synced */
    if (!time_synced) {
        /* Dim the time display slightly when not synced */
        lv_obj_set_style_text_opa(time_label, LV_OPA_70, 0);
    } else {
        lv_obj_set_style_text_opa(time_label, LV_OPA_COVER, 0);
    }
}

/**
 * @brief Get watch face screen
 */
lv_obj_t* watch_face_get_screen() {
    return screen;
}

