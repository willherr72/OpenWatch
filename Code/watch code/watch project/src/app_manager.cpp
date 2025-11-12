/**
 * @file app_manager.cpp
 * @brief App manager implementation for smartwatch navigation
 */

#include "app_manager.h"
#include "watch_face.h"

/* Current app state */
static AppType current_app = AppType::WATCH_FACE;
static lv_obj_t *current_screen = nullptr;

/* App screens (created on demand) */
static lv_obj_t *fitness_screen = nullptr;
static lv_obj_t *music_screen = nullptr;
static lv_obj_t *weather_screen = nullptr;
static lv_obj_t *settings_screen = nullptr;
static lv_obj_t *heart_rate_screen = nullptr;
static lv_obj_t *more_settings_screen = nullptr;
static lv_obj_t *time_setting_screen = nullptr;
static lv_obj_t *power_menu_screen = nullptr;

/* Time setting UI elements */
static lv_obj_t *hour_roller = nullptr;
static lv_obj_t *minute_roller = nullptr;

/* Power menu UI elements */
static lv_obj_t *power_off_slider = nullptr;
static lv_obj_t *reboot_slider = nullptr;

/**
 * @brief Create fitness app screen
 */
static lv_obj_t* create_fitness_screen() {
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_grad_color(screen, lv_color_hex(0x1a0a00), 0);
    lv_obj_set_style_bg_grad_dir(screen, LV_GRAD_DIR_VER, 0);
    
    /* Title */
    lv_obj_t *title = lv_label_create(screen);
    lv_label_set_text(title, "FITNESS");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xFF6B35), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 20);
    
    /* Steps arc */
    lv_obj_t *steps_arc = lv_arc_create(screen);
    lv_obj_set_size(steps_arc, 160, 160);
    lv_obj_center(steps_arc);
    lv_obj_set_style_arc_color(steps_arc, lv_color_hex(0x2a2a2a), LV_PART_MAIN);
    lv_obj_set_style_arc_width(steps_arc, 12, LV_PART_MAIN);
    lv_obj_set_style_arc_color(steps_arc, lv_color_hex(0xFF6B35), LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(steps_arc, 12, LV_PART_INDICATOR);
    lv_obj_remove_style(steps_arc, NULL, LV_PART_KNOB);
    lv_obj_set_style_opa(steps_arc, 0, LV_PART_KNOB);
    lv_arc_set_range(steps_arc, 0, 10000);
    lv_arc_set_value(steps_arc, 1526);
    lv_arc_set_bg_angles(steps_arc, 0, 360);
    lv_arc_set_rotation(steps_arc, 270);
    
    /* Steps count */
    lv_obj_t *steps_label = lv_label_create(screen);
    lv_label_set_text(steps_label, "1,526");
    lv_obj_set_style_text_font(steps_label, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(steps_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(steps_label, LV_ALIGN_CENTER, 0, -10);
    
    lv_obj_t *steps_text = lv_label_create(screen);
    lv_label_set_text(steps_text, "STEPS");
    lv_obj_set_style_text_font(steps_text, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(steps_text, lv_color_hex(0x888888), 0);
    lv_obj_align(steps_text, LV_ALIGN_CENTER, 0, 20);
    
    /* Goal */
    lv_obj_t *goal_label = lv_label_create(screen);
    lv_label_set_text(goal_label, "Goal: 10,000");
    lv_obj_set_style_text_font(goal_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(goal_label, lv_color_hex(0x666666), 0);
    lv_obj_align(goal_label, LV_ALIGN_BOTTOM_MID, 0, -20);
    
    /* Swipe hint */
    lv_obj_t *hint = lv_label_create(screen);
    lv_label_set_text(hint, LV_SYMBOL_DOWN " Swipe");
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(hint, lv_color_hex(0x444444), 0);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -5);
    
    return screen;
}

/**
 * @brief Create music player screen
 */
static lv_obj_t* create_music_screen() {
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_grad_color(screen, lv_color_hex(0x0a001a), 0);
    lv_obj_set_style_bg_grad_dir(screen, LV_GRAD_DIR_VER, 0);
    
    /* Title */
    lv_obj_t *title = lv_label_create(screen);
    lv_label_set_text(title, "MUSIC");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xBB86FC), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 20);
    
    /* Album art placeholder */
    lv_obj_t *album = lv_obj_create(screen);
    lv_obj_set_size(album, 120, 120);
    lv_obj_center(album);
    lv_obj_set_style_bg_color(album, lv_color_hex(0x2a2a2a), 0);
    lv_obj_set_style_border_width(album, 0, 0);
    lv_obj_set_style_radius(album, 10, 0);
    
    lv_obj_t *album_icon = lv_label_create(album);
    lv_label_set_text(album_icon, LV_SYMBOL_AUDIO);
    lv_obj_set_style_text_font(album_icon, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(album_icon, lv_color_hex(0xBB86FC), 0);
    lv_obj_center(album_icon);
    
    /* Song info */
    lv_obj_t *song_title = lv_label_create(screen);
    lv_label_set_text(song_title, "No Song Playing");
    lv_obj_set_style_text_font(song_title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(song_title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(song_title, LV_ALIGN_BOTTOM_MID, 0, -50);
    
    /* Controls */
    lv_obj_t *controls = lv_obj_create(screen);
    lv_obj_set_size(controls, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_align(controls, LV_ALIGN_BOTTOM_MID, 0, -15);
    lv_obj_set_style_bg_opa(controls, 0, 0);
    lv_obj_set_style_border_width(controls, 0, 0);
    lv_obj_set_flex_flow(controls, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(controls, 20, 0);
    
    lv_obj_t *prev = lv_label_create(controls);
    lv_label_set_text(prev, LV_SYMBOL_PREV);
    lv_obj_set_style_text_font(prev, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(prev, lv_color_hex(0x888888), 0);
    
    lv_obj_t *play = lv_label_create(controls);
    lv_label_set_text(play, LV_SYMBOL_PLAY);
    lv_obj_set_style_text_font(play, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(play, lv_color_hex(0xBB86FC), 0);
    
    lv_obj_t *next = lv_label_create(controls);
    lv_label_set_text(next, LV_SYMBOL_NEXT);
    lv_obj_set_style_text_font(next, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(next, lv_color_hex(0x888888), 0);
    
    return screen;
}

/**
 * @brief Create weather screen
 */
static lv_obj_t* create_weather_screen() {
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_grad_color(screen, lv_color_hex(0x001a2a), 0);
    lv_obj_set_style_bg_grad_dir(screen, LV_GRAD_DIR_VER, 0);
    
    /* Title */
    lv_obj_t *title = lv_label_create(screen);
    lv_label_set_text(title, "WEATHER");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0x88CCFF), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 20);
    
    /* Weather icon */
    lv_obj_t *icon = lv_label_create(screen);
    lv_label_set_text(icon, "\xE2\x98\x81");  // Unicode cloud symbol ☁
    lv_obj_set_style_text_font(icon, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(icon, lv_color_hex(0x88CCFF), 0);
    lv_obj_align(icon, LV_ALIGN_CENTER, 0, -30);
    
    /* Temperature */
    lv_obj_t *temp = lv_label_create(screen);
    lv_label_set_text(temp, "26°C");
    lv_obj_set_style_text_font(temp, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(temp, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(temp, LV_ALIGN_CENTER, 0, 30);
    
    /* Condition */
    lv_obj_t *condition = lv_label_create(screen);
    lv_label_set_text(condition, "Partly Cloudy");
    lv_obj_set_style_text_font(condition, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(condition, lv_color_hex(0x888888), 0);
    lv_obj_align(condition, LV_ALIGN_BOTTOM_MID, 0, -30);
    
    /* Location */
    lv_obj_t *location = lv_label_create(screen);
    lv_label_set_text(location, "Current Location");
    lv_obj_set_style_text_font(location, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(location, lv_color_hex(0x666666), 0);
    lv_obj_align(location, LV_ALIGN_BOTTOM_MID, 0, -10);
    
    return screen;
}

/**
 * @brief Event handler for More Settings button
 */
static void more_settings_button_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        Serial.println("[AppManager] More settings clicked");
        app_manager_navigate_to(AppType::MORE_SETTINGS);
    }
}

/**
 * @brief Create settings screen (circular menu like LVGL demo)
 */
static lv_obj_t* create_settings_screen() {
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x000000), 0);
    
    /* Title at top (smaller) */
    lv_obj_t *title = lv_label_create(screen);
    lv_label_set_text(title, "Settings");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xAAAAAA), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 12);
    
    /* Vertical menu items - centered */
    int y_start = 55;
    int spacing = 38;
    
    /* WiFi */
    lv_obj_t *wifi_label = lv_label_create(screen);
    lv_label_set_text(wifi_label, "WiFi");
    lv_obj_set_style_text_font(wifi_label, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(wifi_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_pos(wifi_label, 120 - 20, y_start);
    
    /* Bluetooth */
    lv_obj_t *bt_label = lv_label_create(screen);
    lv_label_set_text(bt_label, "Bluetooth");
    lv_obj_set_style_text_font(bt_label, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(bt_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_pos(bt_label, 120 - 45, y_start + spacing);
    
    /* Find your phone */
    lv_obj_t *phone_label = lv_label_create(screen);
    lv_label_set_text(phone_label, "Find your phone");
    lv_obj_set_style_text_font(phone_label, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(phone_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_pos(phone_label, 120 - 76, y_start + spacing * 2);
    
    /* More settings - make it clickable */
    lv_obj_t *more_btn = lv_btn_create(screen);
    lv_obj_set_size(more_btn, 180, 35);
    lv_obj_set_pos(more_btn, 120 - 90, y_start + spacing * 3 - 5);
    lv_obj_set_style_bg_opa(more_btn, 0, 0);
    lv_obj_set_style_bg_opa(more_btn, LV_OPA_20, LV_STATE_PRESSED);
    lv_obj_set_style_border_width(more_btn, 0, 0);
    lv_obj_set_style_shadow_width(more_btn, 0, 0);
    lv_obj_add_event_cb(more_btn, more_settings_button_event_cb, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *more_label = lv_label_create(more_btn);
    lv_label_set_text(more_label, "More settings");
    lv_obj_set_style_text_font(more_label, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(more_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(more_label);
    
    /* Instructions to swipe up */
    lv_obj_t *hint = lv_label_create(screen);
    lv_label_set_text(hint, LV_SYMBOL_UP " Swipe up");
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(hint, lv_color_hex(0x666666), 0);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -10);
    
    return screen;
}

/**
 * @brief Create heart rate screen
 */
static lv_obj_t* create_heart_rate_screen() {
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_grad_color(screen, lv_color_hex(0x1a0000), 0);
    lv_obj_set_style_bg_grad_dir(screen, LV_GRAD_DIR_VER, 0);
    
    /* Title */
    lv_obj_t *title = lv_label_create(screen);
    lv_label_set_text(title, "HEART RATE");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xFF4444), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 20);
    
    /* Heart icon */
    lv_obj_t *icon = lv_label_create(screen);
    lv_label_set_text(icon, "\xE2\x99\xA5");  // Unicode heart symbol ♥
    lv_obj_set_style_text_font(icon, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(icon, lv_color_hex(0xFF4444), 0);
    lv_obj_align(icon, LV_ALIGN_CENTER, 0, -30);
    
    /* BPM */
    lv_obj_t *bpm = lv_label_create(screen);
    lv_label_set_text(bpm, "--");
    lv_obj_set_style_text_font(bpm, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(bpm, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(bpm, LV_ALIGN_CENTER, 0, 30);
    
    /* Status */
    lv_obj_t *status = lv_label_create(screen);
    lv_label_set_text(status, "No Data");
    lv_obj_set_style_text_font(status, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(status, lv_color_hex(0x888888), 0);
    lv_obj_align(status, LV_ALIGN_BOTTOM_MID, 0, -20);
    
    return screen;
}

/**
 * @brief Event handler for Set Time button in More Settings
 */
static void set_time_button_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        Serial.println("[AppManager] Set Time button clicked");
        app_manager_navigate_to(AppType::TIME_SETTING);
    }
}

/**
 * @brief Create More Settings submenu screen
 */
static lv_obj_t* create_more_settings_screen() {
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x000000), 0);
    
    /* Title at top */
    lv_obj_t *title = lv_label_create(screen);
    lv_label_set_text(title, "More Settings");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xAAAAAA), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 12);
    
    /* Set Time button */
    lv_obj_t *set_time_btn = lv_btn_create(screen);
    lv_obj_set_size(set_time_btn, 180, 50);
    lv_obj_align(set_time_btn, LV_ALIGN_CENTER, 0, -10);
    lv_obj_set_style_bg_color(set_time_btn, lv_color_hex(0x2a2a2a), 0);
    lv_obj_set_style_bg_color(set_time_btn, lv_color_hex(0x3a3a3a), LV_STATE_PRESSED);
    lv_obj_set_style_radius(set_time_btn, 12, 0);
    lv_obj_set_style_border_width(set_time_btn, 0, 0);
    lv_obj_add_event_cb(set_time_btn, set_time_button_event_cb, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *btn_label = lv_label_create(set_time_btn);
    lv_label_set_text(btn_label, "Set Time");
    lv_obj_set_style_text_font(btn_label, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(btn_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(btn_label);
    
    /* Swipe hint */
    lv_obj_t *hint = lv_label_create(screen);
    lv_label_set_text(hint, LV_SYMBOL_RIGHT " Swipe back");
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(hint, lv_color_hex(0x666666), 0);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -10);
    
    return screen;
}

/**
 * @brief Event handler for Save Time button
 */
static void save_time_button_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        /* Get selected values from rollers */
        if (hour_roller && minute_roller) {
            uint16_t hour_sel = lv_roller_get_selected(hour_roller);
            uint16_t minute_sel = lv_roller_get_selected(minute_roller);
            
            Serial.printf("[AppManager] Saving time: %02d:%02d\n", hour_sel, minute_sel);
            
            /* Set the time */
            watch_face_set_time_manually(hour_sel, minute_sel);
            
            /* Return to watch face */
            app_manager_navigate_to(AppType::WATCH_FACE);
        }
    }
}

/**
 * @brief Create Time Setting screen with hour/minute rollers
 */
static lv_obj_t* create_time_setting_screen() {
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x000000), 0);
    
    /* Title */
    lv_obj_t *title = lv_label_create(screen);
    lv_label_set_text(title, "Set Time");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0x00d9ff), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 15);
    
    /* Create hour roller - configure styles FIRST, then add options */
    hour_roller = lv_roller_create(screen);
    
    /* Set size and layout first */
    lv_roller_set_visible_row_count(hour_roller, 3);
    lv_obj_set_width(hour_roller, 65);
    
    /* Clear any inherited padding and spacing */
    lv_obj_set_style_pad_top(hour_roller, 0, 0);
    lv_obj_set_style_pad_bottom(hour_roller, 0, 0);
    lv_obj_set_style_pad_left(hour_roller, 0, 0);
    lv_obj_set_style_pad_right(hour_roller, 0, 0);
    lv_obj_set_style_text_line_space(hour_roller, 0, 0);
    
    /* Set font and colors */
    lv_obj_set_style_text_font(hour_roller, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(hour_roller, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_color(hour_roller, lv_color_hex(0xFFFFFF), LV_PART_SELECTED);
    lv_obj_set_style_bg_color(hour_roller, lv_color_hex(0x2a2a2a), 0);
    lv_obj_set_style_bg_color(hour_roller, lv_color_hex(0x00d9ff), LV_PART_SELECTED);
    lv_obj_set_style_text_align(hour_roller, LV_TEXT_ALIGN_CENTER, 0);
    
    /* NOW set the options after all styling is complete */
    lv_roller_set_options(hour_roller,
        "00\n01\n02\n03\n04\n05\n06\n07\n08\n09\n10\n11\n"
        "12\n13\n14\n15\n16\n17\n18\n19\n20\n21\n22\n23",
        LV_ROLLER_MODE_INFINITE);
    
    lv_obj_align(hour_roller, LV_ALIGN_CENTER, -40, -10);
    
    /* Get current time for initial selection */
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    lv_roller_set_selected(hour_roller, timeinfo.tm_hour, LV_ANIM_OFF);
    
    /* Colon separator */
    lv_obj_t *colon = lv_label_create(screen);
    lv_label_set_text(colon, ":");
    lv_obj_set_style_text_font(colon, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(colon, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(colon, LV_ALIGN_CENTER, 0, -10);
    
    /* Create minute roller - configure styles FIRST, then add options */
    minute_roller = lv_roller_create(screen);
    
    /* Set size and layout first */
    lv_roller_set_visible_row_count(minute_roller, 3);
    lv_obj_set_width(minute_roller, 65);
    
    /* Clear any inherited padding and spacing */
    lv_obj_set_style_pad_top(minute_roller, 0, 0);
    lv_obj_set_style_pad_bottom(minute_roller, 0, 0);
    lv_obj_set_style_pad_left(minute_roller, 0, 0);
    lv_obj_set_style_pad_right(minute_roller, 0, 0);
    lv_obj_set_style_text_line_space(minute_roller, 0, 0);
    
    /* Set font and colors */
    lv_obj_set_style_text_font(minute_roller, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(minute_roller, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_color(minute_roller, lv_color_hex(0xFFFFFF), LV_PART_SELECTED);
    lv_obj_set_style_bg_color(minute_roller, lv_color_hex(0x2a2a2a), 0);
    lv_obj_set_style_bg_color(minute_roller, lv_color_hex(0x00d9ff), LV_PART_SELECTED);
    lv_obj_set_style_text_align(minute_roller, LV_TEXT_ALIGN_CENTER, 0);
    
    /* NOW set the options after all styling is complete - use NORMAL mode instead of INFINITE */
    lv_roller_set_options(minute_roller,
        "00\n01\n02\n03\n04\n05\n06\n07\n08\n09\n"
        "10\n11\n12\n13\n14\n15\n16\n17\n18\n19\n"
        "20\n21\n22\n23\n24\n25\n26\n27\n28\n29\n"
        "30\n31\n32\n33\n34\n35\n36\n37\n38\n39\n"
        "40\n41\n42\n43\n44\n45\n46\n47\n48\n49\n"
        "50\n51\n52\n53\n54\n55\n56\n57\n58\n59",
        LV_ROLLER_MODE_NORMAL);
    
    lv_obj_align(minute_roller, LV_ALIGN_CENTER, 40, -10);
    lv_roller_set_selected(minute_roller, timeinfo.tm_min, LV_ANIM_OFF);
    
    /* Force invalidation to refresh rendering */
    lv_obj_invalidate(minute_roller);
    
    /* Save button - smaller size */
    lv_obj_t *save_btn = lv_btn_create(screen);
    lv_obj_set_size(save_btn, 100, 38);
    lv_obj_align(save_btn, LV_ALIGN_BOTTOM_MID, 0, -40);
    lv_obj_set_style_bg_color(save_btn, lv_color_hex(0x00d9ff), 0);
    lv_obj_set_style_bg_color(save_btn, lv_color_hex(0x00a8cc), LV_STATE_PRESSED);
    lv_obj_set_style_radius(save_btn, 10, 0);
    lv_obj_set_style_border_width(save_btn, 0, 0);
    lv_obj_add_event_cb(save_btn, save_time_button_event_cb, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *save_label = lv_label_create(save_btn);
    lv_label_set_text(save_label, "Save");
    lv_obj_set_style_text_font(save_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(save_label, lv_color_hex(0x000000), 0);
    lv_obj_center(save_label);
    
    /* Swipe hint - shorter text */
    lv_obj_t *hint = lv_label_create(screen);
    lv_label_set_text(hint, LV_SYMBOL_RIGHT " Swipe back");
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(hint, lv_color_hex(0x666666), 0);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -10);
    
    return screen;
}

/**
 * @brief Event handler for power off slider
 */
static void power_off_slider_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *slider = lv_event_get_target(e);
    
    if (code == LV_EVENT_VALUE_CHANGED) {
        int value = lv_slider_get_value(slider);
        if (value >= 90) {  // Slid to the end
            Serial.println("[PowerMenu] Power Off activated!");
            Serial.println("[PowerMenu] Pulling GPIO17 LOW to cut power...");
            digitalWrite(17, LOW);  // Cut power
            // Device will shut down here
        }
    }
}

/**
 * @brief Event handler for reboot slider
 */
static void reboot_slider_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *slider = lv_event_get_target(e);
    
    if (code == LV_EVENT_VALUE_CHANGED) {
        int value = lv_slider_get_value(slider);
        if (value >= 90) {  // Slid to the end
            Serial.println("[PowerMenu] Reboot activated!");
            Serial.println("[PowerMenu] Rebooting in 1 second...");
            delay(1000);
            ESP.restart();  // Software reboot
        }
    }
}

/**
 * @brief Create power menu screen
 */
static lv_obj_t* create_power_menu_screen() {
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x000000), 0);
    
    /* Title */
    lv_obj_t *title = lv_label_create(screen);
    lv_label_set_text(title, "Power Menu");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xFF4444), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);
    
    /* Power Off Section - more compact */
    lv_obj_t *power_off_label = lv_label_create(screen);
    lv_label_set_text(power_off_label, "Power Off");
    lv_obj_set_style_text_font(power_off_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(power_off_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(power_off_label, LV_ALIGN_TOP_MID, 0, 45);
    
    power_off_slider = lv_slider_create(screen);
    lv_obj_set_width(power_off_slider, 170);
    lv_obj_set_height(power_off_slider, 10);
    lv_obj_align(power_off_slider, LV_ALIGN_TOP_MID, 0, 70);
    lv_slider_set_range(power_off_slider, 0, 100);
    lv_slider_set_value(power_off_slider, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(power_off_slider, lv_color_hex(0x3a3a3a), LV_PART_MAIN);
    lv_obj_set_style_bg_color(power_off_slider, lv_color_hex(0xFF4444), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(power_off_slider, lv_color_hex(0xFF6666), LV_PART_KNOB);
    lv_obj_set_style_radius(power_off_slider, 5, LV_PART_MAIN);
    lv_obj_add_event_cb(power_off_slider, power_off_slider_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    
    /* Reboot Section - more compact */
    lv_obj_t *reboot_label = lv_label_create(screen);
    lv_label_set_text(reboot_label, "Reboot");
    lv_obj_set_style_text_font(reboot_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(reboot_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(reboot_label, LV_ALIGN_TOP_MID, 0, 105);
    
    reboot_slider = lv_slider_create(screen);
    lv_obj_set_width(reboot_slider, 170);
    lv_obj_set_height(reboot_slider, 10);
    lv_obj_align(reboot_slider, LV_ALIGN_TOP_MID, 0, 130);
    lv_slider_set_range(reboot_slider, 0, 100);
    lv_slider_set_value(reboot_slider, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(reboot_slider, lv_color_hex(0x3a3a3a), LV_PART_MAIN);
    lv_obj_set_style_bg_color(reboot_slider, lv_color_hex(0xFFB74D), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(reboot_slider, lv_color_hex(0xFFCC77), LV_PART_KNOB);
    lv_obj_set_style_radius(reboot_slider, 5, LV_PART_MAIN);
    lv_obj_add_event_cb(reboot_slider, reboot_slider_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    
    /* Hint - smaller and closer to bottom */
    lv_obj_t *hint = lv_label_create(screen);
    lv_label_set_text(hint, "Slide right to confirm");
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(hint, lv_color_hex(0x666666), 0);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -8);
    
    return screen;
}

/**
 * @brief Load screen with animation
 */
static void load_screen_with_animation(lv_obj_t *screen, lv_scr_load_anim_t anim_type) {
    if (!screen) return;
    
    lv_scr_load_anim(screen, anim_type, 300, 0, false);
    current_screen = screen;
}

/**
 * @brief Initialize app manager
 */
void app_manager_init() {
    Serial.println("[AppManager] Initializing...");
    
    /* Start with watch face */
    current_app = AppType::WATCH_FACE;
    current_screen = watch_face_get_screen();
    
    Serial.println("[AppManager] Initialized");
}

/**
 * @brief Handle swipe gestures
 */
void app_manager_handle_swipe(SwipeDirection dir) {
    Serial.printf("[AppManager] Swipe detected: ");
    
    switch (dir) {
        case SwipeDirection::UP:
            Serial.println("UP");
            /* Swipe up from settings -> back to watch face */
            if (current_app == AppType::SETTINGS) {
                // Don't reinit - just reload existing watch face screen
                load_screen_with_animation(watch_face_get_screen(), LV_SCR_LOAD_ANIM_MOVE_TOP);
                current_app = AppType::WATCH_FACE;
            } else if (current_app == AppType::WATCH_FACE) {
                /* Swipe up from watch face -> Fitness */
                if (!fitness_screen) {
                    fitness_screen = create_fitness_screen();
                }
                load_screen_with_animation(fitness_screen, LV_SCR_LOAD_ANIM_MOVE_TOP);
                current_app = AppType::FITNESS;
            } else if (current_app == AppType::FITNESS) {
                /* From fitness -> weather */
                if (!weather_screen) {
                    weather_screen = create_weather_screen();
                }
                load_screen_with_animation(weather_screen, LV_SCR_LOAD_ANIM_MOVE_TOP);
                current_app = AppType::WEATHER;
            }
            break;
            
        case SwipeDirection::DOWN:
            Serial.println("DOWN");
            /* Swipe down from watch face -> Settings */
            if (current_app == AppType::WATCH_FACE) {
                if (!settings_screen) {
                    settings_screen = create_settings_screen();
                }
                load_screen_with_animation(settings_screen, LV_SCR_LOAD_ANIM_MOVE_BOTTOM);
                current_app = AppType::SETTINGS;
            } else if (current_app == AppType::FITNESS) {
                // Don't reinit - just reload existing watch face screen
                load_screen_with_animation(watch_face_get_screen(), LV_SCR_LOAD_ANIM_MOVE_BOTTOM);
                current_app = AppType::WATCH_FACE;
            } else if (current_app == AppType::WEATHER) {
                if (!fitness_screen) {
                    fitness_screen = create_fitness_screen();
                }
                load_screen_with_animation(fitness_screen, LV_SCR_LOAD_ANIM_MOVE_BOTTOM);
                current_app = AppType::FITNESS;
            } else if (current_app == AppType::MUSIC) {
                // Don't reinit - just reload existing watch face screen
                load_screen_with_animation(watch_face_get_screen(), LV_SCR_LOAD_ANIM_MOVE_BOTTOM);
                current_app = AppType::WATCH_FACE;
            } else if (current_app == AppType::HEART_RATE) {
                // Don't reinit - just reload existing watch face screen
                load_screen_with_animation(watch_face_get_screen(), LV_SCR_LOAD_ANIM_MOVE_BOTTOM);
                current_app = AppType::WATCH_FACE;
            }
            break;
            
        case SwipeDirection::LEFT:
            Serial.println("LEFT");
            /* Swipe left from watch face -> Music */
            if (current_app == AppType::WATCH_FACE) {
                if (!music_screen) {
                    music_screen = create_music_screen();
                }
                load_screen_with_animation(music_screen, LV_SCR_LOAD_ANIM_MOVE_LEFT);
                current_app = AppType::MUSIC;
            }
            break;
            
        case SwipeDirection::RIGHT:
            Serial.println("RIGHT");
            /* Swipe right from watch face -> Heart Rate */
            if (current_app == AppType::WATCH_FACE) {
                if (!heart_rate_screen) {
                    heart_rate_screen = create_heart_rate_screen();
                }
                load_screen_with_animation(heart_rate_screen, LV_SCR_LOAD_ANIM_MOVE_RIGHT);
                current_app = AppType::HEART_RATE;
            } else if (current_app == AppType::MUSIC) {
                // Don't reinit - just reload existing watch face screen
                load_screen_with_animation(watch_face_get_screen(), LV_SCR_LOAD_ANIM_MOVE_RIGHT);
                current_app = AppType::WATCH_FACE;
            } else if (current_app == AppType::MORE_SETTINGS) {
                /* Swipe right from More Settings -> back to Settings */
                if (!settings_screen) {
                    settings_screen = create_settings_screen();
                }
                load_screen_with_animation(settings_screen, LV_SCR_LOAD_ANIM_MOVE_RIGHT);
                current_app = AppType::SETTINGS;
            } else if (current_app == AppType::TIME_SETTING) {
                /* Swipe right from Time Setting -> back to More Settings */
                if (!more_settings_screen) {
                    more_settings_screen = create_more_settings_screen();
                }
                load_screen_with_animation(more_settings_screen, LV_SCR_LOAD_ANIM_MOVE_RIGHT);
                current_app = AppType::MORE_SETTINGS;
            }
            break;
            
        default:
            Serial.println("NONE");
            break;
    }
}

/**
 * @brief Navigate to specific app
 */
void app_manager_navigate_to(AppType app) {
    if (app == current_app) return;
    
    current_app = app;
    
    /* Load appropriate screen */
    switch (app) {
        case AppType::WATCH_FACE:
            // Don't reinit - just reload existing watch face screen
            load_screen_with_animation(watch_face_get_screen(), LV_SCR_LOAD_ANIM_FADE_IN);
            break;
        case AppType::FITNESS:
            if (!fitness_screen) fitness_screen = create_fitness_screen();
            load_screen_with_animation(fitness_screen, LV_SCR_LOAD_ANIM_FADE_IN);
            break;
        case AppType::MUSIC:
            if (!music_screen) music_screen = create_music_screen();
            load_screen_with_animation(music_screen, LV_SCR_LOAD_ANIM_FADE_IN);
            break;
        case AppType::WEATHER:
            if (!weather_screen) weather_screen = create_weather_screen();
            load_screen_with_animation(weather_screen, LV_SCR_LOAD_ANIM_FADE_IN);
            break;
        case AppType::SETTINGS:
            if (!settings_screen) settings_screen = create_settings_screen();
            load_screen_with_animation(settings_screen, LV_SCR_LOAD_ANIM_FADE_IN);
            break;
        case AppType::HEART_RATE:
            if (!heart_rate_screen) heart_rate_screen = create_heart_rate_screen();
            load_screen_with_animation(heart_rate_screen, LV_SCR_LOAD_ANIM_FADE_IN);
            break;
        case AppType::MORE_SETTINGS:
            if (!more_settings_screen) more_settings_screen = create_more_settings_screen();
            load_screen_with_animation(more_settings_screen, LV_SCR_LOAD_ANIM_MOVE_LEFT);
            break;
        case AppType::TIME_SETTING:
            if (!time_setting_screen) time_setting_screen = create_time_setting_screen();
            load_screen_with_animation(time_setting_screen, LV_SCR_LOAD_ANIM_MOVE_LEFT);
            break;
        case AppType::POWER_MENU:
            if (!power_menu_screen) power_menu_screen = create_power_menu_screen();
            load_screen_with_animation(power_menu_screen, LV_SCR_LOAD_ANIM_FADE_IN);
            break;
        default:
            break;
    }
}

/**
 * @brief Get current app
 */
AppType app_manager_get_current() {
    return current_app;
}

/**
 * @brief Return to watch face
 */
void app_manager_return_home() {
    app_manager_navigate_to(AppType::WATCH_FACE);
}

/**
 * @brief Update current app
 */
void app_manager_update() {
    /* Update watch face if it's current */
    if (current_app == AppType::WATCH_FACE) {
        watch_face_update();
    }
    
    /* Add updates for other apps here as needed */
}

