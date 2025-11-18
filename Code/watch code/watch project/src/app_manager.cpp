/**
 * @file app_manager.cpp
 * @brief App manager implementation for smartwatch navigation
 */

#include "app_manager.h"
#include "watch_face.h"
#include "sensor_handler.h"
#include "vibration.h"
#include <math.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "ble_handler.h"

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
static lv_obj_t *compass_screen = nullptr;
static lv_obj_t *gps_screen = nullptr;

/* Time setting UI elements */
static lv_obj_t *hour_roller = nullptr;
static lv_obj_t *minute_roller = nullptr;

/* Power menu UI elements */
static lv_obj_t *power_off_slider = nullptr;
static lv_obj_t *reboot_slider = nullptr;

/* Sensor streaming UI elements */
static lv_obj_t *sensor_streaming_btn = nullptr;

/* Compass UI elements */
static lv_obj_t *compass_yaw_label = nullptr;
static lv_obj_t *compass_pitch_label = nullptr;
static lv_obj_t *compass_roll_label = nullptr;
static lv_obj_t *compass_needle = nullptr;

/* GPS UI elements */
static lv_obj_t *gps_status_label = nullptr;
static lv_obj_t *gps_lat_label = nullptr;
static lv_obj_t *gps_lon_label = nullptr;
static lv_obj_t *gps_alt_label = nullptr;
static lv_obj_t *gps_sat_label = nullptr;

/* Timer UI elements */
static lv_obj_t *timer_screen = nullptr;
static lv_obj_t *timer_display_label = nullptr;
static lv_obj_t *timer_minute_roller = nullptr;
static lv_obj_t *timer_second_roller = nullptr;
static lv_obj_t *timer_btn_start = nullptr;
static lv_obj_t *timer_btn_reset = nullptr;

/* Timer state */
static int timer_minutes = 0;
static int timer_seconds = 0;
static bool timer_running = false;
static unsigned long timer_start_millis = 0;
static int timer_duration_seconds = 0;  // Total duration set
static bool timer_finished = false;
static unsigned long timer_last_button_press = 0;
#define TIMER_BUTTON_DEBOUNCE 300  // 300ms debounce

/* Heart Rate UI elements */
static lv_obj_t *hr_bpm_label = nullptr;
static lv_obj_t *hr_spo2_label = nullptr;
static lv_obj_t *hr_status_label = nullptr;

/* Weather UI elements */
static lv_obj_t *weather_temp_label = nullptr;
static lv_obj_t *weather_condition_label = nullptr;
static lv_obj_t *weather_location_label = nullptr;
static lv_obj_t *weather_icon_label = nullptr;

/* Fitness UI elements */
static lv_obj_t *fitness_steps_arc = nullptr;
static lv_obj_t *fitness_steps_label = nullptr;

/* Settings UI elements */
static lv_obj_t *connection_mode_switch = nullptr;

/* Weather data */
struct WeatherData {
    float temp;
    String condition;
    String location;
    String icon_text;
    bool valid;
    unsigned long last_update;
};
static WeatherData weather_data = {0, "", "Sugar Land, TX", "~", false, 0};

/* NOAA Weather API - Completely free, no API key needed */
#define WEATHER_UPDATE_INTERVAL 600000  // 10 minutes
/* Default location: Sugar Land, TX (zip 77479) */
#define DEFAULT_LAT 29.6197
#define DEFAULT_LON -95.6349

/**
 * @brief Get weather icon text based on condition
 */
static String get_weather_icon(const String& forecast) {
    String fc_lower = forecast;
    fc_lower.toLowerCase();
    
    if (fc_lower.indexOf("sunny") >= 0 || fc_lower.indexOf("clear") >= 0) return "O";  // Sun
    if (fc_lower.indexOf("cloud") >= 0 || fc_lower.indexOf("overcast") >= 0) return "~"; // Clouds
    if (fc_lower.indexOf("rain") >= 0 || fc_lower.indexOf("shower") >= 0) return "R";   // Rain
    if (fc_lower.indexOf("thunder") >= 0 || fc_lower.indexOf("storm") >= 0) return "T";  // Thunderstorm
    if (fc_lower.indexOf("snow") >= 0) return "S";                                      // Snow
    if (fc_lower.indexOf("fog") >= 0 || fc_lower.indexOf("mist") >= 0) return "F";     // Fog
    return "~";  // Default to clouds
}

/**
 * @brief Fetch weather data from NOAA Weather API
 */
static bool fetch_weather_data() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[Weather] WiFi not connected");
        return false;
    }
    
    /* Get GPS coordinates if available, otherwise use default location */
    GPSData gps_data = sensor_handler_get_gps();
    double lat, lon;
    
    if (gps_data.valid) {
        lat = gps_data.latitude;
        lon = gps_data.longitude;
        Serial.printf("[Weather] Using GPS location: %.4f, %.4f\n", lat, lon);
    } else {
        lat = DEFAULT_LAT;
        lon = DEFAULT_LON;
        Serial.println("[Weather] Using default location: Sugar Land, TX");
    }
    
    /* NOAA API requires rounding to 4 decimal places */
    String url = "https://api.weather.gov/points/" + String(lat, 4) + "," + String(lon, 4);
    
    HTTPClient http;
    
    /* Try to resolve DNS issue by flushing and retrying */
    static int dns_fails = 0;
    if (dns_fails > 3) {
        WiFi.disconnect();
        delay(100);
        WiFi.reconnect();
        dns_fails = 0;
        Serial.println("[Weather] Reconnecting WiFi due to DNS issues...");
        return false;
    }
    
    http.begin(url);
    http.setTimeout(15000);  // Longer timeout for HTTPS
    http.addHeader("User-Agent", "OpenWatch-Smartwatch");  // NOAA requires User-Agent
    
    int httpCode = http.GET();
    
    /* Track DNS failures */
    if (httpCode == -1) {
        dns_fails++;
        Serial.printf("[Weather] DNS fail count: %d/3\n", dns_fails);
    } else {
        dns_fails = 0;  // Reset on success
    }
    
    if (httpCode == 200) {
        String payload = http.getString();
        
        /* Parse JSON to get forecast URL */
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, payload);
        
        if (!error) {
            String forecastUrl = doc["properties"]["forecast"].as<String>();
            String city = doc["properties"]["relativeLocation"]["properties"]["city"].as<String>();
            String state = doc["properties"]["relativeLocation"]["properties"]["state"].as<String>();
            
            http.end();
            
            /* Now fetch the actual forecast */
            http.begin(forecastUrl);
            http.addHeader("User-Agent", "OpenWatch-Smartwatch");
            httpCode = http.GET();
            
            if (httpCode == 200) {
                payload = http.getString();
                JsonDocument forecastDoc;
                error = deserializeJson(forecastDoc, payload);
                
                if (!error) {
                    /* Access first period directly without copying proxy */
                    weather_data.temp = forecastDoc["properties"]["periods"][0]["temperature"];
                    weather_data.condition = forecastDoc["properties"]["periods"][0]["shortForecast"].as<String>();
                    weather_data.location = city + ", " + state;
                    weather_data.icon_text = get_weather_icon(weather_data.condition);
                    weather_data.valid = true;
                    weather_data.last_update = millis();
                    
                    Serial.printf("[Weather] Updated: %.0f°F, %s, %s\n", 
                                 weather_data.temp, weather_data.condition.c_str(), 
                                 weather_data.location.c_str());
                    
                    /* Update watch face immediately */
                    watch_face_set_weather(weather_data.temp, weather_data.icon_text.c_str());
                    
                    http.end();
                    return true;
                }
            }
        } else {
            Serial.printf("[Weather] JSON parse error: %s\n", error.c_str());
        }
    } else {
        Serial.printf("[Weather] HTTP error: %d\n", httpCode);
    }
    
    http.end();
    return false;
}

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
    
    /* Steps arc - store reference for live updates */
    fitness_steps_arc = lv_arc_create(screen);
    lv_obj_set_size(fitness_steps_arc, 160, 160);
    lv_obj_center(fitness_steps_arc);
    lv_obj_set_style_arc_color(fitness_steps_arc, lv_color_hex(0x2a2a2a), LV_PART_MAIN);
    lv_obj_set_style_arc_width(fitness_steps_arc, 12, LV_PART_MAIN);
    lv_obj_set_style_arc_color(fitness_steps_arc, lv_color_hex(0xFF6B35), LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(fitness_steps_arc, 12, LV_PART_INDICATOR);
    lv_obj_remove_style(fitness_steps_arc, NULL, LV_PART_KNOB);
    lv_obj_set_style_opa(fitness_steps_arc, 0, LV_PART_KNOB);
    lv_arc_set_range(fitness_steps_arc, 0, 10000);
    lv_arc_set_value(fitness_steps_arc, 0);
    lv_arc_set_bg_angles(fitness_steps_arc, 0, 360);
    lv_arc_set_rotation(fitness_steps_arc, 270);
    
    /* Make arc non-interactive (display only) */
    lv_obj_clear_flag(fitness_steps_arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(fitness_steps_arc, LV_OBJ_FLAG_EVENT_BUBBLE);
    
    /* Steps count - store reference for live updates */
    fitness_steps_label = lv_label_create(screen);
    lv_label_set_text(fitness_steps_label, "0");
    lv_obj_set_style_text_font(fitness_steps_label, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(fitness_steps_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(fitness_steps_label, LV_ALIGN_CENTER, 0, -10);
    
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
    lv_label_set_text(hint, LV_SYMBOL_LEFT " " LV_SYMBOL_DOWN " Swipe");
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
    
    /* Weather icon - use text instead of emoji */
    weather_icon_label = lv_label_create(screen);
    lv_label_set_text(weather_icon_label, "~");  // Will update with weather condition
    lv_obj_set_style_text_font(weather_icon_label, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(weather_icon_label, lv_color_hex(0x88CCFF), 0);
    lv_obj_align(weather_icon_label, LV_ALIGN_CENTER, 0, -35);
    
    /* Temperature */
    weather_temp_label = lv_label_create(screen);
    lv_label_set_text(weather_temp_label, "--°");
    lv_obj_set_style_text_font(weather_temp_label, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(weather_temp_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(weather_temp_label, LV_ALIGN_CENTER, 0, 25);
    
    /* Condition - enable wrapping for long text */
    weather_condition_label = lv_label_create(screen);
    lv_label_set_text(weather_condition_label, "Loading...");
    lv_obj_set_style_text_font(weather_condition_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(weather_condition_label, lv_color_hex(0x888888), 0);
    lv_obj_set_width(weather_condition_label, 220);  // Set max width
    lv_obj_set_style_text_align(weather_condition_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(weather_condition_label, LV_LABEL_LONG_WRAP);  // Wrap long text
    lv_obj_align(weather_condition_label, LV_ALIGN_BOTTOM_MID, 0, -30);
    
    /* Location */
    weather_location_label = lv_label_create(screen);
    lv_label_set_text(weather_location_label, "Sugar Land, TX");
    lv_obj_set_style_text_font(weather_location_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(weather_location_label, lv_color_hex(0x666666), 0);
    lv_obj_align(weather_location_label, LV_ALIGN_BOTTOM_MID, 0, -10);
    
    return screen;
}

/**
 * @brief Event handler for WiFi/BLE mode switch
 */
static void connection_mode_switch_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_VALUE_CHANGED) {
        static unsigned long last_switch_time = 0;
        unsigned long now = millis();
        
        // Debounce - prevent rapid switching (minimum 2 seconds between switches)
        if (now - last_switch_time < 2000) {
            Serial.println("[AppManager] Switch ignored - too soon after last change (debounce)");
            return;
        }
        
        lv_obj_t *sw = lv_event_get_target(e);
        bool is_ble = lv_obj_has_state(sw, LV_STATE_CHECKED);
        ConnectionMode target_mode = is_ble ? ConnectionMode::BLE : ConnectionMode::WIFI;
        
        // Don't switch if already in target mode
        if (ble_handler_get_mode() == target_mode) {
            Serial.println("[AppManager] Already in target mode - ignoring");
            return;
        }
        
        Serial.printf("[AppManager] Connection mode switch requested: %s\n", is_ble ? "BLE" : "WiFi");
        
        // Update mode
        ble_handler_set_mode(target_mode);
        
        last_switch_time = now;
    }
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
    
    /* Title at top */
    lv_obj_t *title = lv_label_create(screen);
    lv_label_set_text(title, "SETTINGS");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0x00AAFF), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 15);
    
    /* Connection mode label */
    lv_obj_t *conn_title = lv_label_create(screen);
    lv_label_set_text(conn_title, "Connection");
    lv_obj_set_style_text_font(conn_title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(conn_title, lv_color_hex(0xAAAAAA), 0);
    lv_obj_align(conn_title, LV_ALIGN_TOP_MID, 0, 55);
    
    /* WiFi/BLE Switch with labels */
    connection_mode_switch = lv_switch_create(screen);
    lv_obj_set_size(connection_mode_switch, 50, 25);
    lv_obj_align(connection_mode_switch, LV_ALIGN_TOP_MID, 0, 85);
    lv_obj_add_event_cb(connection_mode_switch, connection_mode_switch_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    
    /* Prevent gestures from interfering with switch */
    lv_obj_clear_flag(connection_mode_switch, LV_OBJ_FLAG_GESTURE_BUBBLE);
    
    /* Set initial state based on current mode */
    ConnectionMode current_mode = ble_handler_get_mode();
    if (current_mode == ConnectionMode::BLE) {
        lv_obj_add_state(connection_mode_switch, LV_STATE_CHECKED);
    }
    
    /* Mode labels beside switch */
    lv_obj_t *wifi_text = lv_label_create(screen);
    lv_label_set_text(wifi_text, "WiFi");
    lv_obj_set_style_text_font(wifi_text, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(wifi_text, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align_to(wifi_text, connection_mode_switch, LV_ALIGN_OUT_LEFT_MID, -10, 0);
    
    lv_obj_t *ble_text = lv_label_create(screen);
    lv_label_set_text(ble_text, "BLE");
    lv_obj_set_style_text_font(ble_text, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(ble_text, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align_to(ble_text, connection_mode_switch, LV_ALIGN_OUT_RIGHT_MID, 10, 0);
    
    /* More settings button */
    lv_obj_t *more_btn = lv_btn_create(screen);
    lv_obj_set_size(more_btn, 180, 40);
    lv_obj_align(more_btn, LV_ALIGN_CENTER, 0, 25);
    lv_obj_set_style_bg_color(more_btn, lv_color_hex(0x1a1a1a), 0);
    lv_obj_set_style_bg_opa(more_btn, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_opa(more_btn, LV_OPA_50, LV_STATE_PRESSED);
    lv_obj_set_style_border_width(more_btn, 1, 0);
    lv_obj_set_style_border_color(more_btn, lv_color_hex(0x00AAFF), 0);
    lv_obj_set_style_radius(more_btn, 8, 0);
    lv_obj_set_style_shadow_width(more_btn, 0, 0);
    lv_obj_add_event_cb(more_btn, more_settings_button_event_cb, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *more_label = lv_label_create(more_btn);
    lv_label_set_text(more_label, "More Settings");
    lv_obj_set_style_text_font(more_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(more_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(more_label);
    
    /* Swipe hint */
    lv_obj_t *hint = lv_label_create(screen);
    lv_label_set_text(hint, LV_SYMBOL_UP " " LV_SYMBOL_DOWN " Swipe");
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(hint, lv_color_hex(0x444444), 0);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -5);
    
    return screen;
}

/**
 * @brief Create heart rate screen with SpO2
 */
static lv_obj_t* create_heart_rate_screen() {
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_grad_color(screen, lv_color_hex(0x1a0000), 0);
    lv_obj_set_style_bg_grad_dir(screen, LV_GRAD_DIR_VER, 0);
    
    /* Title */
    lv_obj_t *title = lv_label_create(screen);
    lv_label_set_text(title, "HEALTH");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xFF4444), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 15);
    
    /* Heart Rate Section - smaller box */
    lv_obj_t *hr_container = lv_obj_create(screen);
    lv_obj_set_size(hr_container, 200, 55);
    lv_obj_align(hr_container, LV_ALIGN_CENTER, 0, -30);
    lv_obj_set_style_bg_color(hr_container, lv_color_hex(0x0a0000), 0);
    lv_obj_set_style_border_width(hr_container, 1, 0);
    lv_obj_set_style_border_color(hr_container, lv_color_hex(0xFF4444), 0);
    lv_obj_set_style_radius(hr_container, 8, 0);
    lv_obj_set_style_pad_all(hr_container, 6, 0);
    
    /* Heart Rate label */
    lv_obj_t *hr_label = lv_label_create(hr_container);
    lv_label_set_text(hr_label, "HR");
    lv_obj_set_style_text_font(hr_label, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(hr_label, lv_color_hex(0xFF4444), 0);
    lv_obj_align(hr_label, LV_ALIGN_LEFT_MID, 8, 0);
    
    /* BPM Value */
    hr_bpm_label = lv_label_create(hr_container);
    lv_label_set_text(hr_bpm_label, "-- BPM");
    lv_obj_set_style_text_font(hr_bpm_label, &lv_font_montserrat_22, 0);
    lv_obj_set_style_text_color(hr_bpm_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(hr_bpm_label, LV_ALIGN_CENTER, 10, 0);
    
    /* SpO2 Section - smaller box */
    lv_obj_t *spo2_container = lv_obj_create(screen);
    lv_obj_set_size(spo2_container, 200, 55);
    lv_obj_align(spo2_container, LV_ALIGN_CENTER, 0, 35);
    lv_obj_set_style_bg_color(spo2_container, lv_color_hex(0x000a1a), 0);
    lv_obj_set_style_border_width(spo2_container, 1, 0);
    lv_obj_set_style_border_color(spo2_container, lv_color_hex(0x4488FF), 0);
    lv_obj_set_style_radius(spo2_container, 8, 0);
    lv_obj_set_style_pad_all(spo2_container, 6, 0);
    
    /* O2 label */
    lv_obj_t *o2_label = lv_label_create(spo2_container);
    lv_label_set_text(o2_label, "O2");
    lv_obj_set_style_text_font(o2_label, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(o2_label, lv_color_hex(0x4488FF), 0);
    lv_obj_align(o2_label, LV_ALIGN_LEFT_MID, 8, 0);
    
    /* SpO2 Value */
    hr_spo2_label = lv_label_create(spo2_container);
    lv_label_set_text(hr_spo2_label, "-- %");
    lv_obj_set_style_text_font(hr_spo2_label, &lv_font_montserrat_22, 0);
    lv_obj_set_style_text_color(hr_spo2_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(hr_spo2_label, LV_ALIGN_CENTER, 10, 0);
    
    /* Status */
    hr_status_label = lv_label_create(screen);
    lv_label_set_text(hr_status_label, "Waiting for sensor...");
    lv_obj_set_style_text_font(hr_status_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(hr_status_label, lv_color_hex(0x888888), 0);
    lv_obj_align(hr_status_label, LV_ALIGN_BOTTOM_MID, 0, -25);
    
    /* Swipe hint */
    lv_obj_t *hint = lv_label_create(screen);
    lv_label_set_text(hint, LV_SYMBOL_RIGHT " Swipe");
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(hint, lv_color_hex(0x444444), 0);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -5);
    
    return screen;
}

/**
 * @brief Create compass screen
 */
static lv_obj_t* create_compass_screen() {
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_grad_color(screen, lv_color_hex(0x001a0a), 0);
    lv_obj_set_style_bg_grad_dir(screen, LV_GRAD_DIR_VER, 0);
    
    /* Title */
    lv_obj_t *title = lv_label_create(screen);
    lv_label_set_text(title, "COMPASS");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0x00FF88), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 15);
    
    /* Compass circle background */
    lv_obj_t *compass_bg = lv_obj_create(screen);
    lv_obj_set_size(compass_bg, 130, 130);
    lv_obj_align(compass_bg, LV_ALIGN_CENTER, 0, -15);
    lv_obj_set_style_radius(compass_bg, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(compass_bg, lv_color_hex(0x1a1a1a), 0);
    lv_obj_set_style_border_width(compass_bg, 2, 0);
    lv_obj_set_style_border_color(compass_bg, lv_color_hex(0x00FF88), 0);
    lv_obj_set_style_pad_all(compass_bg, 0, 0);
    
    /* Cardinal direction markers */
    lv_obj_t *north = lv_label_create(compass_bg);
    lv_label_set_text(north, "N");
    lv_obj_set_style_text_font(north, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(north, lv_color_hex(0xFF4444), 0);
    lv_obj_align(north, LV_ALIGN_TOP_MID, 0, 3);
    
    lv_obj_t *south = lv_label_create(compass_bg);
    lv_label_set_text(south, "S");
    lv_obj_set_style_text_font(south, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(south, lv_color_hex(0xAAAAAA), 0);
    lv_obj_align(south, LV_ALIGN_BOTTOM_MID, 0, -3);
    
    lv_obj_t *east = lv_label_create(compass_bg);
    lv_label_set_text(east, "E");
    lv_obj_set_style_text_font(east, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(east, lv_color_hex(0xAAAAAA), 0);
    lv_obj_align(east, LV_ALIGN_RIGHT_MID, -3, 0);
    
    lv_obj_t *west = lv_label_create(compass_bg);
    lv_label_set_text(west, "W");
    lv_obj_set_style_text_font(west, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(west, lv_color_hex(0xAAAAAA), 0);
    lv_obj_align(west, LV_ALIGN_LEFT_MID, 3, 0);
    
    /* Compass needle - using absolute positioning within compass_bg */
    compass_needle = lv_line_create(compass_bg);
    static lv_point_t needle_points_init[] = {{65, 65}, {65, 25}};  // From center to top
    lv_line_set_points(compass_needle, needle_points_init, 2);
    lv_obj_set_style_line_width(compass_needle, 3, 0);
    lv_obj_set_style_line_color(compass_needle, lv_color_hex(0xFF4444), 0);
    lv_obj_set_style_line_rounded(compass_needle, true, 0);
    
    /* Center dot */
    lv_obj_t *center = lv_obj_create(compass_bg);
    lv_obj_set_size(center, 6, 6);
    lv_obj_center(center);
    lv_obj_set_style_radius(center, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(center, lv_color_hex(0x00FF88), 0);
    lv_obj_set_style_border_width(center, 0, 0);
    
    /* Simple stacked text layout at bottom - centered */
    int bottom_y = 175;
    
    /* Azimuth row */
    lv_obj_t *azm_label = lv_label_create(screen);
    lv_label_set_text(azm_label, "Azm: ---°");
    lv_obj_set_style_text_font(azm_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(azm_label, lv_color_hex(0xAAAAAA), 0);
    lv_obj_set_pos(azm_label, 0, bottom_y);
    lv_obj_set_width(azm_label, 240);
    lv_obj_set_style_text_align(azm_label, LV_TEXT_ALIGN_CENTER, 0);
    compass_yaw_label = azm_label;  // Store reference for updating
    
    /* Pitch row */
    lv_obj_t *pitch_label = lv_label_create(screen);
    lv_label_set_text(pitch_label, "Pitch: ---°");
    lv_obj_set_style_text_font(pitch_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(pitch_label, lv_color_hex(0xAAAAAA), 0);
    lv_obj_set_pos(pitch_label, 0, bottom_y + 17);
    lv_obj_set_width(pitch_label, 240);
    lv_obj_set_style_text_align(pitch_label, LV_TEXT_ALIGN_CENTER, 0);
    compass_pitch_label = pitch_label;
    
    /* Roll row */
    lv_obj_t *roll_label = lv_label_create(screen);
    lv_label_set_text(roll_label, "Roll: ---°");
    lv_obj_set_style_text_font(roll_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(roll_label, lv_color_hex(0xAAAAAA), 0);
    lv_obj_set_pos(roll_label, 0, bottom_y + 34);
    lv_obj_set_width(roll_label, 240);
    lv_obj_set_style_text_align(roll_label, LV_TEXT_ALIGN_CENTER, 0);
    compass_roll_label = roll_label;
    
    /* Swipe hint */
    lv_obj_t *hint = lv_label_create(screen);
    lv_label_set_text(hint, LV_SYMBOL_RIGHT " Swipe");
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(hint, lv_color_hex(0x555555), 0);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_RIGHT, -5, -3);
    
    return screen;
}

/**
 * @brief Create GPS screen
 */
static lv_obj_t* create_gps_screen() {
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_grad_color(screen, lv_color_hex(0x001a1a), 0);
    lv_obj_set_style_bg_grad_dir(screen, LV_GRAD_DIR_VER, 0);
    
    /* Title */
    lv_obj_t *title = lv_label_create(screen);
    lv_label_set_text(title, "GPS");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0x00D4FF), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 15);
    
    /* GPS Status indicator */
    gps_status_label = lv_label_create(screen);
    lv_label_set_text(gps_status_label, "Searching...");
    lv_obj_set_style_text_font(gps_status_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(gps_status_label, lv_color_hex(0xFFAA00), 0);
    lv_obj_align(gps_status_label, LV_ALIGN_TOP_MID, 0, 45);
    
    /* Satellite icon/count - position below status */
    gps_sat_label = lv_label_create(screen);
    lv_label_set_text(gps_sat_label, LV_SYMBOL_GPS " 0 sats");
    lv_obj_set_style_text_font(gps_sat_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(gps_sat_label, lv_color_hex(0x888888), 0);
    lv_obj_align(gps_sat_label, LV_ALIGN_TOP_MID, 0, 67);
    
    /* Coordinates section - moved down to avoid overlap */
    lv_obj_t *coord_container = lv_obj_create(screen);
    lv_obj_set_size(coord_container, 210, 90);
    lv_obj_align(coord_container, LV_ALIGN_CENTER, 0, 10);
    lv_obj_set_style_bg_color(coord_container, lv_color_hex(0x0a0a0a), 0);
    lv_obj_set_style_border_width(coord_container, 1, 0);
    lv_obj_set_style_border_color(coord_container, lv_color_hex(0x00D4FF), 0);
    lv_obj_set_style_radius(coord_container, 8, 0);
    lv_obj_set_style_pad_all(coord_container, 10, 0);
    
    /* Latitude */
    gps_lat_label = lv_label_create(coord_container);
    lv_label_set_text(gps_lat_label, "Lat: ---.------");
    lv_obj_set_style_text_font(gps_lat_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(gps_lat_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(gps_lat_label, LV_ALIGN_TOP_LEFT, 5, 5);
    
    /* Longitude */
    gps_lon_label = lv_label_create(coord_container);
    lv_label_set_text(gps_lon_label, "Lon: ---.------");
    lv_obj_set_style_text_font(gps_lon_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(gps_lon_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(gps_lon_label, LV_ALIGN_TOP_LEFT, 5, 28);
    
    /* Altitude (from BMP280) */
    gps_alt_label = lv_label_create(coord_container);
    lv_label_set_text(gps_alt_label, "Alt: --- m");
    lv_obj_set_style_text_font(gps_alt_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(gps_alt_label, lv_color_hex(0x88DDFF), 0);
    lv_obj_align(gps_alt_label, LV_ALIGN_TOP_LEFT, 5, 51);
    
    /* Hint for GPS accuracy */
    lv_obj_t *hint_accuracy = lv_label_create(screen);
    lv_label_set_text(hint_accuracy, "Cold start: 30-60s");
    lv_obj_set_style_text_font(hint_accuracy, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(hint_accuracy, lv_color_hex(0x555555), 0);
    lv_obj_align(hint_accuracy, LV_ALIGN_BOTTOM_MID, 0, -25);
    
    /* Swipe hint */
    lv_obj_t *hint = lv_label_create(screen);
    lv_label_set_text(hint, LV_SYMBOL_UP " " LV_SYMBOL_DOWN " Swipe");
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(hint, lv_color_hex(0x444444), 0);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -5);
    
    return screen;
}

/**
 * @brief Timer button event handlers
 */
static void timer_start_button_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        // Debounce check
        unsigned long now = millis();
        if (now - timer_last_button_press < TIMER_BUTTON_DEBOUNCE) {
            return;  // Ignore rapid clicks
        }
        timer_last_button_press = now;
        
        if (!timer_running) {
            // Get time from rollers
            timer_minutes = lv_roller_get_selected(timer_minute_roller);
            timer_seconds = lv_roller_get_selected(timer_second_roller);
            timer_duration_seconds = timer_minutes * 60 + timer_seconds;
            
            // Start timer
            if (timer_duration_seconds > 0) {
                timer_running = true;
                timer_start_millis = millis();
                lv_label_set_text(lv_obj_get_child(timer_btn_start, 0), "STOP");
                
                // Hide rollers and labels, show countdown display
                lv_obj_add_flag(timer_minute_roller, LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_flag(timer_second_roller, LV_OBJ_FLAG_HIDDEN);
                lv_obj_clear_flag(timer_display_label, LV_OBJ_FLAG_HIDDEN);
                
                Serial.printf("[Timer] Started: %d:%02d\n", timer_minutes, timer_seconds);
            }
        } else {
            // Stop timer
            timer_running = false;
            lv_label_set_text(lv_obj_get_child(timer_btn_start, 0), "START");
            
            // Show rollers, hide countdown display
            lv_obj_clear_flag(timer_minute_roller, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(timer_second_roller, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(timer_display_label, LV_OBJ_FLAG_HIDDEN);
            
            Serial.println("[Timer] Stopped");
        }
    }
}

static void timer_reset_button_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        // Debounce check
        unsigned long now = millis();
        if (now - timer_last_button_press < TIMER_BUTTON_DEBOUNCE) {
            return;  // Ignore rapid clicks
        }
        timer_last_button_press = now;
        
        timer_running = false;
        timer_finished = false;
        timer_minutes = timer_duration_seconds / 60;
        timer_seconds = timer_duration_seconds % 60;
        vibration_off();
        
        // Reset rollers to initial duration
        lv_roller_set_selected(timer_minute_roller, timer_minutes, LV_ANIM_OFF);
        lv_roller_set_selected(timer_second_roller, timer_seconds, LV_ANIM_OFF);
        
        // Show rollers, hide countdown display
        lv_obj_clear_flag(timer_minute_roller, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(timer_second_roller, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(timer_display_label, LV_OBJ_FLAG_HIDDEN);
        
        lv_label_set_text(lv_obj_get_child(timer_btn_start, 0), "START");
        Serial.println("[Timer] Reset");
    }
}

/**
 * @brief Create Timer screen
 */
static lv_obj_t* create_timer_screen() {
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_grad_color(screen, lv_color_hex(0x0a0a0a), 0);
    lv_obj_set_style_bg_grad_dir(screen, LV_GRAD_DIR_VER, 0);
    
    /* Title */
    lv_obj_t *title = lv_label_create(screen);
    lv_label_set_text(title, "TIMER");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xAA66FF), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 15);
    
    /* Timer display (large) - only shown when running */
    timer_display_label = lv_label_create(screen);
    lv_label_set_text(timer_display_label, "0:00");
    lv_obj_set_style_text_font(timer_display_label, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(timer_display_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(timer_display_label, LV_ALIGN_CENTER, 0, -10);
    lv_obj_add_flag(timer_display_label, LV_OBJ_FLAG_HIDDEN);  // Hidden by default
    
    /* Minute roller */
    timer_minute_roller = lv_roller_create(screen);
    lv_roller_set_options(timer_minute_roller,
        "00\n01\n02\n03\n04\n05\n06\n07\n08\n09\n"
        "10\n11\n12\n13\n14\n15\n16\n17\n18\n19\n"
        "20\n21\n22\n23\n24\n25\n26\n27\n28\n29\n"
        "30\n31\n32\n33\n34\n35\n36\n37\n38\n39\n"
        "40\n41\n42\n43\n44\n45\n46\n47\n48\n49\n"
        "50\n51\n52\n53\n54\n55\n56\n57\n58\n59",
        LV_ROLLER_MODE_NORMAL);
    lv_roller_set_visible_row_count(timer_minute_roller, 3);
    lv_obj_set_width(timer_minute_roller, 70);
    lv_obj_align(timer_minute_roller, LV_ALIGN_CENTER, -45, -15);
    lv_obj_set_style_bg_color(timer_minute_roller, lv_color_hex(0x1a1a1a), 0);
    lv_obj_set_style_text_color(timer_minute_roller, lv_color_hex(0xFFFFFF), LV_PART_SELECTED);
    lv_obj_set_style_bg_color(timer_minute_roller, lv_color_hex(0x3a3a3a), LV_PART_SELECTED);
    
    /* Minute label */
    lv_obj_t *min_label = lv_label_create(screen);
    lv_label_set_text(min_label, "min");
    lv_obj_set_style_text_font(min_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(min_label, lv_color_hex(0x888888), 0);
    lv_obj_align(min_label, LV_ALIGN_CENTER, -45, 45);
    
    /* Second roller */
    timer_second_roller = lv_roller_create(screen);
    lv_roller_set_options(timer_second_roller,
        "00\n01\n02\n03\n04\n05\n06\n07\n08\n09\n"
        "10\n11\n12\n13\n14\n15\n16\n17\n18\n19\n"
        "20\n21\n22\n23\n24\n25\n26\n27\n28\n29\n"
        "30\n31\n32\n33\n34\n35\n36\n37\n38\n39\n"
        "40\n41\n42\n43\n44\n45\n46\n47\n48\n49\n"
        "50\n51\n52\n53\n54\n55\n56\n57\n58\n59",
        LV_ROLLER_MODE_NORMAL);
    lv_roller_set_visible_row_count(timer_second_roller, 3);
    lv_obj_set_width(timer_second_roller, 70);
    lv_obj_align(timer_second_roller, LV_ALIGN_CENTER, 45, -15);
    lv_obj_set_style_bg_color(timer_second_roller, lv_color_hex(0x1a1a1a), 0);
    lv_obj_set_style_text_color(timer_second_roller, lv_color_hex(0xFFFFFF), LV_PART_SELECTED);
    lv_obj_set_style_bg_color(timer_second_roller, lv_color_hex(0x3a3a3a), LV_PART_SELECTED);
    
    /* Second label */
    lv_obj_t *sec_label = lv_label_create(screen);
    lv_label_set_text(sec_label, "sec");
    lv_obj_set_style_text_font(sec_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(sec_label, lv_color_hex(0x888888), 0);
    lv_obj_align(sec_label, LV_ALIGN_CENTER, 45, 45);
    
    /* Start/Stop button - smaller, muted colors */
    timer_btn_start = lv_btn_create(screen);
    lv_obj_set_size(timer_btn_start, 85, 38);
    lv_obj_align(timer_btn_start, LV_ALIGN_BOTTOM_MID, -50, -40);
    lv_obj_set_style_bg_color(timer_btn_start, lv_color_hex(0x2a5a2a), 0);  // Dark green
    lv_obj_add_event_cb(timer_btn_start, timer_start_button_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *label_start = lv_label_create(timer_btn_start);
    lv_label_set_text(label_start, "START");
    lv_obj_set_style_text_font(label_start, &lv_font_montserrat_12, 0);
    lv_obj_center(label_start);
    
    /* Reset button - smaller, muted colors */
    timer_btn_reset = lv_btn_create(screen);
    lv_obj_set_size(timer_btn_reset, 85, 38);
    lv_obj_align(timer_btn_reset, LV_ALIGN_BOTTOM_MID, 50, -40);
    lv_obj_set_style_bg_color(timer_btn_reset, lv_color_hex(0x5a2a2a), 0);  // Dark red
    lv_obj_add_event_cb(timer_btn_reset, timer_reset_button_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *label_reset = lv_label_create(timer_btn_reset);
    lv_label_set_text(label_reset, "RESET");
    lv_obj_set_style_text_font(label_reset, &lv_font_montserrat_12, 0);
    lv_obj_center(label_reset);
    
    /* Swipe hint */
    lv_obj_t *hint = lv_label_create(screen);
    lv_label_set_text(hint, LV_SYMBOL_UP " " LV_SYMBOL_DOWN " Swipe");
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(hint, lv_color_hex(0x444444), 0);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -5);
    
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
 * @brief Event handler for Sensor Streaming button in More Settings
 */
static void sensor_streaming_button_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        /* Debouncing - prevent rapid toggling */
        static unsigned long lastToggleTime = 0;
        unsigned long now = millis();
        if (now - lastToggleTime < 300) {  // 300ms debounce
            return;
        }
        lastToggleTime = now;
        
        /* Toggle sensor streaming */
        bool current_state = sensor_handler_is_streaming();
        sensor_handler_set_streaming(!current_state);
        
        /* Update button appearance */
        lv_obj_t *btn = lv_event_get_target(e);
        if (!current_state) {
            /* Streaming enabled - change to green */
            lv_obj_set_style_bg_color(btn, lv_color_hex(0x00CC66), 0);
            lv_obj_set_style_bg_color(btn, lv_color_hex(0x00AA55), LV_STATE_PRESSED);
            Serial.println("[AppManager] Sensor Streaming ENABLED");
        } else {
            /* Streaming disabled - change to gray */
            lv_obj_set_style_bg_color(btn, lv_color_hex(0x2a2a2a), 0);
            lv_obj_set_style_bg_color(btn, lv_color_hex(0x3a3a3a), LV_STATE_PRESSED);
            Serial.println("[AppManager] Sensor Streaming DISABLED");
        }
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
    lv_obj_align(set_time_btn, LV_ALIGN_CENTER, 0, -35);
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
    
    /* Sensor Streaming button */
    sensor_streaming_btn = lv_btn_create(screen);
    lv_obj_set_size(sensor_streaming_btn, 180, 50);
    lv_obj_align(sensor_streaming_btn, LV_ALIGN_CENTER, 0, 25);
    
    /* Set initial color based on streaming state */
    bool streaming_state = sensor_handler_is_streaming();
    if (streaming_state) {
        lv_obj_set_style_bg_color(sensor_streaming_btn, lv_color_hex(0x00CC66), 0);
        lv_obj_set_style_bg_color(sensor_streaming_btn, lv_color_hex(0x00AA55), LV_STATE_PRESSED);
    } else {
        lv_obj_set_style_bg_color(sensor_streaming_btn, lv_color_hex(0x2a2a2a), 0);
        lv_obj_set_style_bg_color(sensor_streaming_btn, lv_color_hex(0x3a3a3a), LV_STATE_PRESSED);
    }
    
    lv_obj_set_style_radius(sensor_streaming_btn, 12, 0);
    lv_obj_set_style_border_width(sensor_streaming_btn, 0, 0);
    lv_obj_add_event_cb(sensor_streaming_btn, sensor_streaming_button_event_cb, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *sensor_label = lv_label_create(sensor_streaming_btn);
    lv_label_set_text(sensor_label, "Sensor Streaming");
    lv_obj_set_style_text_font(sensor_label, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(sensor_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(sensor_label);
    
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
            } else if (current_app == AppType::WEATHER) {
                /* From weather -> GPS */
                if (!gps_screen) {
                    gps_screen = create_gps_screen();
                }
                load_screen_with_animation(gps_screen, LV_SCR_LOAD_ANIM_MOVE_TOP);
                current_app = AppType::GPS;
            } else if (current_app == AppType::GPS) {
                /* From GPS -> Timer */
                if (!timer_screen) {
                    timer_screen = create_timer_screen();
                }
                load_screen_with_animation(timer_screen, LV_SCR_LOAD_ANIM_MOVE_TOP);
                current_app = AppType::TIMER;
            } else if (current_app == AppType::TIMER) {
                /* From Timer -> back to watch face */
                load_screen_with_animation(watch_face_get_screen(), LV_SCR_LOAD_ANIM_MOVE_TOP);
                current_app = AppType::WATCH_FACE;
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
            } else if (current_app == AppType::GPS) {
                /* From GPS -> back to weather */
                if (!weather_screen) {
                    weather_screen = create_weather_screen();
                }
                load_screen_with_animation(weather_screen, LV_SCR_LOAD_ANIM_MOVE_BOTTOM);
                current_app = AppType::WEATHER;
            } else if (current_app == AppType::TIMER) {
                /* From Timer -> back to GPS */
                if (!gps_screen) {
                    gps_screen = create_gps_screen();
                }
                load_screen_with_animation(gps_screen, LV_SCR_LOAD_ANIM_MOVE_BOTTOM);
                current_app = AppType::GPS;
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
            } else if (current_app == AppType::FITNESS) {
                /* Swipe left from fitness -> Heart Rate */
                if (!heart_rate_screen) {
                    heart_rate_screen = create_heart_rate_screen();
                }
                load_screen_with_animation(heart_rate_screen, LV_SCR_LOAD_ANIM_MOVE_LEFT);
                current_app = AppType::HEART_RATE;
            } else if (current_app == AppType::WEATHER) {
                /* Swipe left from weather -> Compass */
                if (!compass_screen) {
                    compass_screen = create_compass_screen();
                }
                load_screen_with_animation(compass_screen, LV_SCR_LOAD_ANIM_MOVE_LEFT);
                current_app = AppType::COMPASS;
            }
            break;
            
        case SwipeDirection::RIGHT:
            Serial.println("RIGHT");
            /* Swipe right - no action from watch face anymore */
            if (current_app == AppType::HEART_RATE) {
                /* Swipe right from heart rate -> back to fitness */
                if (!fitness_screen) {
                    fitness_screen = create_fitness_screen();
                }
                load_screen_with_animation(fitness_screen, LV_SCR_LOAD_ANIM_MOVE_RIGHT);
                current_app = AppType::FITNESS;
            } else if (current_app == AppType::MUSIC) {
                // Don't reinit - just reload existing watch face screen
                load_screen_with_animation(watch_face_get_screen(), LV_SCR_LOAD_ANIM_MOVE_RIGHT);
                current_app = AppType::WATCH_FACE;
            } else if (current_app == AppType::COMPASS) {
                /* Swipe right from compass -> back to weather */
                if (!weather_screen) {
                    weather_screen = create_weather_screen();
                }
                load_screen_with_animation(weather_screen, LV_SCR_LOAD_ANIM_MOVE_RIGHT);
                current_app = AppType::WEATHER;
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
        case AppType::COMPASS:
            if (!compass_screen) compass_screen = create_compass_screen();
            load_screen_with_animation(compass_screen, LV_SCR_LOAD_ANIM_FADE_IN);
            break;
        case AppType::GPS:
            if (!gps_screen) gps_screen = create_gps_screen();
            load_screen_with_animation(gps_screen, LV_SCR_LOAD_ANIM_FADE_IN);
            break;
        case AppType::TIMER:
            if (!timer_screen) timer_screen = create_timer_screen();
            load_screen_with_animation(timer_screen, LV_SCR_LOAD_ANIM_FADE_IN);
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
 * @brief Update compass screen with live sensor data
 */
static void update_compass_screen() {
    if (!compass_screen || !compass_yaw_label || !compass_pitch_label || !compass_roll_label) {
        return;
    }
    
    /* Get BNO055 data */
    BNO055Data imu_data = sensor_handler_get_bno055();
    
    if (imu_data.valid) {
        /* Update digital readouts with label prefix */
        static char buf[32];
        
        snprintf(buf, sizeof(buf), "Azm: %.1f°", imu_data.yaw);
        lv_label_set_text(compass_yaw_label, buf);
        
        snprintf(buf, sizeof(buf), "Pitch: %.1f°", imu_data.pitch);
        lv_label_set_text(compass_pitch_label, buf);
        
        snprintf(buf, sizeof(buf), "Roll: %.1f°", imu_data.roll);
        lv_label_set_text(compass_roll_label, buf);
        
        /* Update compass needle rotation */
        if (compass_needle) {
            /* Calculate needle endpoint based on yaw angle */
            /* BNO055 Yaw: 0° = North, 90° = East, 180° = South, 270° = West */
            /* Compass needle points TO North, so invert the yaw (add 180°) */
            float angle_rad = (imu_data.yaw) * 3.14159 / 180.0;  // Convert to radians, invert direction
            int center_x = 65;
            int center_y = 65;
            int needle_length = 42;
            
            int end_x = center_x + (int)(needle_length * cos(angle_rad));
            int end_y = center_y - (int)(needle_length * sin(angle_rad));  // Subtract because Y increases downward
            
            static lv_point_t needle_points[2];
            needle_points[0].x = center_x;
            needle_points[0].y = center_y;
            needle_points[1].x = end_x;
            needle_points[1].y = end_y;
            
            lv_line_set_points(compass_needle, needle_points, 2);
        }
    } else {
        /* No valid data */
        lv_label_set_text(compass_yaw_label, "Azm: ---°");
        lv_label_set_text(compass_pitch_label, "Pitch: ---°");
        lv_label_set_text(compass_roll_label, "Roll: ---°");
    }
}

/**
 * @brief Update GPS screen with live sensor data
 */
static void update_gps_screen() {
    if (!gps_screen || !gps_status_label || !gps_lat_label || !gps_lon_label || !gps_alt_label || !gps_sat_label) {
        return;
    }
    
    /* Get GPS data */
    GPSData gps_data = sensor_handler_get_gps();
    bool gps_available = sensor_handler_gps_available();
    
    /* Get altitude from BMP280 */
    BMP280Data bmp_data = sensor_handler_get_bmp280();
    
    static char buf[64];
    
    if (!gps_available) {
        /* GPS not responding */
        lv_label_set_text(gps_status_label, "GPS Not Responding");
        lv_obj_set_style_text_color(gps_status_label, lv_color_hex(0xFF4444), 0);
        lv_label_set_text(gps_sat_label, LV_SYMBOL_GPS " No GPS");
        lv_obj_set_style_text_color(gps_sat_label, lv_color_hex(0xFF4444), 0);
        
        lv_label_set_text(gps_lat_label, "Lat: ---.------");
        lv_label_set_text(gps_lon_label, "Lon: ---.------");
    } else if (gps_data.valid) {
        /* GPS lock acquired - show coordinates */
        lv_label_set_text(gps_status_label, "GPS Locked");
        lv_obj_set_style_text_color(gps_status_label, lv_color_hex(0x00FF88), 0);
        
        snprintf(buf, sizeof(buf), LV_SYMBOL_GPS " %d sats", gps_data.satellites);
        lv_label_set_text(gps_sat_label, buf);
        lv_obj_set_style_text_color(gps_sat_label, lv_color_hex(0x00FF88), 0);
        
        /* Display coordinates with 6 decimal places for precision */
        snprintf(buf, sizeof(buf), "Lat: %.6f", gps_data.latitude);
        lv_label_set_text(gps_lat_label, buf);
        
        snprintf(buf, sizeof(buf), "Lon: %.6f", gps_data.longitude);
        lv_label_set_text(gps_lon_label, buf);
    } else {
        /* GPS is receiving data but no fix yet */
        lv_label_set_text(gps_status_label, "Searching...");
        lv_obj_set_style_text_color(gps_status_label, lv_color_hex(0xFFAA00), 0);
        
        snprintf(buf, sizeof(buf), LV_SYMBOL_GPS " %d sats", gps_data.satellites);
        lv_label_set_text(gps_sat_label, buf);
        lv_obj_set_style_text_color(gps_sat_label, lv_color_hex(0xFFAA00), 0);
        
        lv_label_set_text(gps_lat_label, "Lat: ---.------");
        lv_label_set_text(gps_lon_label, "Lon: ---.------");
    }
    
    /* Update altitude from BMP280 altimeter */
    /* Check if BMP280 sensor is available */
    bool bmp_available = sensor_handler_bmp280_available();
    
    if (bmp_available) {
        if (bmp_data.valid && bmp_data.pressure > 300.0 && bmp_data.pressure < 1100.0) {
            /* Valid pressure range - calculate altitude */
            /* Sea level pressure = 1013.25 hPa */
            float pressure_ratio = bmp_data.pressure / 1013.25;
            float altitude = 44330.0 * (1.0 - pow(pressure_ratio, 0.1903));
            snprintf(buf, sizeof(buf), "Alt: %.1f m", altitude);
            lv_label_set_text(gps_alt_label, buf);
        } else {
            /* Sensor responding but pressure invalid - show actual value for debugging */
            snprintf(buf, sizeof(buf), "Bad: %.0f hPa", bmp_data.pressure);
            lv_label_set_text(gps_alt_label, buf);
        }
    } else {
        /* Sensor not available */
        lv_label_set_text(gps_alt_label, "Alt: No sensor");
    }
}

/**
 * @brief Update timer screen
 */
static void update_timer_screen() {
    if (!timer_screen || !timer_display_label) {
        return;
    }
    
    static char buf[16];
    
    if (timer_running) {
        // Calculate remaining time
        unsigned long elapsed = millis() - timer_start_millis;
        int remaining_seconds = timer_duration_seconds - (elapsed / 1000);
        
        if (remaining_seconds < 0) {
            remaining_seconds = 0;
        }
        
        timer_minutes = remaining_seconds / 60;
        timer_seconds = remaining_seconds % 60;
        
        // Check if timer finished
        if (remaining_seconds == 0 && !timer_finished) {
            timer_running = false;
            timer_finished = true;
            lv_label_set_text(lv_obj_get_child(timer_btn_start, 0), "START");
            
            // Start 2-second vibration
            vibration_pulse(2000);
            Serial.println("[Timer] Finished! Vibrating for 2 seconds...");
        }
    }
    
    // Update display
    snprintf(buf, sizeof(buf), "%d:%02d", timer_minutes, timer_seconds);
    lv_label_set_text(timer_display_label, buf);
    
    // Change color when running or finished
    if (timer_running) {
        lv_obj_set_style_text_color(timer_display_label, lv_color_hex(0x00FF88), 0);
    } else if (timer_finished && timer_minutes == 0 && timer_seconds == 0) {
        // Flash red when finished
        lv_obj_set_style_text_color(timer_display_label, lv_color_hex(0xFF0000), 0);
    } else {
        lv_obj_set_style_text_color(timer_display_label, lv_color_hex(0xFFFFFF), 0);
    }
    
    // Note: watch face timer is updated in app_manager_update() regardless of current screen
}

/**
 * @brief Update fitness screen with step count
 */
static void update_fitness_screen() {
    if (!fitness_steps_arc || !fitness_steps_label) {
        return;
    }
    
    /* Get current step count */
    int steps = sensor_handler_get_steps();
    
    /* Update arc progress (out of 10,000 goal) */
    lv_arc_set_value(fitness_steps_arc, steps > 10000 ? 10000 : steps);
    
    /* Update step count label with formatting */
    static char steps_buf[16];
    if (steps >= 1000) {
        snprintf(steps_buf, sizeof(steps_buf), "%d,%03d", steps / 1000, steps % 1000);
    } else {
        snprintf(steps_buf, sizeof(steps_buf), "%d", steps);
    }
    lv_label_set_text(fitness_steps_label, steps_buf);
    
    /* Update watch face steps display */
    watch_face_set_steps(steps);
}

/**
 * @brief Update heart rate screen with live MAX30102 data
 */
static void update_heart_rate_screen() {
    if (!heart_rate_screen || !hr_bpm_label || !hr_spo2_label || !hr_status_label) {
        return;
    }
    
    /* Get MAX30102 data */
    MAX30102Data hr_data = sensor_handler_get_max30102();
    bool max_available = sensor_handler_max30102_available();
    
    static char buf[32];
    
    if (!max_available) {
        /* Sensor not detected - clean minimal message */
        lv_label_set_text(hr_bpm_label, "---");
        lv_label_set_text(hr_spo2_label, "---");
        lv_label_set_text(hr_status_label, "No sensor");
        lv_obj_set_style_text_color(hr_status_label, lv_color_hex(0x666666), 0);
    } else if (hr_data.valid) {
        /* Valid heart rate and SpO2 data */
        snprintf(buf, sizeof(buf), "%d BPM", hr_data.heartRate);
        lv_label_set_text(hr_bpm_label, buf);
        
        snprintf(buf, sizeof(buf), "%d%%", hr_data.spo2);
        lv_label_set_text(hr_spo2_label, buf);
        
        /* Status message based on values */
        if (hr_data.heartRate > 100) {
            lv_label_set_text(hr_status_label, "Elevated");
            lv_obj_set_style_text_color(hr_status_label, lv_color_hex(0xFFAA00), 0);
        } else if (hr_data.heartRate < 60) {
            lv_label_set_text(hr_status_label, "Low");
            lv_obj_set_style_text_color(hr_status_label, lv_color_hex(0x4488FF), 0);
        } else {
            lv_label_set_text(hr_status_label, "Normal");
            lv_obj_set_style_text_color(hr_status_label, lv_color_hex(0x00FF88), 0);
        }
    } else {
        /* Sensor available but no valid reading */
        lv_label_set_text(hr_bpm_label, "---");
        lv_label_set_text(hr_spo2_label, "---");
        lv_label_set_text(hr_status_label, "Place finger");
        lv_obj_set_style_text_color(hr_status_label, lv_color_hex(0x888888), 0);
    }
}

/**
 * @brief Update weather screen with live data
 */
static void update_weather_screen() {
    if (!weather_screen || !weather_temp_label || !weather_condition_label || 
        !weather_location_label || !weather_icon_label) {
        return;
    }
    
    static char buf[64];
    unsigned long now = millis();
    
    /* Check connection mode */
    ConnectionMode mode = ble_handler_get_mode();
    
    if (mode == ConnectionMode::WIFI) {
        /* WiFi mode - fetch weather data if needed (every 10 minutes or if invalid) */
        if (!weather_data.valid || (now - weather_data.last_update) > WEATHER_UPDATE_INTERVAL) {
            /* Only fetch if we haven't tried recently (prevent spam on failure) */
            static unsigned long last_attempt = 0;
            if (now - last_attempt > 60000) {  // Try max once per minute
                last_attempt = now;
                fetch_weather_data();
            }
        }
    }
    
    /* Update UI with current weather data */
    if (weather_data.valid) {
        /* Temperature in Fahrenheit */
        snprintf(buf, sizeof(buf), "%.0f°F", weather_data.temp);
        lv_label_set_text(weather_temp_label, buf);
        
        /* Condition */
        lv_label_set_text(weather_condition_label, weather_data.condition.c_str());
        
        /* Location */
        lv_label_set_text(weather_location_label, weather_data.location.c_str());
        
        /* Icon */
        lv_label_set_text(weather_icon_label, weather_data.icon_text.c_str());
        
        /* Update watch face weather as well */
        watch_face_set_weather(weather_data.temp, weather_data.icon_text.c_str());
    } else {
        /* No valid data */
        ConnectionMode mode = ble_handler_get_mode();
        if (mode == ConnectionMode::BLE) {
            lv_label_set_text(weather_temp_label, "--°");
            lv_label_set_text(weather_condition_label, "Weather needs WiFi");
            lv_label_set_text(weather_location_label, "Switch to WiFi mode");
            lv_label_set_text(weather_icon_label, "?");
        } else {
            if (WiFi.status() != WL_CONNECTED) {
                lv_label_set_text(weather_temp_label, "--°");
                lv_label_set_text(weather_condition_label, "No WiFi");
                lv_label_set_text(weather_icon_label, "?");
            } else {
                lv_label_set_text(weather_temp_label, "--°");
                lv_label_set_text(weather_condition_label, "Loading...");
                lv_label_set_text(weather_icon_label, "?");
            }
        }
    }
}

/**
 * @brief Update current app
 */
void app_manager_update() {
    /* Update watch face if it's current */
    if (current_app == AppType::WATCH_FACE) {
        watch_face_update();
    }
    
    /* Update compass screen if it's current */
    if (current_app == AppType::COMPASS) {
        update_compass_screen();
    }
    
    /* Update GPS screen if it's current */
    if (current_app == AppType::GPS) {
        update_gps_screen();
    }
    
    /* Update timer screen if it's current */
    if (current_app == AppType::TIMER) {
        update_timer_screen();
    }
    
    /* Always update timer state on watch face (even when not on timer screen) */
    if (timer_running || (timer_minutes > 0 || timer_seconds > 0)) {
        // Calculate current time if running
        if (timer_running) {
            unsigned long elapsed = millis() - timer_start_millis;
            int remaining_seconds = timer_duration_seconds - (elapsed / 1000);
            if (remaining_seconds < 0) remaining_seconds = 0;
            int current_minutes = remaining_seconds / 60;
            int current_seconds = remaining_seconds % 60;
            watch_face_set_timer(current_minutes, current_seconds, timer_running);
        } else {
            // Not running, just show current set time
            watch_face_set_timer(timer_minutes, timer_seconds, false);
        }
    } else {
        // No timer set, hide on watch face
        watch_face_set_timer(0, 0, false);
    }
    
    /* Always update vibration motor (for timer alerts even when not on timer screen) */
    vibration_update();
    
    /* Update heart rate screen if it's current */
    if (current_app == AppType::HEART_RATE) {
        update_heart_rate_screen();
    }
    
    /* Update weather screen if it's current */
    if (current_app == AppType::WEATHER) {
        update_weather_screen();
    }
    
    /* Update fitness screen if it's current */
    if (current_app == AppType::FITNESS) {
        update_fitness_screen();
    }
    
    /* Add updates for other apps here as needed */
}

