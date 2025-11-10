#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <SPI.h>
#include <Adafruit_GC9A01A.h>
#include <lvgl.h>
#include "secrets.h"
#include "lvgl_display.h"
#include "lvgl_touch.h"
#include "watch_face.h"
#include "button_handler.h"
#include "sleep_manager.h"
#include "wifi_handler.h"

/* SPI and TFT Display */
SPIClass displaySPI(FSPI);
Adafruit_GC9A01A tft = Adafruit_GC9A01A(&displaySPI, TFT_DC_PIN, TFT_CS_PIN, TFT_RST_PIN);

/* Button handlers */
ButtonHandler button(BUTTON_PRIMARY_PIN);
ButtonHandler menuButton(MENU_BUTTON_PIN);

/* Sleep manager */
SleepManager sleepMgr;

/* Time synchronization */
bool timeSynced = false;
unsigned long lastWiFiAttempt = 0;
unsigned long lastNtpAttempt = 0;
unsigned long lastWiFiCheckTime = 0;  // For throttling WiFi operations

// ═══════════════════════════════════════════════════════════════════════
// TIMEZONE CONFIGURATION - Change timezone here
// ═══════════════════════════════════════════════════════════════════════
// Central Standard Time (CST) = UTC-6 (DST ended Nov 2, 2025)
const long TIMEZONE_OFFSET_SEC = -21600;  // -6 hours * 3600 = -21600 seconds
// 
// Other common US timezones:
// CDT (Central Daylight Time) = -18000  (UTC-5)
// EST (Eastern Standard Time) = -18000  (UTC-5)  
// EDT (Eastern Daylight Time) = -14400  (UTC-4)
// PST (Pacific Standard Time) = -28800  (UTC-8)
// PDT (Pacific Daylight Time) = -25200  (UTC-7)
// ═══════════════════════════════════════════════════════════════════════

/* Timezone configuration for Central Time Zone */
long gmtOffsetSec = TIMEZONE_OFFSET_SEC;
int daylightOffsetSec = 0;      // Not using DST offset

/* Update timers */
unsigned long lastWatchFaceUpdate = 0;
const unsigned long WATCH_FACE_UPDATE_INTERVAL = 1000;  // Update every second

void setup() {
    Serial.begin(115200);
    while(!Serial && millis() < 1500) { }
    
    Serial.println();
    Serial.println(F("  ___                  __        __    _       _     "));
    Serial.println(F(" / _ \\ _ __   ___ _ _\\ \\      / /_ _| |_ ___| |__  "));
    Serial.println(F("| | | | '_ \\ / _ \\ '_ \\ \\ /\\ / / _` | __/ __| '_ \\ "));
    Serial.println(F("| |_| | |_) |  __/ | | \\ V  V / (_| | || (__| | | |"));
    Serial.println(F(" \\___/| .__/ \\___|_| |_|\\_/\\_/ \\__,_|\\__\\___|_| |_|"));
    Serial.println(F("      |_|                                           "));
    Serial.println();
    Serial.println(F("=== OpenWatch - LVGL Edition ==="));
    Serial.println(F("Starting up..."));

    /* Set GPIO 17 to HIGH (power rail) */
    pinMode(17, OUTPUT);
    digitalWrite(17, HIGH);
    Serial.println(F("GPIO 17 set to HIGH"));

    /* CPU frequency check */
    Serial.printf("CPU Frequency: %d MHz\n", getCpuFrequencyMhz());

    /* Initialize SPI bus for display */
    Serial.println(F("Initializing SPI bus for display..."));
    displaySPI.begin(TFT_SCK_PIN, TFT_MISO_PIN, TFT_MOSI_PIN, TFT_CS_PIN);
    Serial.println(F("SPI bus initialized"));

    /* Initialize display with LVGL */
    Serial.println(F("Initializing LVGL display..."));
    lvgl_display_init(&tft);
    Serial.println(F("LVGL display initialized"));
    
    /* Show splash screen (temporary) */
    Serial.println(F("Creating splash screen..."));
    lv_obj_t *splash_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(splash_screen, lv_color_hex(0x000000), 0);
    
    Serial.println(F("Creating splash label..."));
    lv_obj_t *splash_label = lv_label_create(splash_screen);
    lv_label_set_text(splash_label, "OpenWatch");
    lv_obj_set_style_text_font(splash_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(splash_label, lv_color_hex(0x00d9ff), 0);
    lv_obj_center(splash_label);
    
    Serial.println(F("Loading splash screen..."));
    lv_scr_load(splash_screen);
    
    Serial.println(F("Running LVGL timer handler..."));
    lv_timer_handler();
    delay(1000);
    
    Serial.println(F("Splash screen shown, will be replaced by watch face"));
    Serial.flush();

    /* Initialize I2C for sensors (touch controller, etc.) */
    Serial.println(F("Initializing I2C..."));
    Wire.begin(TOUCH_SDA_PIN, TOUCH_SCL_PIN);  // SDA=8, SCL=9 (shared with sensors)
    Serial.printf("I2C initialized on SDA=%d, SCL=%d\n", TOUCH_SDA_PIN, TOUCH_SCL_PIN);
    
    /* Initialize touch input */
    Serial.println(F("Initializing touch input..."));
    Serial.flush();
    lvgl_touch_init();
    Serial.println(F("Touch input initialized"));
    Serial.flush();

    /* Auto-connect WiFi for time sync */
    Serial.println(F("Connecting to WiFi for time sync..."));
    Serial.flush();
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    
    /* Wait up to 10 seconds for WiFi connection */
    int wifi_retry = 0;
    while (WiFi.status() != WL_CONNECTED && wifi_retry < 20) {
        delay(500);
        Serial.print(".");
        wifi_retry++;
    }
    Serial.println();
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println(F("WiFi connected!"));
        Serial.printf("IP: %s\n", WiFi.localIP().toString().c_str());
        
        /* Sync time via NTP */
        Serial.println(F("Syncing time with NTP..."));
        configTime(gmtOffsetSec, daylightOffsetSec, "pool.ntp.org", "time.nist.gov");
        
        /* Wait for time sync */
        int ntp_retry = 0;
        while (time(nullptr) < 100000 && ntp_retry < 10) {
            delay(500);
            Serial.print(".");
            ntp_retry++;
        }
        Serial.println();
        
        if (time(nullptr) > 100000) {
            timeSynced = true;
            Serial.println(F("Time synced successfully!"));
            struct tm timeinfo;
            time_t now = time(nullptr);
            now -= gmtOffsetSec;
            localtime_r(&now, &timeinfo);
            Serial.printf("Current time: %02d:%02d:%02d\n", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
        } else {
            Serial.println(F("NTP sync timeout"));
        }
    } else {
        Serial.println(F("WiFi connection failed"));
    }
    Serial.flush();

    /* Initialize buttons */
    Serial.println(F("Initializing buttons..."));
    Serial.flush();
    button.begin();
    Serial.println(F("Primary button initialized"));
    Serial.flush();
    
    menuButton.begin();
    Serial.println(F("Menu button initialized"));
    Serial.flush();
    
    Serial.printf("Initial button states - Primary (GPIO %d): %d, Menu (GPIO %d): %d\n", 
        BUTTON_PRIMARY_PIN, digitalRead(BUTTON_PRIMARY_PIN), 
        MENU_BUTTON_PIN, digitalRead(MENU_BUTTON_PIN));
    
    /* Configure wake-up for light sleep */
    Serial.println(F("Configuring wake-up for light sleep..."));
    Serial.flush();
    button.enableWakeup();
    menuButton.enableWakeup();
    Serial.println(F("Wake-up configuration complete"));
    Serial.flush();

    /* Initialize watch face - this will replace splash screen */
    Serial.println(F("Creating watch face..."));
    Serial.flush();
    watch_face_init();  // This calls lv_scr_load() internally
    Serial.println(F("Watch face created and loaded"));
    Serial.flush();
    
    watch_face_set_wifi_status(false);
    watch_face_set_time_synced(false);
    Serial.println(F("Watch face status set"));
    Serial.flush();
    
    /* Delete splash screen to free memory */
    Serial.println(F("Deleting splash screen..."));
    lv_obj_del(splash_screen);
    Serial.println(F("Splash screen deleted"));
    Serial.flush();
    
    /* Force immediate update */
    lastWatchFaceUpdate = 0;
    
    Serial.println(F("Forcing immediate display refresh..."));
    Serial.flush();
    
    /* Force LVGL to refresh NOW - this is critical for screen transitions */
    lv_refr_now(NULL);
    Serial.println(F("lv_refr_now() completed"));
    Serial.flush();
    
    Serial.println(F("Running LVGL timer handlers..."));
    Serial.flush();
    lv_timer_handler();
    lv_timer_handler();
    lv_timer_handler();
    Serial.println(F("LVGL timers completed"));
    Serial.flush();
    
    Serial.println(F("Boot complete - Watch face active"));
    Serial.println(F("Check display now - should see watch face"));
    Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());
    Serial.flush();
}

void loop() {
    static bool firstLoop = true;
    if (firstLoop) {
        Serial.println(F("[LOOP] Entered main loop"));
        Serial.flush();
        firstLoop = false;
    }
    
    unsigned long now = millis();
    static unsigned long lastBackgroundTasks = 0;
    
    /* Handle LVGL tasks (most important - do this frequently) */
    lv_timer_handler();
    
    /* ============================================
     * Button handling
     * ============================================ */
    ButtonEvent primaryEvent = button.update();
    ButtonEvent menuEvent = menuButton.update();

    /* Debug button events */
    if (primaryEvent != ButtonEvent::NONE) {
        Serial.printf("[BTN] Primary: %s\n", 
                      primaryEvent == ButtonEvent::SHORT_PRESS ? "SHORT" : "LONG");
    }
    if (menuEvent != ButtonEvent::NONE) {
        Serial.printf("[BTN] Menu: %s\n", 
                      menuEvent == ButtonEvent::SHORT_PRESS ? "SHORT" : "LONG");
    }

    /* Long press primary button -> sleep */
    if (primaryEvent == ButtonEvent::LONG_PRESS) {
        Serial.println(F("[BTN] Long press - initiating sleep"));
        sleepMgr.triggerSleep();
    }
    
    /* Short press menu button -> toggle WiFi (for time sync) */
    if (menuEvent == ButtonEvent::SHORT_PRESS) {
        if (WiFi.status() == WL_CONNECTED) {
            Serial.println(F("[BTN] Disconnecting WiFi"));
            WiFi.disconnect();
            WiFi.mode(WIFI_OFF);
            watch_face_set_wifi_status(false);
        } else {
            Serial.println(F("[BTN] Connecting WiFi..."));
            WiFi.mode(WIFI_STA);
            WiFi.begin(WIFI_SSID, WIFI_PASS);
            lastWiFiAttempt = now;
        }
    }
    
    /* ============================================
     * Sleep handling
     * ============================================ */
    if (sleepMgr.getState() == SleepState::GOING_TO_SLEEP) {
        if (sleepMgr.updateCountdown()) {
            /* Show sleep message */
            lv_obj_t *sleep_screen = lv_obj_create(NULL);
            lv_obj_set_style_bg_color(sleep_screen, lv_color_hex(0x000000), 0);
            lv_scr_load(sleep_screen);
            
            lv_obj_t *sleep_label = lv_label_create(sleep_screen);
            lv_label_set_text(sleep_label, "Sleeping...");
            lv_obj_set_style_text_color(sleep_label, lv_color_hex(0x888888), 0);
            lv_obj_center(sleep_label);
            lv_timer_handler();
            
            /* Sleep */
            sleepMgr.goToSleep(tft);
            
            /* Wake up */
            lastWiFiAttempt = 0;
            
            /* Show wake message */
            lv_obj_t *wake_screen = lv_obj_create(NULL);
            lv_obj_set_style_bg_color(wake_screen, lv_color_hex(0x000000), 0);
            lv_scr_load(wake_screen);
            
            lv_obj_t *wake_label = lv_label_create(wake_screen);
            lv_label_set_text(wake_label, "Waking up...");
            lv_obj_set_style_text_color(wake_label, lv_color_hex(0x00d9ff), 0);
            lv_obj_center(wake_label);
            lv_timer_handler();
            delay(1000);
            
            /* Restore watch face */
            watch_face_init();
            lastWatchFaceUpdate = 0;
            return;
        }
    }

    /* ============================================
     * Background tasks (WiFi, NTP, etc.)
     * ============================================ */
    if (now - lastBackgroundTasks > 1000) {  // Every second
        lastBackgroundTasks = now;
        
        /* WiFi connection handling */
        wl_status_t wifiStatus = WiFi.status();
        bool wifiConnected = (wifiStatus == WL_CONNECTED);
        watch_face_set_wifi_status(wifiConnected);
        
        /* Handle WiFi reconnection */
        handleWiFiReconnection(now, lastWiFiAttempt);
        
        /* Handle NTP time sync */
        handleNTPRetry(now, timeSynced, gmtOffsetSec, daylightOffsetSec, lastNtpAttempt);
        watch_face_set_time_synced(timeSynced);
    }

    /* ============================================
     * Watch face update
     * ============================================ */
    if (now - lastWatchFaceUpdate >= WATCH_FACE_UPDATE_INTERVAL) {
        lastWatchFaceUpdate = now;
        watch_face_update();
        
        /* Force display refresh after watch face update */
        lv_refr_now(NULL);
    }
    
    /* Small delay to prevent tight loop */
    delay(5);
}
