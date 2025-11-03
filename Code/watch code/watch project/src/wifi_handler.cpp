#include "wifi_handler.h"
#include "secrets.h"
#include <Arduino.h>
#include <WiFi.h>
#include <time.h>

static const char *TAG = "WiFi";

// Local constants
constexpr unsigned long WIFI_RETRY_INTERVAL = 60000; // 60s
constexpr unsigned long NTP_RETRY_INTERVAL = 20000;  // 20s
constexpr unsigned long WIFI_CONNECT_TIMEOUT = 15000; // 15s timeout for connection

void initWiFi(long gmtOffsetSec, int daylightOffsetSec, unsigned long &lastNtpAttempt) {
  delay(500);  // Ensure Serial is ready
  Serial.println(F("\n[WiFi] ========== WIFI INIT START (Arduino WiFi) =========="));
  delay(200);
  
  Serial.printf("WiFi SSID: %s\n", WIFI_SSID);
  size_t pass_len = strlen(WIFI_PASS);
  Serial.printf("WiFi Password length: %d\n", pass_len);
  
  // Disconnect if already connected
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println(F("[WiFi] Already connected, disconnecting..."));
    WiFi.disconnect();
    delay(500);
  }
  
  // Set WiFi mode to Station
  WiFi.mode(WIFI_STA);
  Serial.println(F("[WiFi] Mode: STA (Station)"));
  
  // Set hostname
  WiFi.setHostname("OpenWatch");
  
  // Enable auto-reconnect
  WiFi.setAutoReconnect(true);
  WiFi.persistent(false);  // Don't save WiFi config to flash
  
  // Disable power saving for better connectivity
  WiFi.setSleep(false);
  Serial.println(F("[WiFi] Auto-reconnect: ON, Power save: OFF"));
  
  // Begin connecting - simple Arduino WiFi API
  Serial.println(F("[WiFi] Connecting..."));
  
  wl_status_t status;
  if (pass_len == 0) {
    // Open network - no password
    Serial.println(F("[WiFi] Connecting to OPEN network (no password)"));
    status = WiFi.begin(WIFI_SSID);
  } else {
    // Secured network - with password
    Serial.println(F("[WiFi] Connecting to WPA2 network"));
    status = WiFi.begin(WIFI_SSID, WIFI_PASS);
  }
  
  // Wait for connection with timeout
  Serial.print(F("[WiFi] Waiting for connection (max 15s)"));
  unsigned long startTime = millis();
  int dotCount = 0;
  
  while (WiFi.status() != WL_CONNECTED && (millis() - startTime) < WIFI_CONNECT_TIMEOUT) {
    delay(500);
    Serial.print(".");
    dotCount++;
    if (dotCount >= 6) {
      Serial.println();
      Serial.print(F("[WiFi] Still connecting"));
      dotCount = 0;
    }
  }
  Serial.println();
  
  // Check if connected
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println(F("\n[WiFi] ✓ CONNECTED!"));
    Serial.printf("[WiFi] IP: %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("[WiFi] RSSI: %d dBm\n", WiFi.RSSI());
    Serial.printf("[WiFi] Gateway: %s\n", WiFi.gatewayIP().toString().c_str());
    Serial.printf("[WiFi] Subnet: %s\n", WiFi.subnetMask().toString().c_str());
    Serial.printf("[WiFi] DNS: %s\n", WiFi.dnsIP().toString().c_str());
    
    // Configure NTP with timezone
    Serial.println(F("[WiFi] Configuring NTP..."));
    Serial.printf("[WiFi] Timezone: GMT%+d (DST: %d)\n", gmtOffsetSec/3600, daylightOffsetSec);
    configTime(gmtOffsetSec, daylightOffsetSec, "pool.ntp.org", "time.google.com");
    lastNtpAttempt = millis();
    
    // Wait for NTP sync
    Serial.print(F("[WiFi] Syncing time"));
    int ntp_attempts = 0;
    time_t now = 0;
    while (ntp_attempts < 10) {
      delay(500);
      Serial.print(".");
      now = time(nullptr);
      if (now > 100000) {
        Serial.println();
        Serial.println(F("[WiFi] ✓ NTP synced!"));
        struct tm timeinfo;
        localtime_r(&now, &timeinfo);
        Serial.printf("[WiFi] Time: %04d-%02d-%02d %02d:%02d:%02d\n",
                      timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                      timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
        break;
      }
      ntp_attempts++;
    }
    if (now <= 100000) {
      Serial.println();
      Serial.println(F("[WiFi] ⚠ NTP sync failed (will retry later)"));
    }
    
    Serial.println(F("========== WIFI INIT END (SUCCESS) ==========\n"));
  } else {
    // Failed to connect
    Serial.println(F("\n[WiFi] ✗ CONNECTION FAILED"));
    Serial.printf("[WiFi] Status: %d\n", WiFi.status());
    
    // Decode status
    switch(WiFi.status()) {
      case WL_NO_SSID_AVAIL:
        Serial.println(F("[WiFi] Reason: SSID not found"));
        break;
      case WL_CONNECT_FAILED:
        Serial.println(F("[WiFi] Reason: Connection failed (wrong password?)"));
        break;
      case WL_CONNECTION_LOST:
        Serial.println(F("[WiFi] Reason: Connection lost"));
        break;
      case WL_DISCONNECTED:
        Serial.println(F("[WiFi] Reason: Disconnected"));
        break;
      default:
        Serial.println(F("[WiFi] Reason: Unknown"));
        break;
    }
    
    Serial.println(F("[WiFi] Device will retry in background"));
    Serial.println(F("========== WIFI INIT END (FAILED - WILL RETRY) ==========\n"));
  }
}

bool isWiFiConnected() {
  return (WiFi.status() == WL_CONNECTED);
}

void handleWiFiReconnection(unsigned long now, unsigned long &lastWiFiAttempt) {
  static unsigned long lastStatusPrint = 0;
  static unsigned long disconnectedSince = 0;
  static unsigned long lastReconnectAttempt = 0;
  
  bool connected = isWiFiConnected();
  
  // Print WiFi status periodically for debugging
  if (now - lastStatusPrint > 10000) {  // Every 10 seconds
    if (connected) {
      Serial.printf("[WiFi] ✓ Connected, IP: %s, Signal: %d dBm\n", 
                    WiFi.localIP().toString().c_str(), WiFi.RSSI());
    } else {
      Serial.println(F("[WiFi] ✗ Not connected (retrying...)"));
    }
    lastStatusPrint = now;
  }
  
  // Track disconnection time
  if (!connected) {
    if (disconnectedSince == 0) {
      disconnectedSince = now;
      Serial.println(F("[WiFi] Disconnected - will retry connection"));
    }
  } else {
    disconnectedSince = 0;  // Reset on connection
  }
  
  // Aggressive reconnect - try every 30 seconds if disconnected
  if (disconnectedSince > 0 && 
      (now - lastReconnectAttempt > 30000)) {  // Every 30 seconds
    
    Serial.println(F("[WiFi] [BG] Attempting reconnect..."));
    WiFi.reconnect();
    lastReconnectAttempt = now;
  }
  
  // Note: NTP timezone configuration is done in initWiFi() and handleNTPRetry()
  // No need to reconfigure here as it would use wrong timezone (UTC)
}

void handleNTPRetry(unsigned long now, bool timeSynced, long gmtOffsetSec, int daylightOffsetSec, unsigned long &lastNtpAttempt) {
  // Retry NTP configuration if still not synced
  if (!timeSynced && isWiFiConnected() && (now - lastNtpAttempt > NTP_RETRY_INTERVAL)) {
    Serial.println(F("Retry NTP config."));
    // Configure with timezone and DST
    configTime(gmtOffsetSec, daylightOffsetSec, "pool.ntp.org", "time.nist.gov");
    lastNtpAttempt = now;
  }
}