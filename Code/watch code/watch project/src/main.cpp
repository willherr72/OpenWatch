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
#include "app_manager.h"
#include "button_handler.h"
#include "wifi_handler.h"
#include "sensor_handler.h"
#include "ble_handler.h"
#include "vibration.h"

/* SPI and TFT Display */
SPIClass displaySPI(FSPI);
Adafruit_GC9A01A tft = Adafruit_GC9A01A(&displaySPI, TFT_DC_PIN, TFT_CS_PIN, TFT_RST_PIN);

/* Button handlers */
ButtonHandler button(BUTTON_PRIMARY_PIN);        // GPIO18 - Power menu button (3-second hold)
ButtonHandler menuButton(MENU_BUTTON_PIN);      // GPIO14 - Home button (return to watch face)

/* GPIO18 power menu state tracking */
unsigned long gpio18_press_start = 0;
bool gpio18_pressed = false;
bool power_menu_shown = false;

/* Time synchronization */
bool timeSynced = false;
unsigned long lastWiFiAttempt = 0;
unsigned long lastNtpAttempt = 0;
unsigned long lastWiFiCheckTime = 0;  // For throttling WiFi operations

/* Touch retry */
bool touchInitialized = false;
unsigned long lastTouchRetry = 0;
const unsigned long TOUCH_RETRY_INTERVAL = 200;  // Retry every 200ms for quick response

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

/* Battery monitoring configuration */
// BATTERY_ADC_PIN is defined in platformio.ini
const float BATTERY_MAX_VOLTAGE = 3.36;  // 100% battery voltage
const float BATTERY_MIN_VOLTAGE = 2.64;  // 0% battery voltage
const float ADC_VOLTAGE_REF = 3.3;       // ESP32-S3 ADC reference voltage
const int ADC_MAX_VALUE = 4095;          // 12-bit ADC

/**
 * @brief Read battery percentage from ADC pin
 * @return Battery percentage (0-100)
 */
int readBatteryPercentage() {
    // Read ADC value from GPIO1
    int adcValue = analogRead(BATTERY_ADC_PIN);
    
    // Convert ADC value to voltage
    // Using default attenuation (ADC_11db) which supports 0-3.9V range
    float voltage = (adcValue / (float)ADC_MAX_VALUE) * ADC_VOLTAGE_REF;
    
    // Map voltage to percentage (2.64V = 0%, 3.36V = 100%)
    float percentage = ((voltage - BATTERY_MIN_VOLTAGE) / (BATTERY_MAX_VOLTAGE - BATTERY_MIN_VOLTAGE)) * 100.0;
    
    // Clamp percentage to 0-100 range
    if (percentage < 0) percentage = 0;
    if (percentage > 100) percentage = 100;
    
    return (int)percentage;
}

void setup() {
    Serial.begin(115200);
    // No COM port wait - allows immediate streaming for FPGA/direct bus reading
    // Serial will work either way, data is available on the bus immediately
    
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
    Serial.println(F("GPIO 17 set to HIGH (power rail for peripherals)"));
    Serial.println(F("Waiting for peripherals to power up..."));
    delay(200);  // CST816S needs time to boot after power-on
    Serial.println(F("Power rail stabilized"));
    
    /* Configure ADC for battery monitoring */
    analogReadResolution(12);  // 12-bit resolution (0-4095)
    analogSetAttenuation(ADC_11db);  // 0-3.9V range (default for ESP32-S3)
    pinMode(BATTERY_ADC_PIN, INPUT);
    Serial.printf("Battery ADC configured on GPIO%d (12-bit, 0-3.9V range)\n", BATTERY_ADC_PIN);

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
    delay(50);  // Brief delay after display init before I2C operations
    
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
    Wire.setClock(100000);  // 100kHz for better reliability with CST816S
    Wire.setTimeout(100);   // 100ms timeout
    Serial.printf("I2C initialized on SDA=%d, SCL=%d at 100kHz\n", TOUCH_SDA_PIN, TOUCH_SCL_PIN);
    delay(50);  // Let I2C bus stabilize
    
    /* Scan I2C bus to detect devices */
    Serial.println(F("Scanning I2C bus for devices..."));
    int devicesFound = 0;
    for (byte address = 1; address < 127; address++) {
        Wire.beginTransmission(address);
        byte error = Wire.endTransmission();
        if (error == 0) {
            Serial.printf("  ✓ I2C device found at 0x%02X\n", address);
            devicesFound++;
        } else if (error == 4) {
            Serial.printf("  ✗ Unknown error at 0x%02X\n", address);
        }
        delay(10);  // Small delay between scans
    }
    if (devicesFound == 0) {
        Serial.println(F("  ✗ NO I2C devices found!"));
        Serial.println(F("  Check connections: SDA=8, SCL=9, Power=GPIO17"));
    } else {
        Serial.printf("  Found %d I2C device(s)\n", devicesFound);
    }
    Serial.println(F("I2C scan complete"));
    
    /* Initialize sensors (BMP280, BNO055, MAX30102, GPS) */
    Serial.println(F("Initializing sensors..."));
    Serial.flush();
    sensor_handler_init();
    Serial.println(F("Sensor initialization complete"));
    Serial.flush();
    
    /* Initialize touch input */
    Serial.println(F("Initializing touch input..."));
    Serial.flush();
    lvgl_touch_init();
    touchInitialized = lvgl_touch_is_ready();
    if (touchInitialized) {
        Serial.println(F("Touch input initialized successfully"));
    } else {
        Serial.println(F("Touch input failed - will retry in background"));
    }
    Serial.flush();

    /* Ensure WiFi runs on Core 0, leaving Core 1 for LVGL/Touch */
    Serial.println(F("Configuring WiFi to run on Core 0..."));
    WiFi.useStaticBuffers(true);  // Use static buffers for better performance
    
    /* Start WiFi connection in background (non-blocking) */
    Serial.println(F("Starting WiFi connection in background..."));
    Serial.flush();
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    lastWiFiAttempt = millis();
    Serial.println(F("WiFi init started - will connect in background"));
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
    
    /* GPIO14 is MENU_BUTTON_PIN (already initialized above as menuButton) */
    /* GPIO18 is BUTTON_PRIMARY_PIN (already initialized above as button) */
    /* GPIO18 power button functionality added in loop */
    Serial.printf("GPIO14 (Menu/Home) and GPIO18 (Primary/Power) configured\n");
    
    Serial.printf("Initial button states - Primary (GPIO %d): %d, Menu (GPIO %d): %d\n", 
        BUTTON_PRIMARY_PIN, digitalRead(BUTTON_PRIMARY_PIN), 
        MENU_BUTTON_PIN, digitalRead(MENU_BUTTON_PIN));
    
    /* Wake-up configuration removed - sleep functionality not used */
    
    /* Initialize vibration motor */
    Serial.println(F("Initializing vibration motor..."));
    Serial.flush();
    vibration_init();
    Serial.println(F("Vibration motor initialized"));
    Serial.flush();

    /* Initialize watch face - this will replace splash screen */
    Serial.println(F("Creating watch face..."));
    Serial.flush();
    watch_face_init();  // This calls lv_scr_load() internally
    Serial.println(F("Watch face created and loaded"));
    Serial.flush();
    
    watch_face_set_connectivity_status(false, false);  // Start in WiFi mode, disconnected
    watch_face_set_time_synced(false);
    
    /* Read initial battery level from ADC */
    int initialBattery = readBatteryPercentage();
    watch_face_set_battery(initialBattery, false);
    Serial.printf("Initial battery level: %d%%\n", initialBattery);
    
    watch_face_set_steps(0);            // Initial step count (will be updated by sensor)
    watch_face_set_heart_rate(0);       // No heart rate data yet
    Serial.println(F("Watch face status set"));
    Serial.flush();
    
    /* Initialize BLE handler (not started unless mode is switched) */
    Serial.println(F("Initializing BLE handler..."));
    Serial.flush();
    ble_handler_init();
    Serial.println(F("BLE handler initialized (inactive)"));
    Serial.flush();
    
    /* Initialize app manager for multi-screen navigation */
    Serial.println(F("Initializing app manager..."));
    Serial.flush();
    app_manager_init();
    Serial.println(F("App manager initialized"));
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
        Serial.println(F("[LOOP] Entered main loop on Core "));
        Serial.println(xPortGetCoreID());
        Serial.flush();
        firstLoop = false;
    }
    
    unsigned long now = millis();
    static unsigned long lastBackgroundTasks = 0;
    static unsigned long lastLvglCall = 0;
    
    /* Handle LVGL tasks FIRST and FREQUENTLY (critical for touch responsiveness) */
    /* Call every ~5ms for smooth 200Hz polling rate */
    if (now - lastLvglCall >= 5 || lastLvglCall == 0) {
        lv_timer_handler();
        lastLvglCall = now;
    }
    
    /* ============================================
     * Touch retry (if not initialized)
     * ============================================ */
    if (!touchInitialized && (now - lastTouchRetry > TOUCH_RETRY_INTERVAL)) {
        lastTouchRetry = now;
        // Silently retry - only report success
        lvgl_touch_init();
        touchInitialized = lvgl_touch_is_ready();
        if (touchInitialized) {
            Serial.println(F("[Touch] ✓ Touch controller initialized successfully!"));
        }
    }
    
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

    /* Long press primary button -> removed (GPIO18 now used for power menu) */
    // Sleep function removed from GPIO18
    
    /* Menu button (GPIO14) functionality moved above to return to watch face */
    /* WiFi toggle functionality removed to use GPIO14 as home button */
    
    /* ============================================
     * GPIO14 - Home button (return to watch face)
     * Note: GPIO14 is MENU_BUTTON_PIN, so we use the menuButton short press
     * ============================================ */
    if (menuEvent == ButtonEvent::SHORT_PRESS) {
        // GPIO14 short press returns to watch face from any menu
        if (app_manager_get_current() != AppType::WATCH_FACE && app_manager_get_current() != AppType::POWER_MENU) {
            Serial.println(F("[GPIO14] Menu button - returning to watch face"));
            app_manager_return_home();
        }
    }
    
    /* ============================================
     * GPIO18 - Power menu button (3-second hold)
     * Using ButtonHandler for better debouncing
     * ============================================ */
    bool gpio18_current = button.isButtonPressed();
    
    if (gpio18_current && !gpio18_pressed) {
        // Button just pressed
        gpio18_pressed = true;
        gpio18_press_start = now;
        power_menu_shown = false;
        Serial.println(F("[GPIO18] Power button pressed - waiting for 3 seconds..."));
    } else if (!gpio18_current && gpio18_pressed) {
        // Button released
        gpio18_pressed = false;
        power_menu_shown = false;
        Serial.println(F("[GPIO18] Power button released"));
    } else if (gpio18_current && gpio18_pressed && !power_menu_shown && (now - gpio18_press_start >= 3000)) {
        // Held for 3 seconds
        power_menu_shown = true;
        Serial.println(F("[GPIO18] Power button held for 3 seconds - showing power menu"));
        app_manager_navigate_to(AppType::POWER_MENU);
    }
    
    /* ============================================
     * Sensor updates (rate limited internally to 200ms)
     * ============================================ */
    sensor_handler_update();
    
    /* ============================================
     * BLE updates
     * ============================================ */
    ble_handler_update();
    
    /* ============================================
     * Background tasks (WiFi, NTP, BLE time sync, etc.)
     * ============================================ */
    if (now - lastBackgroundTasks > 1000) {  // Every second
        lastBackgroundTasks = now;
        
        /* Check connection mode */
        ConnectionMode mode = ble_handler_get_mode();
        
        /* WiFi connection handling - non-blocking */
        wl_status_t wifiStatus = WiFi.status();
        bool wifiConnected = (wifiStatus == WL_CONNECTED);
        
        if (mode == ConnectionMode::WIFI) {
            /* If WiFi just connected, configure NTP (sets RTC - persists without WiFi!) */
            static bool ntpConfigured = false;
            if (wifiConnected && !ntpConfigured) {
                Serial.println(F("[WiFi] Connected! Configuring NTP..."));
                Serial.printf("IP: %s\n", WiFi.localIP().toString().c_str());
                Serial.println(F("[NTP] Configuring time servers (sets ESP32 RTC)..."));
                configTime(gmtOffsetSec, daylightOffsetSec, "pool.ntp.org", "time.nist.gov");
                ntpConfigured = true;
                lastNtpAttempt = now;
            }
            
            /* Check if time is synced (RTC persists even without WiFi) */
            if (!timeSynced) {
                time_t nowTime = time(nullptr);
                if (nowTime > 100000) {
                    // Time is valid - either just synced or RTC is maintaining it
                    timeSynced = true;
                    struct tm timeinfo;
                    getLocalTime(&timeinfo);
                    
                    if (wifiConnected) {
                        Serial.println(F("[NTP] Time synced successfully from NTP!"));
                    } else {
                        Serial.println(F("[RTC] Time already set in RTC (persists without WiFi)"));
                    }
                    
                    Serial.printf("Current time: %04d-%02d-%02d %02d:%02d:%02d\n", 
                        timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                        timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
                }
            }
            
            /* Handle WiFi reconnection */
            handleWiFiReconnection(now, lastWiFiAttempt);
            
            /* Handle NTP time sync */
            handleNTPRetry(now, timeSynced, gmtOffsetSec, daylightOffsetSec, lastNtpAttempt);
            
        } else {
            /* BLE mode - check for time sync */
            if (ble_handler_time_synced() && !timeSynced) {
                Serial.println(F("[BLE] Time synced via BLE!"));
                timeSynced = true;
            }
        }
        
        /* Update connectivity indicator on watch face */
        if (mode == ConnectionMode::BLE) {
            watch_face_set_connectivity_status(ble_handler_is_connected(), true);
        } else {
            watch_face_set_connectivity_status(wifiConnected, false);
        }
        
        watch_face_set_time_synced(timeSynced);
        
        // Read actual battery level from ADC
        int batteryLevel = readBatteryPercentage();
        watch_face_set_battery(batteryLevel, false);
        
        // Note: Steps and heart rate are updated in watch_face_update() from real sensors
    }

    /* ============================================
     * Watch face and app updates
     * ============================================ */
    if (now - lastWatchFaceUpdate >= WATCH_FACE_UPDATE_INTERVAL) {
        lastWatchFaceUpdate = now;
        
        /* Update current app/screen */
        app_manager_update();
        
        /* Force display refresh after update */
        lv_refr_now(NULL);
    }
    
    /* No delay needed - LVGL handler provides timing control */
    /* This allows maximum touch responsiveness */
}
