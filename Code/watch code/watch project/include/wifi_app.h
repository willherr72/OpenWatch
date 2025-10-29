#pragma once

#include "app_manager.h"
#include "button_handler.h"
#include <WiFi.h>

// WiFi app states
enum class WiFiAppState {
  SCANNING,
  NETWORK_LIST,
  CONNECTING,
  CONNECTED,
  ENTER_PASSWORD
};

// Network info structure
struct NetworkInfo {
  char ssid[33];
  int rssi;
  bool encrypted;
  wifi_auth_mode_t authMode;
};

// WiFi app context
struct WiFiAppContext {
  WiFiAppState state;
  NetworkInfo networks[10];  // Store up to 10 networks
  int networkCount;
  int selectedIndex;
  int scrollOffset;
  bool scanning;
  unsigned long lastScanTime;
  char selectedSSID[33];
  char password[64];
  int passwordLength;
  bool connectionAttempted;
};

extern WiFiAppContext gWiFiAppCtx;

// WiFi app functions
void wifiAppInit();
void wifiAppUpdate();
void wifiAppDraw(Gc9Display &display);
void wifiAppHandleButton(int buttonEvent);
void wifiAppReset(Gc9Display &display);

// Helper functions
void startNetworkScan();
void connectToSelectedNetwork();

// Register the app
void registerWiFiApp(AppManager& appManager);
