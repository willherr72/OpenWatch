/**
 * @file watch_face.cpp
 * @brief Enhanced modern watch face implementation inspired by LVGL NXP demo
 */

#include "watch_face.h"
#include "sensor_handler.h"
#include <time.h>
#include <sys/time.h>
#include <math.h>

/* UI Objects */
static lv_obj_t *screen = nullptr;
static lv_obj_t *time_label = nullptr;
static lv_obj_t *date_label = nullptr;
static lv_obj_t *day_label = nullptr;

/* Analog clock hands */
static lv_obj_t *hour_hand = nullptr;
static lv_obj_t *minute_hand = nullptr;
static lv_obj_t *second_hand = nullptr;
static lv_point_t hour_points[2];
static lv_point_t minute_points[2];
static lv_point_t second_points[2];

/* Status indicators */
static lv_obj_t *battery_arc = nullptr;
static lv_obj_t *battery_label = nullptr;
static lv_obj_t *battery_icon = nullptr;

static lv_obj_t *steps_icon = nullptr;
static lv_obj_t *steps_label = nullptr;

static lv_obj_t *heart_icon = nullptr;
static lv_obj_t *heart_label = nullptr;

static lv_obj_t *weather_icon = nullptr;
static lv_obj_t *weather_label = nullptr;

static lv_obj_t *timer_icon = nullptr;
static lv_obj_t *timer_label = nullptr;

static lv_obj_t *wifi_icon = nullptr;
static lv_obj_t *bt_icon = nullptr;
static lv_obj_t *messages_icon = nullptr;

/* Status data */
static bool wifi_connected = false;
static bool time_synced = false;
static int battery_level = 86;
static bool battery_charging = false;
static int heart_rate = 0;

/* Time cache to minimize updates */
static int last_hour = -1;
static int last_minute = -1;
static int last_second = -1;
static int last_day = -1;

/**
 * @brief Create the background
 */
static void create_background() {
    /* Dark theme background */
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_grad_color(screen, lv_color_hex(0x0a0a0a), 0);
    lv_obj_set_style_bg_grad_dir(screen, LV_GRAD_DIR_VER, 0);
}

/**
 * @brief Create battery indicator at top
 */
static void create_battery_indicator() {
    /* Battery percentage and icon at top - compact design */
    battery_icon = lv_label_create(screen);
    lv_label_set_text(battery_icon, LV_SYMBOL_CHARGE);
    lv_obj_set_style_text_font(battery_icon, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(battery_icon, lv_color_hex(0x00d9ff), 0);
    lv_obj_align(battery_icon, LV_ALIGN_TOP_MID, -18, 15);
    
    battery_label = lv_label_create(screen);
    lv_label_set_text_fmt(battery_label, "%d%%", battery_level);
    lv_obj_set_style_text_font(battery_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(battery_label, lv_color_hex(0x00d9ff), 0);
    lv_obj_align(battery_label, LV_ALIGN_TOP_MID, 12, 17);
}

/**
 * @brief Create hour markers around the edge
 */
static void create_hour_markers() {
    for (int i = 0; i < 12; i++) {
        int angle = i * 30;
        float rad = (angle - 90) * 3.14159 / 180.0;
        int radius = 110;
        int x = 120 + (int)(radius * cos(rad));
        int y = 120 + (int)(radius * sin(rad));
        
        lv_obj_t *marker = lv_obj_create(screen);
        if (i % 3 == 0) {
            lv_obj_set_size(marker, 3, 8);
            lv_obj_set_style_bg_color(marker, lv_color_hex(0x555555), 0);
        } else {
            lv_obj_set_size(marker, 2, 5);
            lv_obj_set_style_bg_color(marker, lv_color_hex(0x333333), 0);
        }
        lv_obj_set_style_border_width(marker, 0, 0);
        lv_obj_set_style_radius(marker, 1, 0);
        lv_obj_set_pos(marker, x - 1, y - 2);
    }
}

/**
 * @brief Create main time display (digital + analog)
 */
static void create_time_display() {
    /* Digital time label - smaller and positioned higher */
    time_label = lv_label_create(screen);
    lv_label_set_text(time_label, "12:00");
    lv_obj_set_style_text_font(time_label, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(time_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(time_label, LV_ALIGN_CENTER, 0, -30);
    
    /* Add subtle glow to time */
    lv_obj_set_style_shadow_width(time_label, 12, 0);
    lv_obj_set_style_shadow_color(time_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_shadow_opa(time_label, LV_OPA_20, 0);
    
    /* Create analog clock hands */
    // Hour hand (shortest, thickest)
    hour_hand = lv_line_create(screen);
    hour_points[0] = {120, 120};
    hour_points[1] = {120, 80};
    lv_line_set_points(hour_hand, hour_points, 2);
    lv_obj_set_style_line_width(hour_hand, 4, 0);
    lv_obj_set_style_line_color(hour_hand, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_line_rounded(hour_hand, true, 0);
    
    // Minute hand (medium length)
    minute_hand = lv_line_create(screen);
    minute_points[0] = {120, 120};
    minute_points[1] = {120, 60};
    lv_line_set_points(minute_hand, minute_points, 2);
    lv_obj_set_style_line_width(minute_hand, 3, 0);
    lv_obj_set_style_line_color(minute_hand, lv_color_hex(0x00d9ff), 0);
    lv_obj_set_style_line_rounded(minute_hand, true, 0);
    
    // Second hand (longest, thinnest, red like traditional watches)
    second_hand = lv_line_create(screen);
    second_points[0] = {120, 120};
    second_points[1] = {120, 50};
    lv_line_set_points(second_hand, second_points, 2);
    lv_obj_set_style_line_width(second_hand, 2, 0);
    lv_obj_set_style_line_color(second_hand, lv_color_hex(0xFF6B35), 0);  // Orange/red
    lv_obj_set_style_line_rounded(second_hand, true, 0);
    
    // Center dot
    lv_obj_t *center = lv_obj_create(screen);
    lv_obj_set_size(center, 8, 8);
    lv_obj_set_style_radius(center, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(center, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_width(center, 0, 0);
    lv_obj_align(center, LV_ALIGN_CENTER, 0, 0);
}

/**
 * @brief Create day/date display
 */
static void create_date_display() {
    /* Day of week - left side, vertically centered */
    day_label = lv_label_create(screen);
    lv_label_set_text(day_label, "WED");
    lv_obj_set_style_text_font(day_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(day_label, lv_color_hex(0xFFB74D), 0);  // Orange/gold
    lv_obj_set_style_text_letter_space(day_label, 1, 0);
    lv_obj_align(day_label, LV_ALIGN_LEFT_MID, 25, -15);
    
    /* Date number below day */
    date_label = lv_label_create(screen);
    lv_label_set_text(date_label, "12");
    lv_obj_set_style_text_font(date_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(date_label, lv_color_hex(0xFFB74D), 0);
    lv_obj_align(date_label, LV_ALIGN_LEFT_MID, 25, 10);
}

/**
 * @brief Create status indicators
 */
static void create_status_indicators() {
    /* Steps counter - bottom left, well within visible area */
    steps_icon = lv_label_create(screen);
    lv_label_set_text(steps_icon, LV_SYMBOL_SHUFFLE);  // This symbol works!
    lv_obj_set_style_text_font(steps_icon, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(steps_icon, lv_color_hex(0xFF6B35), 0);  // Orange
    lv_obj_align(steps_icon, LV_ALIGN_LEFT_MID, 55, 30);
    
    steps_label = lv_label_create(screen);
    lv_label_set_text(steps_label, "0");  // Will be updated by sensor handler
    lv_obj_set_style_text_font(steps_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(steps_label, lv_color_hex(0xFF6B35), 0);
    lv_obj_align_to(steps_label, steps_icon, LV_ALIGN_OUT_BOTTOM_MID, 0, 2);
    
    /* Heart rate - bottom center */
    heart_icon = lv_label_create(screen);
    lv_label_set_text(heart_icon, "HR");  // Simple text label for heart rate
    lv_obj_set_style_text_font(heart_icon, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(heart_icon, lv_color_hex(0xFF4444), 0);  // Red
    lv_obj_align(heart_icon, LV_ALIGN_BOTTOM_MID, 0, -30);
    
    heart_label = lv_label_create(screen);
    lv_label_set_text(heart_label, "--");
    lv_obj_set_style_text_font(heart_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(heart_label, lv_color_hex(0xFF4444), 0);
    lv_obj_align_to(heart_label, heart_icon, LV_ALIGN_OUT_BOTTOM_MID, 0, 2);
    
    /* Weather - bottom right, well within visible area */
    weather_icon = lv_label_create(screen);
    lv_label_set_text(weather_icon, LV_SYMBOL_IMAGE);  // Keep emoji image symbol for watch face
    lv_obj_set_style_text_font(weather_icon, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(weather_icon, lv_color_hex(0x88CCFF), 0);  // Light blue
    lv_obj_align(weather_icon, LV_ALIGN_RIGHT_MID, -55, 30);
    
    weather_label = lv_label_create(screen);
    lv_label_set_text(weather_label, "--°");
    lv_obj_set_style_text_font(weather_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(weather_label, lv_color_hex(0x88CCFF), 0);
    lv_obj_align_to(weather_label, weather_icon, LV_ALIGN_OUT_BOTTOM_MID, 0, 2);
    
    /* Timer - top center (only shown when active) - centered like battery but lower */
    timer_icon = lv_label_create(screen);
    lv_label_set_text(timer_icon, LV_SYMBOL_LOOP);  // Clock/timer symbol
    lv_obj_set_style_text_font(timer_icon, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(timer_icon, lv_color_hex(0xAA66FF), 0);  // Purple
    lv_obj_align(timer_icon, LV_ALIGN_TOP_MID, -20, 32);  // Centered like battery, y=32
    lv_obj_add_flag(timer_icon, LV_OBJ_FLAG_HIDDEN);  // Hidden by default
    
    timer_label = lv_label_create(screen);
    lv_label_set_text(timer_label, "0:00");
    lv_obj_set_style_text_font(timer_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(timer_label, lv_color_hex(0xAA66FF), 0);
    lv_obj_align(timer_label, LV_ALIGN_TOP_MID, 10, 34);  // Centered like battery, y=34
    lv_obj_add_flag(timer_label, LV_OBJ_FLAG_HIDDEN);  // Hidden by default
    
    /* Connectivity icons - centered below clock hands */
    // WiFi icon (shown in WiFi mode)
    wifi_icon = lv_label_create(screen);
    lv_label_set_text(wifi_icon, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_font(wifi_icon, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(wifi_icon, lv_color_hex(0x00FF88), 0);  // Start green
    lv_obj_align(wifi_icon, LV_ALIGN_CENTER, -12, 40);
    
    // Bluetooth icon (shown in BLE mode)
    bt_icon = lv_label_create(screen);
    lv_label_set_text(bt_icon, LV_SYMBOL_BLUETOOTH);
    lv_obj_set_style_text_font(bt_icon, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(bt_icon, lv_color_hex(0x00AAFF), 0);  // Blue
    lv_obj_align(bt_icon, LV_ALIGN_CENTER, -12, 40);  // Same position as WiFi
    lv_obj_add_flag(bt_icon, LV_OBJ_FLAG_HIDDEN);  // Hidden by default (WiFi mode)
    
    // Notifications icon
    messages_icon = lv_label_create(screen);
    lv_label_set_text(messages_icon, LV_SYMBOL_BELL);
    lv_obj_set_style_text_font(messages_icon, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(messages_icon, lv_color_hex(0x666666), 0);
    lv_obj_align(messages_icon, LV_ALIGN_CENTER, 25, 40);
}

/**
 * @brief Create brand logo (removed for cleaner look)
 */
static void create_brand_logo() {
    // Logo removed per user request for cleaner watch face
}

/**
 * @brief Update WiFi status indicator
 */
void watch_face_set_connectivity_status(bool connected, bool is_ble) {
    if (is_ble) {
        // BLE mode - show Bluetooth icon, hide WiFi
        if (wifi_icon) {
            lv_obj_add_flag(wifi_icon, LV_OBJ_FLAG_HIDDEN);
        }
        if (bt_icon) {
            lv_obj_clear_flag(bt_icon, LV_OBJ_FLAG_HIDDEN);
            if (connected) {
                lv_obj_set_style_text_color(bt_icon, lv_color_hex(0x00AAFF), 0);  // Blue
            } else {
                lv_obj_set_style_text_color(bt_icon, lv_color_hex(0xFF4444), 0);  // Red
            }
        }
    } else {
        // WiFi mode - show WiFi icon, hide Bluetooth
        if (bt_icon) {
            lv_obj_add_flag(bt_icon, LV_OBJ_FLAG_HIDDEN);
        }
        if (wifi_icon) {
            lv_obj_clear_flag(wifi_icon, LV_OBJ_FLAG_HIDDEN);
            if (connected) {
                lv_obj_set_style_text_color(wifi_icon, lv_color_hex(0x00FF88), 0);  // Green
            } else {
                lv_obj_set_style_text_color(wifi_icon, lv_color_hex(0xFF4444), 0);  // Red
            }
        }
    }
    wifi_connected = connected;  // Keep for backwards compatibility
}

/**
 * @brief Update time sync status
 */
void watch_face_set_time_synced(bool synced) {
    time_synced = synced;
}

/**
 * @brief Set battery level
 */
void watch_face_set_battery(int level, bool charging) {
    battery_level = level;
    battery_charging = charging;
    
    /* Update battery label */
    if (battery_label) {
        lv_label_set_text_fmt(battery_label, "%d%%", level);
    }
    
    /* Update icon and color based on level and charging status */
    if (battery_icon) {
        if (charging) {
            lv_label_set_text(battery_icon, LV_SYMBOL_CHARGE);
            lv_obj_set_style_text_color(battery_icon, lv_color_hex(0x00d9ff), 0);
            if (battery_label) lv_obj_set_style_text_color(battery_label, lv_color_hex(0x00d9ff), 0);
        } else {
            lv_label_set_text(battery_icon, LV_SYMBOL_BATTERY_FULL);
            
            /* Change color based on level */
            if (level > 60) {
                lv_obj_set_style_text_color(battery_icon, lv_color_hex(0x00d9ff), 0);
                if (battery_label) lv_obj_set_style_text_color(battery_label, lv_color_hex(0x00d9ff), 0);
            } else if (level > 20) {
                lv_obj_set_style_text_color(battery_icon, lv_color_hex(0xFFB74D), 0);
                if (battery_label) lv_obj_set_style_text_color(battery_label, lv_color_hex(0xFFB74D), 0);
            } else {
                lv_obj_set_style_text_color(battery_icon, lv_color_hex(0xFF4444), 0);
                if (battery_label) lv_obj_set_style_text_color(battery_label, lv_color_hex(0xFF4444), 0);
            }
        }
    }
}

/**
 * @brief Set heart rate
 */
void watch_face_set_heart_rate(int bpm) {
    heart_rate = bpm;
    if (heart_label) {
        if (bpm > 0) {
            lv_label_set_text_fmt(heart_label, "%d", bpm);
        } else {
            lv_label_set_text(heart_label, "--");
        }
    }
}

/**
 * @brief Set weather display
 */
void watch_face_set_weather(float temp, const char* icon_text) {
    if (weather_label) {
        /* Update temperature */
        static char buf[8];
        snprintf(buf, sizeof(buf), "%.0f°", temp);
        lv_label_set_text(weather_label, buf);
        
        /* Keep the emoji icon on watch face - don't change it */
        // Icon stays as LV_SYMBOL_IMAGE for consistency
    }
}

/**
 * @brief Set steps display
 */
void watch_face_set_steps(int steps) {
    if (steps_label) {
        /* Format with comma for thousands */
        static char buf[16];
        if (steps >= 1000) {
            snprintf(buf, sizeof(buf), "%d,%03d", steps / 1000, steps % 1000);
        } else {
            snprintf(buf, sizeof(buf), "%d", steps);
        }
        lv_label_set_text(steps_label, buf);
    }
}

/**
 * @brief Initialize watch face
 */
void watch_face_init() {
    Serial.println("[WatchFace] Creating enhanced screen...");
    
    /* Create screen */
    screen = lv_obj_create(NULL);
    
    /* Create UI elements in layered order */
    create_background();
    create_hour_markers();
    create_battery_indicator();
    create_time_display();
    create_date_display();
    create_status_indicators();
    create_brand_logo();
    
    /* Load the screen */
    lv_scr_load(screen);
    
    /* Force redraw */
    lv_obj_invalidate(screen);
    
    Serial.println("[WatchFace] Enhanced watch face created!");
}

/**
 * @brief Update watch face with current time
 */
void watch_face_update() {
    if (!screen || !time_label) {
        return;
    }
    
    /* Get current time */
    time_t now;
    struct tm timeinfo;
    time(&now);
    
    /* localtime_r() automatically applies the timezone offset configured via configTime() */
    localtime_r(&now, &timeinfo);
    
    int hour = timeinfo.tm_hour;
    int minute = timeinfo.tm_min;
    int second = timeinfo.tm_sec;
    int day = timeinfo.tm_mday;
    
    /* Update time display (HH:MM) */
    if (hour != last_hour || minute != last_minute) {
        /* Convert to 12-hour format */
        int hour12 = hour % 12;
        if (hour12 == 0) hour12 = 12;
        
        char time_str[16];
        snprintf(time_str, sizeof(time_str), "%2d:%02d", hour12, minute);
        lv_label_set_text(time_label, time_str);
        
        /* Update analog clock hands */
        if (hour_hand && minute_hand) {
            // Calculate angles (12 o'clock = 0°, clockwise)
            float hour_angle = ((hour12 % 12) * 30.0) + (minute * 0.5) - 90;  // -90 to start at top
            float minute_angle = (minute * 6.0) - 90;  // -90 to start at top
            
            // Convert to radians
            float hour_rad = hour_angle * 3.14159 / 180.0;
            float minute_rad = minute_angle * 3.14159 / 180.0;
            
            // Update hour hand (40 pixels long)
            hour_points[0] = {120, 120};
            hour_points[1] = {(lv_coord_t)(120 + 40 * cos(hour_rad)), 
                             (lv_coord_t)(120 + 40 * sin(hour_rad))};
            lv_line_set_points(hour_hand, hour_points, 2);
            
            // Update minute hand (60 pixels long)
            minute_points[0] = {120, 120};
            minute_points[1] = {(lv_coord_t)(120 + 60 * cos(minute_rad)), 
                               (lv_coord_t)(120 + 60 * sin(minute_rad))};
            lv_line_set_points(minute_hand, minute_points, 2);
        }
        
        last_hour = hour;
        last_minute = minute;
    }
    
    /* Update second hand (updates every second) */
    if (second != last_second && second_hand) {
        // Calculate angle for second hand
        float second_angle = (second * 6.0) - 90;  // -90 to start at top
        float second_rad = second_angle * 3.14159 / 180.0;
        
        // Update second hand (70 pixels long)
        second_points[0] = {120, 120};
        second_points[1] = {(lv_coord_t)(120 + 70 * cos(second_rad)), 
                           (lv_coord_t)(120 + 70 * sin(second_rad))};
        lv_line_set_points(second_hand, second_points, 2);
        
        last_second = second;
    }
    
    /* Update date display */
    if (day != last_day) {
        /* Day of week (abbreviated) */
        const char* days[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
        lv_label_set_text(day_label, days[timeinfo.tm_wday]);
        
        /* Day number */
        lv_label_set_text_fmt(date_label, "%d", timeinfo.tm_mday);
        
        last_day = day;
    }
    
    /* Dim display if time not synced */
    if (!time_synced) {
        lv_obj_set_style_text_opa(time_label, LV_OPA_70, 0);
    } else {
        lv_obj_set_style_text_opa(time_label, LV_OPA_COVER, 0);
    }
    
    /* SPOOFED DATA FOR VIDEO - Replace with real code when done */
    /* Spoofed step count */
    watch_face_set_steps(7842);
    
    /* Spoofed heart rate */
    watch_face_set_heart_rate(72);
}

/**
 * @brief Get watch face screen
 */
lv_obj_t* watch_face_get_screen() {
    return screen;
}

/**
 * @brief Manually set the time
 */
void watch_face_set_time_manually(int hour, int minute) {
    /* Create a time structure with the desired time */
    struct tm timeinfo;
    time_t now;
    time(&now);
    localtime_r(&now, &timeinfo);
    
    /* Set the new time */
    timeinfo.tm_hour = hour;
    timeinfo.tm_min = minute;
    timeinfo.tm_sec = 0;
    
    /* Convert to time_t */
    time_t new_time = mktime(&timeinfo);
    
    /* Set system time */
    struct timeval tv;
    tv.tv_sec = new_time;
    tv.tv_usec = 0;
    settimeofday(&tv, NULL);
    
    Serial.printf("[WatchFace] Time manually set to %02d:%02d\n", hour, minute);
    
    /* Mark as time synced */
    watch_face_set_time_synced(true);
}

/**
 * @brief Set timer display
 */
void watch_face_set_timer(int minutes, int seconds, bool running) {
    if (!timer_icon || !timer_label) {
        return;
    }
    
    // Hide timer if at 0:00 (whether running or not - means timer is not set or finished)
    if (minutes == 0 && seconds == 0) {
        lv_obj_add_flag(timer_icon, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(timer_label, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    
    // Show and update timer
    lv_obj_clear_flag(timer_icon, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(timer_label, LV_OBJ_FLAG_HIDDEN);
    
    static char buf[16];
    snprintf(buf, sizeof(buf), "%d:%02d", minutes, seconds);
    lv_label_set_text(timer_label, buf);
    
    // Change color based on state
    if (running) {
        lv_obj_set_style_text_color(timer_icon, lv_color_hex(0x00FF88), 0);  // Green when running
        lv_obj_set_style_text_color(timer_label, lv_color_hex(0x00FF88), 0);
    } else {
        lv_obj_set_style_text_color(timer_icon, lv_color_hex(0xFF4444), 0);  // Red when finished/stopped
        lv_obj_set_style_text_color(timer_label, lv_color_hex(0xFF4444), 0);
    }
}
