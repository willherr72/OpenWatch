#include "wifi_handler.h"
#include "secrets.h"
#include <WiFi.h>

// Local constants
constexpr unsigned long WIFI_RETRY_INTERVAL = 15000; // 15s
constexpr unsigned long NTP_RETRY_INTERVAL = 20000;  // 20s

void initWiFi(long gmtOffsetSec, int daylightOffsetSec, unsigned long &lastNtpAttempt) {
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  
  Serial.print(F("Connecting to ")); 
  Serial.println(F(WIFI_SSID));
  
  uint32_t connectStart = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - connectStart < 12000) {
    delay(300); 
    Serial.print('.');
  }
  Serial.println();
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print(F("WiFi connected, IP: ")); 
    Serial.println(WiFi.localIP());
    // Configure NTP for UTC time - we'll handle timezone conversion manually
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    lastNtpAttempt = millis();
  } else {
    Serial.println(F("WiFi connect failed."));
  }
}

bool isWiFiConnected() {
  return WiFi.status() == WL_CONNECTED;
}

void handleWiFiReconnection(unsigned long now, unsigned long &lastWiFiAttempt) {
  // Retry WiFi if disconnected
  if (WiFi.status() != WL_CONNECTED && (now - lastWiFiAttempt > WIFI_RETRY_INTERVAL)) {
    lastWiFiAttempt = now;
    Serial.println(F("Retry WiFi..."));
    WiFi.disconnect();
    WiFi.begin(WIFI_SSID, WIFI_PASS);
  }
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