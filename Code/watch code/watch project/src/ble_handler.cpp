/**
 * @file ble_handler.cpp
 * @brief BLE connection handler implementation - SERVER MODE
 * Watch advertises, phone/user writes time in flexible formats
 */

#include "ble_handler.h"
#include "wifi_handler.h"
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <WiFi.h>
#include <time.h>

/* BLE UUIDs - Custom time service */
#define SERVICE_UUID_TIME     "12340000-0123-0123-0123-0123456789ab"
#define CHAR_UUID_TIME_SYNC   "12340001-0123-0123-0123-0123456789ab"

/* State variables */
static ConnectionMode current_mode = ConnectionMode::WIFI;
static bool ble_initialized = false;
static bool ble_connected = false;
static bool time_synced_via_ble = false;

/* BLE objects - SERVER mode */
static BLEServer *ble_server = nullptr;
static BLECharacteristic *char_time_sync = nullptr;

/**
 * @brief BLE Server callbacks
 */
class ServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
        ble_connected = true;
        Serial.println("[BLE] ========================================");
        Serial.println("[BLE] ✓ CLIENT CONNECTED!");
        Serial.println("[BLE] ========================================");
    }

    void onDisconnect(BLEServer* pServer) {
        ble_connected = false;
        Serial.println("[BLE] ========================================");
        Serial.println("[BLE] Client disconnected");
        Serial.println("[BLE] Restarting advertising...");
        delay(500);  // Give BLE stack time to clean up
        pServer->startAdvertising();
        Serial.println("[BLE] ✓ ADVERTISING RESTARTED");
        Serial.println("[BLE] ========================================");
    }
};

/**
 * @brief Parse timestamp from multiple formats
 * Accepts: HHMMSS (6 digits), Unix timestamp (10 digits), binary formats
 */
static bool parse_timestamp(const std::string &value, uint32_t &timestamp) {
    size_t len = value.length();
    
    // Check if all characters are digits (for text formats)
    bool all_digits = true;
    for (char c : value) {
        if (!isdigit(c)) {
            all_digits = false;
            break;
        }
    }
    
    // Format 1: HHMMSS (e.g., "143025" for 2:30:25 PM)
    // EASIEST - just look at phone and type 6 digits!
    if (len == 6 && all_digits) {
        int hour = (value[0] - '0') * 10 + (value[1] - '0');
        int minute = (value[2] - '0') * 10 + (value[3] - '0');
        int second = (value[4] - '0') * 10 + (value[5] - '0');
        
        // Validate time
        if (hour >= 0 && hour <= 23 && minute >= 0 && minute <= 59 && second >= 0 && second <= 59) {
            // Get current time to keep the date
            time_t now = time(nullptr);
            struct tm timeinfo;
            localtime_r(&now, &timeinfo);
            
            // Update only the time part
            timeinfo.tm_hour = hour;
            timeinfo.tm_min = minute;
            timeinfo.tm_sec = second;
            
            timestamp = mktime(&timeinfo);
            Serial.printf("[BLE] Format: HHMMSS (%02d:%02d:%02d)\n", hour, minute, second);
            return true;
        } else {
            Serial.printf("[BLE] ERROR: Invalid time values (HH=%d, MM=%d, SS=%d)\n", hour, minute, second);
            return false;
        }
    }
    
    // Format 2: Unix timestamp (e.g., "1731974400")
    if (len >= 10 && len <= 12 && all_digits) {
        timestamp = strtoul(value.c_str(), nullptr, 10);
        
        // Validate reasonable range (2020-2040)
        const uint32_t MIN_VALID = 1577836800;
        const uint32_t MAX_VALID = 2240524800;
        
        if (timestamp >= MIN_VALID && timestamp <= MAX_VALID) {
            Serial.println("[BLE] Format: Unix timestamp");
            return true;
        } else {
            Serial.println("[BLE] ERROR: Unix timestamp out of valid range");
            return false;
        }
    }
    
    // Format 3 & 4: Binary formats (4 bytes)
    if (len == 4) {
        // Try little-endian first (original format)
        uint32_t ts_le = 0;
        memcpy(&ts_le, value.data(), 4);
        
        // Try big-endian (network byte order - more intuitive)
        uint32_t ts_be = __builtin_bswap32(ts_le);
        
        // Unix timestamp should be roughly between year 2020 and 2040
        const uint32_t MIN_VALID = 1577836800;
        const uint32_t MAX_VALID = 2240524800;
        
        if (ts_le >= MIN_VALID && ts_le <= MAX_VALID) {
            timestamp = ts_le;
            Serial.println("[BLE] Format: Binary little-endian");
            return true;
        } else if (ts_be >= MIN_VALID && ts_be <= MAX_VALID) {
            timestamp = ts_be;
            Serial.println("[BLE] Format: Binary big-endian");
            return true;
        }
    }
    
    Serial.printf("[BLE] ERROR: Invalid format (length: %d)\n", len);
    return false;
}

/**
 * @brief Time sync characteristic callback
 * Accepts multiple input formats for ease of use
 */
class TimeSyncCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
        std::string value = pCharacteristic->getValue();
        
        Serial.println("[BLE] ========================================");
        Serial.printf("[BLE] Received time data (%d bytes)\n", value.length());
        
        uint32_t timestamp = 0;
        if (parse_timestamp(value, timestamp)) {
            Serial.printf("[BLE] Parsed timestamp: %u (Unix)\n", timestamp);
            
            // Set system time
            struct timeval tv;
            tv.tv_sec = timestamp;
            tv.tv_usec = 0;
            
            if (settimeofday(&tv, nullptr) == 0) {
                time_synced_via_ble = true;
                
                // Print new time in local timezone
                time_t now = time(nullptr);
                struct tm timeinfo;
                localtime_r(&now, &timeinfo);
                Serial.printf("[BLE] ✓ Time synced successfully!\n");
                Serial.printf("[BLE] New time: %04d-%02d-%02d %02d:%02d:%02d\n",
                             timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                             timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
            } else {
                Serial.println("[BLE] ERROR: Failed to set time!");
            }
        } else {
            Serial.println("[BLE] Supported formats:");
            Serial.println("[BLE]   - HHMMSS: \"143025\" for 2:30:25 PM (easiest!)");
            Serial.println("[BLE]   - Unix timestamp: \"1731974400\"");
            Serial.println("[BLE]   - Binary little/big-endian (4 bytes)");
        }
        
        Serial.println("[BLE] ========================================");
    }
};

/**
 * @brief Initialize BLE handler
 */
void ble_handler_init() {
    Serial.println("[BLE] Initializing BLE handler...");
    
    // BLE will be started only when mode is switched to BLE
    ble_initialized = true;
    
    Serial.println("[BLE] Handler initialized (not started)");
}

/**
 * @brief Start BLE advertising and services
 */
void ble_handler_start() {
    if (!ble_initialized) {
        Serial.println("[BLE] ERROR: Not initialized!");
        return;
    }
    
    // If already started, just restart advertising
    if (ble_server != nullptr) {
        Serial.println("[BLE] BLE already active - restarting advertising...");
        BLEDevice::startAdvertising();
        Serial.println("[BLE] ========================================");
        Serial.println("[BLE] ✓ ADVERTISING RESTARTED AS 'OpenWatch'");
        Serial.println("[BLE] ========================================");
        return;
    }
    
    Serial.println("[BLE] Starting BLE stack (first time)...");
    
    // Initialize BLE - ONLY ONCE
    BLEDevice::init("OpenWatch");
    Serial.println("[BLE] BLE Device initialized");
    
    // Create BLE Server
    ble_server = BLEDevice::createServer();
    ble_server->setCallbacks(new ServerCallbacks());
    Serial.println("[BLE] BLE Server created");
    
    /* Create Time Service */
    BLEService *time_service = ble_server->createService(SERVICE_UUID_TIME);
    
    char_time_sync = time_service->createCharacteristic(
        CHAR_UUID_TIME_SYNC,
        BLECharacteristic::PROPERTY_WRITE
    );
    char_time_sync->setCallbacks(new TimeSyncCallbacks());
    
    time_service->start();
    Serial.println("[BLE] Time service started");
    
    /* Future: Add notification service here when vibration motor is implemented */
    
    /* Start advertising */
    BLEAdvertising *advertising = BLEDevice::getAdvertising();
    advertising->addServiceUUID(SERVICE_UUID_TIME);
    advertising->setScanResponse(true);
    advertising->setMinPreferred(0x06);  // functions that help with iPhone connections issue
    advertising->setMinPreferred(0x12);
    BLEDevice::startAdvertising();
    
    Serial.println("[BLE] ========================================");
    Serial.println("[BLE] ✓ ADVERTISING AS 'OpenWatch'");
    Serial.println("[BLE] ========================================");
    Serial.println("[BLE] Ready for phone connection!");
    Serial.println("[BLE] ");
    Serial.println("[BLE] === HOW TO SET TIME ===");
    Serial.println("[BLE] 1. Open NRF Connect app");
    Serial.println("[BLE] 2. Connect to 'OpenWatch'");
    Serial.println("[BLE] 3. Find characteristic: 12340001-...");
    Serial.println("[BLE] 4. Write time as TEXT (UTF-8)");
    Serial.println("[BLE] ");
    Serial.println("[BLE] EASIEST: Type HHMMSS (e.g., \"143025\" for 2:30:25 PM)");
    Serial.println("[BLE] Just look at your phone's time and type 6 digits!");
    Serial.println("[BLE] ");
    Serial.println("[BLE] Advanced: Full Unix timestamp also supported");
    Serial.println("[BLE] ========================================");
}

/**
 * @brief Stop BLE advertising (but keep stack alive for fast restart)
 */
void ble_handler_stop() {
    if (ble_server == nullptr) {
        Serial.println("[BLE] BLE not started - nothing to stop");
        return;  // Never started
    }
    
    Serial.println("[BLE] Stopping BLE advertising...");
    
    // Just stop advertising - keep the BLE stack alive for faster restart
    // This prevents the hang that occurs when deinit/init happens rapidly
    BLEDevice::getAdvertising()->stop();
    
    // Disconnect any connected clients
    if (ble_connected) {
        ble_server->disconnect(ble_server->getConnId());
        ble_connected = false;
    }
    
    // Note: We intentionally DON'T call BLEDevice::deinit() because:
    // 1. Reinitializing BLE causes hangs on ESP32
    // 2. Keeping stack alive allows instant restart
    // 3. BLE uses minimal power when not advertising
    
    Serial.println("[BLE] BLE advertising stopped (stack remains active)");
}

/**
 * @brief Update BLE (call in main loop)
 */
void ble_handler_update() {
    // BLE is event-driven via callbacks, no polling needed
    // This function is here for future use (e.g., notifications)
}

/**
 * @brief Check if BLE is connected to phone
 */
bool ble_handler_is_connected() {
    return ble_connected;
}

/**
 * @brief Get current connection mode
 */
ConnectionMode ble_handler_get_mode() {
    return current_mode;
}

/**
 * @brief Set connection mode and switch accordingly
 */
void ble_handler_set_mode(ConnectionMode mode) {
    static bool switching_in_progress = false;
    
    // Prevent re-entrant calls
    if (switching_in_progress) {
        Serial.println("[BLE] Mode switch already in progress - ignoring request");
        return;
    }
    
    if (mode == current_mode) {
        Serial.println("[BLE] Already in requested mode - no change needed");
        return;  // Already in this mode
    }
    
    switching_in_progress = true;
    
    Serial.printf("[BLE] Switching mode: %s -> %s\n", 
                  current_mode == ConnectionMode::WIFI ? "WiFi" : "BLE",
                  mode == ConnectionMode::WIFI ? "WiFi" : "BLE");
    
    if (mode == ConnectionMode::BLE) {
        /* Switch to BLE mode */
        Serial.println("[BLE] ====== Switching to BLE Mode ======");
        
        // 1. Disconnect WiFi first
        Serial.println("[BLE] Step 1: Disconnecting WiFi...");
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
        delay(200);  // Give WiFi time to fully shut down
        Serial.println("[BLE] WiFi disabled");
        
        // 2. Start BLE
        Serial.println("[BLE] Step 2: Starting BLE...");
        ble_handler_start();
        
        current_mode = ConnectionMode::BLE;
        Serial.println("[BLE] ====== BLE Mode Active ======");
        
    } else {
        /* Switch to WiFi mode */
        Serial.println("[BLE] ====== Switching to WiFi Mode ======");
        
        // 1. Stop BLE
        Serial.println("[BLE] Step 1: Stopping BLE...");
        ble_handler_stop();
        delay(100);
        
        // 2. Note: WiFi will be reconnected by main loop via wifi_handler
        current_mode = ConnectionMode::WIFI;
        Serial.println("[BLE] ====== WiFi Mode Active ======");
        Serial.println("[BLE] WiFi will reconnect automatically");
    }
    
    switching_in_progress = false;
}

/**
 * @brief Get time sync status from BLE
 */
bool ble_handler_time_synced() {
    return time_synced_via_ble;
}

/**
 * @brief Placeholder: Get weather data (not used - weather requires WiFi)
 */
BLEWeatherData ble_handler_get_weather() {
    BLEWeatherData empty = {0, "", "", '?', false};
    return empty;
}

/**
 * @brief Placeholder: Check weather updates (not used - weather requires WiFi)
 */
bool ble_handler_weather_updated() {
    return false;
}

