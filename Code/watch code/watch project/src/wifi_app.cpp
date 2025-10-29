#include "wifi_app.h"
#include "app_manager.h"
#include "display_adapter.h"
#include <WiFi.h>

WiFiAppContext gWiFiAppCtx;

void wifiAppInit() {
  gWiFiAppCtx.state = WiFiAppState::SCANNING;
  gWiFiAppCtx.networkCount = 0;
  gWiFiAppCtx.selectedIndex = 0;
  gWiFiAppCtx.scrollOffset = 0;
  gWiFiAppCtx.scanning = false;
  gWiFiAppCtx.lastScanTime = 0;
  gWiFiAppCtx.selectedSSID[0] = '\0';
  gWiFiAppCtx.password[0] = '\0';
  gWiFiAppCtx.passwordLength = 0;
  gWiFiAppCtx.connectionAttempted = false;
  
  Serial.println("[WiFi App] Initialized");
}

void startNetworkScan() {
  if (gWiFiAppCtx.scanning) {
    return;  // Already scanning
  }
  
  Serial.println("[WiFi App] Starting network scan...");
  gWiFiAppCtx.scanning = true;
  gWiFiAppCtx.state = WiFiAppState::SCANNING;
  gWiFiAppCtx.networkCount = 0;
  
  // Perform the scan
  int n = WiFi.scanNetworks();
  
  Serial.printf("[WiFi App] Found %d networks\n", n);
  
  if (n > 0) {
    // Store up to 10 networks
    gWiFiAppCtx.networkCount = (n > 10) ? 10 : n;
    
    for (int i = 0; i < gWiFiAppCtx.networkCount; i++) {
      strncpy(gWiFiAppCtx.networks[i].ssid, WiFi.SSID(i).c_str(), 32);
      gWiFiAppCtx.networks[i].ssid[32] = '\0';
      gWiFiAppCtx.networks[i].rssi = WiFi.RSSI(i);
      gWiFiAppCtx.networks[i].encrypted = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
      gWiFiAppCtx.networks[i].authMode = WiFi.encryptionType(i);
      
      Serial.printf("  %d: %s (%d dBm) %s\n", 
                    i, 
                    gWiFiAppCtx.networks[i].ssid,
                    gWiFiAppCtx.networks[i].rssi,
                    gWiFiAppCtx.networks[i].encrypted ? "🔒" : "Open");
    }
    
    gWiFiAppCtx.state = WiFiAppState::NETWORK_LIST;
  } else {
    Serial.println("[WiFi App] No networks found");
    gWiFiAppCtx.networkCount = 0;
    gWiFiAppCtx.state = WiFiAppState::NETWORK_LIST;
  }
  
  gWiFiAppCtx.scanning = false;
  gWiFiAppCtx.lastScanTime = millis();
}

void connectToSelectedNetwork() {
  if (gWiFiAppCtx.selectedIndex >= gWiFiAppCtx.networkCount) {
    return;
  }
  
  NetworkInfo& network = gWiFiAppCtx.networks[gWiFiAppCtx.selectedIndex];
  strncpy(gWiFiAppCtx.selectedSSID, network.ssid, 32);
  gWiFiAppCtx.selectedSSID[32] = '\0';
  
  Serial.printf("[WiFi App] Connecting to: %s\n", gWiFiAppCtx.selectedSSID);
  
  gWiFiAppCtx.state = WiFiAppState::CONNECTING;
  
  // Disconnect from current network
  WiFi.disconnect(false);
  delay(100);
  
  // Connect based on whether network is encrypted
  if (network.encrypted) {
    // For now, try empty password or would need password entry UI
    Serial.println("[WiFi App] Network is encrypted - password required");
    // TODO: Implement password entry
    gWiFiAppCtx.state = WiFiAppState::ENTER_PASSWORD;
  } else {
    // Open network
    WiFi.begin(network.ssid);
    gWiFiAppCtx.connectionAttempted = true;
  }
}

void wifiAppUpdate() {
  // Check connection status if we attempted a connection
  if (gWiFiAppCtx.state == WiFiAppState::CONNECTING && gWiFiAppCtx.connectionAttempted) {
    if (WiFi.status() == WL_CONNECTED) {
      Serial.printf("[WiFi App] Connected! IP: %s\n", WiFi.localIP().toString().c_str());
      gWiFiAppCtx.state = WiFiAppState::CONNECTED;
    } else if (millis() - gWiFiAppCtx.lastScanTime > 10000) {
      // Connection timeout after 10 seconds
      Serial.println("[WiFi App] Connection timeout");
      gWiFiAppCtx.state = WiFiAppState::NETWORK_LIST;
      gWiFiAppCtx.connectionAttempted = false;
    }
  }
  
  // Auto-scan on first entry
  if (gWiFiAppCtx.state == WiFiAppState::SCANNING && !gWiFiAppCtx.scanning) {
    if (millis() - gWiFiAppCtx.lastScanTime > 1000) {
      startNetworkScan();
    }
  }
}

void wifiAppDraw(Gc9Display &display) {
  display.fillScreen(COLOR_BLACK);
  display.setTextColor(COLOR_WHITE);
  display.setTextSize(1);
  
  // Title
  display.setCursor(80, 10);
  display.print("WiFi");
  
  switch (gWiFiAppCtx.state) {
    case WiFiAppState::SCANNING:
      display.setCursor(60, 100);
      display.print("Scanning...");
      
      // Draw spinning animation
      {
        static int angle = 0;
        angle = (angle + 10) % 360;
        int centerX = 120;
        int centerY = 140;
        int radius = 20;
        
        for (int i = 0; i < 8; i++) {
          int a = (angle + i * 45) % 360;
          int x = centerX + (radius * cos(a * 3.14159 / 180.0));
          int y = centerY + (radius * sin(a * 3.14159 / 180.0));
          int size = (i == 0) ? 4 : 2;
          display.fillCircle(x, y, size, COLOR_WHITE);
        }
      }
      break;
      
    case WiFiAppState::NETWORK_LIST:
      if (gWiFiAppCtx.networkCount == 0) {
        display.setCursor(40, 100);
        display.print("No networks found");
        display.setCursor(30, 120);
        display.print("Press to rescan");
      } else {
        // Draw network list (show up to 5 at a time)
        int startY = 35;
        int itemHeight = 38;
        int visibleItems = 5;
        
        // Adjust scroll offset to keep selected item visible
        if (gWiFiAppCtx.selectedIndex < gWiFiAppCtx.scrollOffset) {
          gWiFiAppCtx.scrollOffset = gWiFiAppCtx.selectedIndex;
        }
        if (gWiFiAppCtx.selectedIndex >= gWiFiAppCtx.scrollOffset + visibleItems) {
          gWiFiAppCtx.scrollOffset = gWiFiAppCtx.selectedIndex - visibleItems + 1;
        }
        
        for (int i = 0; i < visibleItems && (i + gWiFiAppCtx.scrollOffset) < gWiFiAppCtx.networkCount; i++) {
          int networkIndex = i + gWiFiAppCtx.scrollOffset;
          int y = startY + (i * itemHeight);
          
          // Highlight selected network
          if (networkIndex == gWiFiAppCtx.selectedIndex) {
            display.fillRect(5, y - 2, 230, itemHeight - 2, COLOR_BLUE);
            display.setTextColor(COLOR_WHITE);
          } else {
            display.setTextColor(COLOR_LIGHTGRAY);
          }
          
          // Draw SSID
          display.setCursor(10, y + 2);
          display.setTextSize(1);
          
          // Truncate long SSIDs
          char displaySSID[21];
          strncpy(displaySSID, gWiFiAppCtx.networks[networkIndex].ssid, 20);
          displaySSID[20] = '\0';
          display.print(displaySSID);
          
          // Draw lock icon if encrypted
          if (gWiFiAppCtx.networks[networkIndex].encrypted) {
            display.setCursor(10, y + 14);
            display.setTextSize(1);
            display.print("LOCK");
          }
          
          // Draw signal strength
          int rssi = gWiFiAppCtx.networks[networkIndex].rssi;
          int bars = 0;
          if (rssi > -60) bars = 4;
          else if (rssi > -70) bars = 3;
          else if (rssi > -80) bars = 2;
          else bars = 1;
          
          int barX = 200;
          int barY = y + 20;
          for (int b = 0; b < 4; b++) {
            int barHeight = (b + 1) * 4;
            uint16_t barColor = (b < bars) ? COLOR_GREEN : COLOR_DARKGRAY;
            display.fillRect(barX + (b * 6), barY - barHeight, 4, barHeight, barColor);
          }
          
          // Reset text color
          display.setTextColor(COLOR_WHITE);
        }
        
        // Draw scroll indicator
        if (gWiFiAppCtx.networkCount > visibleItems) {
          display.setCursor(230, 110);
          display.print("^");
          display.setCursor(230, 210);
          display.print("v");
        }
      }
      break;
      
    case WiFiAppState::CONNECTING:
      display.setCursor(50, 100);
      display.print("Connecting to:");
      display.setCursor(20, 120);
      display.print(gWiFiAppCtx.selectedSSID);
      
      // Draw progress dots
      {
        static int dots = 0;
        dots = (dots + 1) % 4;
        display.setCursor(90, 140);
        for (int i = 0; i < dots; i++) {
          display.print(".");
        }
      }
      break;
      
    case WiFiAppState::CONNECTED:
      display.setCursor(60, 80);
      display.setTextColor(COLOR_GREEN);
      display.print("Connected!");
      display.setTextColor(COLOR_WHITE);
      
      display.setCursor(20, 110);
      display.print(gWiFiAppCtx.selectedSSID);
      
      display.setCursor(30, 140);
      display.print("IP:");
      display.setCursor(60, 140);
      display.print(WiFi.localIP().toString());
      
      display.setCursor(20, 170);
      display.print("Signal:");
      display.setCursor(80, 170);
      display.print(WiFi.RSSI());
      display.print(" dBm");
      break;
      
    case WiFiAppState::ENTER_PASSWORD:
      display.setCursor(40, 80);
      display.print("Password Entry");
      display.setCursor(20, 110);
      display.print("Not yet implemented");
      display.setCursor(30, 140);
      display.print("Press to go back");
      break;
  }
  
  display.display();
}

void wifiAppHandleButton(int buttonEvent) {
  ButtonEvent event = (ButtonEvent)buttonEvent;
  
  if (event == ButtonEvent::NONE) {
    return;
  }
  
  Serial.printf("[WiFi App] Button event: %d, State: %d\n", (int)event, (int)gWiFiAppCtx.state);
  
  switch (gWiFiAppCtx.state) {
    case WiFiAppState::SCANNING:
      // Can't do anything while scanning
      break;
      
    case WiFiAppState::NETWORK_LIST:
      if (gWiFiAppCtx.networkCount == 0) {
        // Rescan
        if (event == ButtonEvent::SHORT_PRESS) {
          startNetworkScan();
        }
      } else {
        if (event == ButtonEvent::SHORT_PRESS) {
          // Move selection down
          gWiFiAppCtx.selectedIndex++;
          if (gWiFiAppCtx.selectedIndex >= gWiFiAppCtx.networkCount) {
            gWiFiAppCtx.selectedIndex = 0;
          }
          Serial.printf("[WiFi App] Selected index: %d\n", gWiFiAppCtx.selectedIndex);
        } else if (event == ButtonEvent::LONG_PRESS) {
          // Connect to selected network
          connectToSelectedNetwork();
        }
      }
      break;
      
    case WiFiAppState::CONNECTING:
      if (event == ButtonEvent::LONG_PRESS) {
        // Cancel connection
        WiFi.disconnect(false);
        gWiFiAppCtx.state = WiFiAppState::NETWORK_LIST;
        gWiFiAppCtx.connectionAttempted = false;
      }
      break;
      
    case WiFiAppState::CONNECTED:
      if (event == ButtonEvent::SHORT_PRESS) {
        // Go back to network list
        gWiFiAppCtx.state = WiFiAppState::NETWORK_LIST;
      }
      break;
      
    case WiFiAppState::ENTER_PASSWORD:
      if (event == ButtonEvent::SHORT_PRESS) {
        // Go back to network list
        gWiFiAppCtx.state = WiFiAppState::NETWORK_LIST;
      }
      break;
  }
}

void wifiAppReset(Gc9Display &display) {
  wifiAppInit();
}

void registerWiFiApp(AppManager& appManager) {
  App wifiApp = {
    "WiFi",
    wifiAppDraw,
    wifiAppUpdate,
    wifiAppHandleButton,
    false,
    wifiAppReset
  };
  
  appManager.registerApp(wifiApp);
  Serial.println("[WiFi App] Registered with app manager");
}
