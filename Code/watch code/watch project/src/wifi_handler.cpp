#include "wifi_handler.h"
#include "secrets.h"
#include <WiFi.h>

// Local constants
constexpr unsigned long WIFI_RETRY_INTERVAL = 60000; // 60s - increased to avoid frequent reconnection attempts
constexpr unsigned long NTP_RETRY_INTERVAL = 20000;  // 20s

void initWiFi(long gmtOffsetSec, int daylightOffsetSec, unsigned long &lastNtpAttempt) {
  Serial.println(F("=== WIFI INIT START ==="));
  Serial.printf("WiFi SSID: %s\n", WIFI_SSID);
  Serial.printf("WiFi Password length: %d\n", strlen(WIFI_PASS));
  
  // Complete WiFi driver reset for ESP32-S3
  WiFi.disconnect(true);  // Disconnect and erase old config
  WiFi.mode(WIFI_OFF);     // Turn off WiFi completely
  delay(500);              // Wait for WiFi to fully shut down
  
  // Reinitialize WiFi properly
  WiFi.mode(WIFI_STA);
  WiFi.persistent(false);  // Don't save credentials to flash
  WiFi.setAutoReconnect(true);
  WiFi.setSleep(false);    // Disable WiFi sleep for better stability
  
  Serial.print(F("Connecting to ")); 
  Serial.println(F(WIFI_SSID));
  
  // For open networks (empty password), use WiFi.begin with just SSID
  if (strlen(WIFI_PASS) == 0) {
    Serial.println(F("Open network detected - connecting without password"));
    WiFi.begin(WIFI_SSID);
  } else {
    WiFi.begin(WIFI_SSID, WIFI_PASS);
  }
  
  // Wait up to 10 seconds for initial connection (prioritize WiFi on boot)
  Serial.println(F("Waiting for WiFi connection (up to 10 seconds)..."));
  unsigned long startTime = millis();
  int dotCount = 0;
  
  while (WiFi.status() != WL_CONNECTED && (millis() - startTime) < 10000) {
    delay(500);
    Serial.print(".");
    dotCount++;
    if (dotCount >= 20) {
      Serial.println();
      dotCount = 0;
    }
  }
  Serial.println();
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println(F("WiFi CONNECTED!"));
    Serial.printf("IP Address: %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("Signal Strength: %d dBm\n", WiFi.RSSI());
    
    // Configure NTP immediately
    Serial.println(F("Configuring NTP time sync..."));
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    lastNtpAttempt = millis();
    
    // Wait a bit for NTP sync
    delay(1000);
    time_t now = time(nullptr);
    if (now > 100000) {
      Serial.println(F("NTP time synced successfully!"));
      struct tm timeinfo;
      localtime_r(&now, &timeinfo);
      Serial.printf("Current UTC time: %04d-%02d-%02d %02d:%02d:%02d\n",
                    timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                    timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    } else {
      Serial.println(F("NTP sync in progress..."));
    }
  } else {
    Serial.println(F("WiFi connection FAILED"));
    Serial.printf("Final status: %d - ", WiFi.status());
    switch(WiFi.status()) {
      case WL_NO_SHIELD: Serial.println("NO_SHIELD"); break;
      case WL_IDLE_STATUS: Serial.println("IDLE"); break;
      case WL_NO_SSID_AVAIL: Serial.println("NO_SSID_AVAIL (Network not found)"); break;
      case WL_SCAN_COMPLETED: Serial.println("SCAN_COMPLETED"); break;
      case WL_CONNECT_FAILED: Serial.println("CONNECT_FAILED (Wrong password?)"); break;
      case WL_CONNECTION_LOST: Serial.println("CONNECTION_LOST"); break;
      case WL_DISCONNECTED: Serial.println("DISCONNECTED"); break;
      default: Serial.println("UNKNOWN"); break;
    }
    
    // Scan for networks to verify SSID is visible
    Serial.println(F("[WiFi] Scanning for networks..."));
    int n = WiFi.scanNetworks();
    if (n == 0) {
      Serial.println(F("[WiFi] No networks found"));
    } else {
      Serial.printf("[WiFi] Found %d networks:\n", n);
      bool foundTarget = false;
      for (int i = 0; i < n && i < 10; ++i) {  // Show first 10
        String ssid = WiFi.SSID(i);
        Serial.printf("  %d: %s (RSSI: %d, Ch: %d, Enc: %s)\n", 
                     i + 1, ssid.c_str(), WiFi.RSSI(i), WiFi.channel(i),
                     WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "OPEN" : "ENCRYPTED");
        if (ssid == WIFI_SSID) {
          foundTarget = true;
          Serial.printf("  ^^ TARGET NETWORK FOUND! RSSI: %d\n", WiFi.RSSI(i));
        }
      }
      if (!foundTarget) {
        Serial.printf("[WiFi] WARNING: Target network '%s' NOT visible in scan!\n", WIFI_SSID);
      }
    }
    WiFi.scanDelete();  // Clean up
    
    Serial.println(F("Will retry in background..."));
  }
  
  Serial.println(F("=== WIFI INIT END ==="));
}

bool isWiFiConnected() {
  return WiFi.status() == WL_CONNECTED;
}

void handleWiFiReconnection(unsigned long now, unsigned long &lastWiFiAttempt) {
  wl_status_t status = WiFi.status();
  
  // Print WiFi status periodically for debugging
  static unsigned long lastStatusPrint = 0;
  if (now - lastStatusPrint > 10000) {  // Every 10 seconds (less spammy)
    if (status == WL_CONNECTED) {
      Serial.printf("[WiFi] Connected, IP: %s, Signal: %d dBm\n", 
                    WiFi.localIP().toString().c_str(), WiFi.RSSI());
    } else {
      Serial.printf("[WiFi] Disconnected - ");
      switch(status) {
        case WL_NO_SHIELD: Serial.println("NO_SHIELD"); break;
        case WL_IDLE_STATUS: Serial.println("IDLE (connecting...)"); break;
        case WL_NO_SSID_AVAIL: Serial.println("NO_SSID_AVAIL"); break;
        case WL_SCAN_COMPLETED: Serial.println("SCAN_COMPLETED"); break;
        case WL_CONNECT_FAILED: Serial.println("CONNECT_FAILED"); break;
        case WL_CONNECTION_LOST: Serial.println("CONNECTION_LOST"); break;
        case WL_DISCONNECTED: Serial.println("DISCONNECTED"); break;
        default: Serial.println("UNKNOWN"); break;
      }
    }
    lastStatusPrint = now;
  }
  
  // Smart reconnection - rely on WiFi auto-reconnect, just monitor status
  // WiFi.setAutoReconnect(true) was set in initWiFi, so WiFi handles reconnection
  // We only manually intervene if stuck in a failed state for too long
  static unsigned long lastReconnectAttempt = 0;
  static unsigned long disconnectedSince = 0;
  
  // Track how long we've been disconnected
  if (status != WL_CONNECTED && status != WL_IDLE_STATUS) {
    if (disconnectedSince == 0) {
      disconnectedSince = now;
    }
  } else {
    disconnectedSince = 0;  // Reset if connected or connecting
  }
  
  // Only manually intervene if disconnected for more than 2 minutes and not trying
  // This prevents interfering with auto-reconnect
  if (disconnectedSince > 0 && 
      (now - disconnectedSince > 120000) &&  // Stuck for 2 minutes
      status != WL_IDLE_STATUS &&  // Not currently trying to connect
      (now - lastReconnectAttempt > 30000)) {  // Don't spam reconnects
    
    Serial.println(F("[WiFi] Stuck disconnected for >2min, forcing reconnect..."));
    WiFi.disconnect(false);  // Don't erase credentials
    delay(500);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    lastReconnectAttempt = now;
    disconnectedSince = 0;  // Reset timer
  }
  
  // Configure NTP when connected (only once)
  static bool ntpConfigured = false;
  if (status == WL_CONNECTED && !ntpConfigured) {
    Serial.println(F("[WiFi] Connected! Configuring NTP..."));
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    ntpConfigured = true;
    lastWiFiAttempt = now;
  }
  
  // Reset NTP flag if disconnected
  if (status != WL_CONNECTED && ntpConfigured) {
    ntpConfigured = false;
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