#include "wifi_handler.h"
#include "secrets.h"
#include <WiFi.h>

// Local constants
constexpr unsigned long WIFI_RETRY_INTERVAL = 60000; // 60s - increased to avoid frequent reconnection attempts
constexpr unsigned long NTP_RETRY_INTERVAL = 20000;  // 20s

void initWiFi(long gmtOffsetSec, int daylightOffsetSec, unsigned long &lastNtpAttempt) {
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  
  Serial.print(F("Connecting to ")); 
  Serial.println(F(WIFI_SSID));
  Serial.println(F("WiFi connecting in background (non-blocking)..."));
  
  // Don't block - WiFi will continue connecting in the background
  // The handleWiFiReconnection() and handleNTPRetry() functions in the main loop
  // will handle the connection attempts and NTP sync when WiFi connects
}

bool isWiFiConnected() {
  return WiFi.status() == WL_CONNECTED;
}

void handleWiFiReconnection(unsigned long now, unsigned long &lastWiFiAttempt) {
  // WiFi reconnection is handled automatically by ESP32's autoReconnect
  // We only check for NTP configuration when connected
  
  wl_status_t status = WiFi.status();
  
  // Check if just connected - configure NTP
  static bool ntpConfigured = false;
  if (status == WL_CONNECTED && !ntpConfigured) {
    Serial.print(F("WiFi connected, IP: ")); 
    Serial.println(WiFi.localIP());
    // Configure NTP for UTC time - we'll handle timezone conversion manually
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    ntpConfigured = true;
    lastWiFiAttempt = now;
    return;
  }
  
  // Note: Manual reconnection attempts have been removed to avoid blocking the main loop
  // The ESP32's autoReconnect feature handles WiFi reconnection in the background
}

void handleNTPRetry(unsigned long now, bool timeSynced, long gmtOffsetSec, int daylightOffsetSec, unsigned long &lastNtpAttempt) {
  // Retry NTP configuration if still not synced
  if (!timeSynced && WiFi.status() == WL_CONNECTED && (now - lastNtpAttempt > NTP_RETRY_INTERVAL)) {
    Serial.println(F("Retry NTP config."));
    // Configure for UTC time - timezone handled manually in display code
    configTime(0, 0, "pool.ntp.org", "time.google.com");
    lastNtpAttempt = now;
  }
}