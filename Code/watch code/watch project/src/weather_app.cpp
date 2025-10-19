#include "weather_app.h"
#include "app_manager.h"
#include <time.h>
#include <cstring>

// Ensure ArduinoJson is included for StaticJsonDocument if not already via header
#include <ArduinoJson.h>

WeatherAppContext gWeatherCtx;

namespace {
struct WeatherRenderCache {
  bool initialized = false;
  bool showingNoData = false;
  char timeStr[16] = {0};
  char tempStr[16] = {0};
  char descStr[32] = {0};
  char highLowStr[32] = {0};
  char locationStr[32] = {0};
};

WeatherRenderCache renderCache;
}

void weatherInit() {
  // Initialize weather data - starts invalid until real data is fetched
  strcpy(gWeatherCtx.weather.location, "Pearland, TX");
  strcpy(gWeatherCtx.weather.description, "Loading...");
  gWeatherCtx.weather.temperature = 0.0;
  gWeatherCtx.weather.highTemp = 0.0;
  gWeatherCtx.weather.lowTemp = 0.0;
  gWeatherCtx.weather.humidity = 0.0;
  gWeatherCtx.weather.pressure = 0.0;
  gWeatherCtx.weather.windSpeed = 0;
  strcpy(gWeatherCtx.weather.windDirection, "");
  gWeatherCtx.weather.valid = false;  // Start invalid - only becomes valid when real data is fetched
  gWeatherCtx.weather.lastUpdate = 0;
  strcpy(gWeatherCtx.gridpoint, "");
  
  Serial.println("Weather app initialized - waiting for real data");
}

void weatherUpdate() {
  static unsigned long lastDebugTime = 0;
  unsigned long now = millis();
  
  // Don't attempt fetch if WiFi is not connected
  if (WiFi.status() != WL_CONNECTED) {
    return;
  }
  
  // Force immediate update on first call
  if (gWeatherCtx.lastFetchTime == 0) {
    Serial.println("First weather update - forcing immediate fetch");
    gWeatherCtx.lastFetchTime = now - gWeatherCtx.FETCH_INTERVAL_MS - 1000;
  }
  
  // Reduce debug spam - only print every 10 seconds
  if (now - lastDebugTime > 10000) {
    Serial.printf("Weather status: WiFi=%d, valid=%d, temp=%.1fF, desc=%s\n", 
                  WiFi.status(), gWeatherCtx.weather.valid, gWeatherCtx.weather.temperature, gWeatherCtx.weather.description);
    lastDebugTime = now;
  }
  
  // Use MUCH longer retry interval to avoid blocking the main loop too frequently
  // Only retry every 5 minutes if we have valid data, 1 minute if not
  unsigned long retryInterval = gWeatherCtx.weather.valid ? 300000 : 60000; // 5min or 1min retry
  
  if (!gWeatherCtx.fetching && (now - gWeatherCtx.lastFetchTime > retryInterval)) {
    if (WiFi.status() == WL_CONNECTED) {
      gWeatherCtx.fetching = true;
      Serial.println("Starting weather fetch...");
      if (fetchWeatherData()) {
        gWeatherCtx.lastFetchTime = now;
        Serial.println("Weather fetch completed successfully");
      } else {
        gWeatherCtx.lastFetchTime = now;  // Update time even on failure for retry timing
        Serial.println("Weather fetch failed - will retry later");
      }
      gWeatherCtx.fetching = false;
    }
  }
}

bool fetchWeatherData() {
  Serial.println("=== WEATHER.GOV FETCH START ===");
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected - cannot fetch weather data");
    return false;
  }

  WiFiClientSecure client;
  client.setInsecure(); // For simplicity - in production use proper certificates
  HTTPClient http;
  
  // Step 1: Get gridpoint if we don't have it yet
  if (strlen(gWeatherCtx.gridpoint) == 0) {
    // Use ZIP 77584 coordinates (Pearland, TX): 29.5636, -95.2861
    String pointsUrl = "https://api.weather.gov/points/29.5636,-95.2861";
    
    Serial.println("Fetching gridpoint from: " + pointsUrl);
    
    http.begin(client, pointsUrl);
    http.addHeader("User-Agent", "ESP32-Weather-Watch");
    http.setTimeout(5000);  // Reduced timeout to 5s
    int code = http.GET();
    
    yield();  // Allow button handler to run
    
    Serial.printf("Gridpoint HTTP Response Code: %d\n", code);
    
    if (code == 200) {
      String payload = http.getString();
      Serial.printf("Gridpoint API Response length: %d bytes\n", payload.length());
      
      JsonDocument doc;
      DeserializationError err = deserializeJson(doc, payload);
      
      yield();  // Allow button handler to run
      
      if (!err) {
        // Use regular forecast - smaller payload and sufficient for our needs
        const char* forecastUrl = doc["properties"]["forecast"];
        if (forecastUrl) {
          strncpy(gWeatherCtx.gridpoint, forecastUrl, sizeof(gWeatherCtx.gridpoint)-1);
          gWeatherCtx.gridpoint[sizeof(gWeatherCtx.gridpoint)-1] = '\0';
          Serial.printf("Got forecast URL: %s\n", gWeatherCtx.gridpoint);
        } else {
          Serial.println("Failed to get forecast URL from gridpoint");
          http.end();
          Serial.println("=== WEATHER.GOV FETCH END (FAILED - NO FORECAST URL) ===");
          return false;
        }
      } else {
        Serial.printf("Gridpoint JSON parse error: %s\n", err.c_str());
        http.end();
        Serial.println("=== WEATHER.GOV FETCH END (FAILED - GRIDPOINT PARSE ERROR) ===");
        return false;
      }
    } else {
      Serial.printf("Gridpoint HTTP failure - code: %d\n", code);
      http.end();
      Serial.println("=== WEATHER.GOV FETCH END (FAILED - GRIDPOINT HTTP ERROR) ===");
      return false;
    }
    
    http.end();
  }
  
  // Step 2: Fetch forecast data
  Serial.printf("Fetching forecast from: %s\n", gWeatherCtx.gridpoint);
  
  http.begin(client, gWeatherCtx.gridpoint);
  http.addHeader("User-Agent", "ESP32-Weather-Watch");
  http.setTimeout(5000);  // Reduced timeout to 5s
  int code = http.GET();
  
  yield();  // Allow button handler to run
  
  Serial.printf("Forecast HTTP Response Code: %d\n", code);
  
  if (code == 200) {
    String payload = http.getString();
    Serial.printf("Forecast API Response length: %d bytes\n", payload.length());
    
    // Regular forecast is much smaller (~8KB vs 61KB for hourly)
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload);
    
    yield();  // Allow button handler to run
    
    if (!err) {
      Serial.println("=== PARSING WEATHER.GOV DATA ===");
      
      // Extract current/next period from regular forecast
      JsonArray periods = doc["properties"]["periods"];
      int periodCount = periods.size();
      Serial.printf("Got %d forecast periods\n", periodCount);
      
      if (periodCount == 0) {
        Serial.println("No periods in forecast!");
        http.end();
        return false;
      }
      
      JsonObject period = periods[0];
      
      // Log what period we're getting
      if (period["name"].is<const char*>()) {
        Serial.printf("Period name: %s\n", period["name"].as<const char*>());
      }
      
      if (period["temperature"].is<int>()) {
        gWeatherCtx.weather.temperature = period["temperature"].as<float>();
        Serial.printf("Current temp: %.1fF\n", gWeatherCtx.weather.temperature);
      }
      
      if (period["shortForecast"].is<const char*>()) {
        String desc = period["shortForecast"].as<const char*>();
        strncpy(gWeatherCtx.weather.description, desc.c_str(), sizeof(gWeatherCtx.weather.description)-1);
        gWeatherCtx.weather.description[sizeof(gWeatherCtx.weather.description)-1] = '\0';
        Serial.printf("Description: %s\n", gWeatherCtx.weather.description);
      }
      
      // Calculate high/low from available periods (usually day/night pairs)
      float high = gWeatherCtx.weather.temperature;
      float low = gWeatherCtx.weather.temperature;
      
      // Process first few periods to get today's high/low
      for (int i = 0; i < periodCount && i < 4; i++) {
        JsonObject p = periods[i];
        if (p["temperature"].is<int>()) {
          float temp = p["temperature"].as<float>();
          if (temp > high) high = temp;
          if (temp < low) low = temp;
        }
      }
      
      gWeatherCtx.weather.highTemp = high;
      gWeatherCtx.weather.lowTemp = low;
      Serial.printf("High: %.1fF, Low: %.1fF (from %d periods)\n", high, low, periodCount);
      
      // NWS forecast API doesn't provide humidity/pressure
      // These would require the hourly forecast or observation stations
      gWeatherCtx.weather.humidity = 0;
      gWeatherCtx.weather.pressure = 0;
      gWeatherCtx.weather.windSpeed = 0;
      
      // Parse wind speed from windSpeed string (e.g., "5 to 10 mph")
      if (period["windSpeed"].is<const char*>()) {
        String windStr = period["windSpeed"].as<const char*>();
        Serial.printf("Wind string: %s\n", windStr.c_str());
        // Extract first number from string
        int windVal = windStr.toInt();
        if (windVal > 0) {
          gWeatherCtx.weather.windSpeed = windVal;
        }
      }
      
      strcpy(gWeatherCtx.weather.location, "Pearland, TX");
      
      gWeatherCtx.weather.lastUpdate = millis();
      gWeatherCtx.weather.valid = true;
      
      Serial.printf("=== FINAL PARSED VALUES ===\n");
      Serial.printf("Current: %.1fF, High: %.1fF, Low: %.1fF\n", 
                    gWeatherCtx.weather.temperature, gWeatherCtx.weather.highTemp, gWeatherCtx.weather.lowTemp);
      Serial.printf("Description: %s\n", gWeatherCtx.weather.description);
      Serial.printf("Location: %s\n", gWeatherCtx.weather.location);
      Serial.printf("Wind: %dmph\n", gWeatherCtx.weather.windSpeed);
      
      http.end();
      Serial.println("=== WEATHER.GOV FETCH END (SUCCESS) ===");
      return true;
    } else {
      Serial.printf("Forecast JSON parse error: %s\n", err.c_str());
    }
  } else {
    Serial.printf("Forecast HTTP failure - code: %d\n", code);
    if (code > 0) {
      String errorResponse = http.getString();
      Serial.printf("Error response: %s\n", errorResponse.c_str());
    }
  }
  
  http.end();
  Serial.println("=== WEATHER.GOV FETCH END (FAILED) ===");
  return false;
}

void resetWeatherDisplay(Gc9Display &display) {
  renderCache = WeatherRenderCache{};
  display.fillScreen(COLOR_BLACK);
}

// Updated draw function accepting display reference declared in header
void drawWeather(Gc9Display &display) {
  display.setTextColor(COLOR_WHITE);

  auto drawCentered = [&](const char *text, int16_t topY, uint8_t textSize) {
    display.setTextSize(textSize);
    int16_t x1, y1;
    uint16_t w, h;
    display.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
    int16_t x = (display.width() - static_cast<int16_t>(w)) / 2;
    if (x < 0) {
      x = 0;
    }
    display.setCursor(x, topY);
    display.print(text);
  };

  auto clearRow = [&](int16_t baselineY, uint8_t textSize) {
    int16_t charHeight = static_cast<int16_t>(textSize) * 8;
    int16_t clearY = baselineY - charHeight - 2;
    int16_t clearH = charHeight + 4;
    if (clearY < 0) {
      clearH += clearY;
      clearY = 0;
    }
    if (clearY + clearH > display.height()) {
      clearH = display.height() - clearY;
    }
    if (clearH > 0) {
      display.fillRect(0, clearY, display.width(), clearH, COLOR_BLACK);
    }
  };

  const char *noDataMsg = "No weather data";
  if (!gWeatherCtx.weather.valid) {
    if (!renderCache.initialized || !renderCache.showingNoData) {
      display.fillScreen(COLOR_BLACK);
      display.setTextSize(1);
      int16_t x1, y1; uint16_t w, h;
      display.getTextBounds(noDataMsg, 0, 0, &x1, &y1, &w, &h);
      int16_t x = (display.width() - static_cast<int16_t>(w)) / 2;
      int16_t y = (display.height() - static_cast<int16_t>(h)) / 2;
      if (x < 0) x = 0;
      if (y < 0) y = 0;
      display.setCursor(x, y);
      display.print(noDataMsg);
      display.display();

      renderCache = WeatherRenderCache{};
      renderCache.initialized = true;
      renderCache.showingNoData = true;
      snprintf(renderCache.descStr, sizeof(renderCache.descStr), "%s", noDataMsg);
    }
    return;
  }

  char timeBuf[16];
  char tempBuf[16];
  char descBuf[32];
  char hlBuf[32];
  char locationBuf[32];

  time_t now = time(nullptr);
  if (now > 100000) {
    // Manual timezone adjustment for Central Daylight Time (UTC-5)
    now -= 18000;  // Convert UTC to CDT (UTC-5)
  }
  struct tm * timeinfo = gmtime(&now);
  strftime(timeBuf, sizeof(timeBuf), "%I:%M %p", timeinfo);
  if (timeBuf[0] == '0') {
    memmove(timeBuf, timeBuf + 1, strlen(timeBuf));
  }

  snprintf(tempBuf, sizeof(tempBuf), "%.0fF", gWeatherCtx.weather.temperature);
  snprintf(descBuf, sizeof(descBuf), "%s", gWeatherCtx.weather.description);
  snprintf(hlBuf, sizeof(hlBuf), "H:%.0f L:%.0f", gWeatherCtx.weather.highTemp, gWeatherCtx.weather.lowTemp);
  snprintf(locationBuf, sizeof(locationBuf), "%s", gWeatherCtx.weather.location);

  bool forceFull = !renderCache.initialized || renderCache.showingNoData;
  bool anyChange = forceFull;

  if (forceFull) {
    display.fillScreen(COLOR_BLACK);
  }

  if (forceFull) {
    clearRow(20, 1);
    drawCentered("Weather", 20, 1);
  }

  if (forceFull || strcmp(renderCache.timeStr, timeBuf) != 0) {
    clearRow(44, 1);
    drawCentered(timeBuf, 44, 1);
    snprintf(renderCache.timeStr, sizeof(renderCache.timeStr), "%s", timeBuf);
    anyChange = true;
  }

  if (forceFull || strcmp(renderCache.tempStr, tempBuf) != 0) {
    clearRow(88, 4);
    drawCentered(tempBuf, 88, 4);
    snprintf(renderCache.tempStr, sizeof(renderCache.tempStr), "%s", tempBuf);
    anyChange = true;
  }

  if (forceFull || strcmp(renderCache.descStr, descBuf) != 0) {
    clearRow(160, 1);
    drawCentered(descBuf, 160, 1);
    snprintf(renderCache.descStr, sizeof(renderCache.descStr), "%s", descBuf);
    anyChange = true;
  }

  if (forceFull || strcmp(renderCache.highLowStr, hlBuf) != 0) {
    clearRow(184, 1);
    drawCentered(hlBuf, 184, 1);
    snprintf(renderCache.highLowStr, sizeof(renderCache.highLowStr), "%s", hlBuf);
    anyChange = true;
  }

  if (forceFull || strcmp(renderCache.locationStr, locationBuf) != 0) {
    clearRow(208, 1);
    drawCentered(locationBuf, 208, 1);
    snprintf(renderCache.locationStr, sizeof(renderCache.locationStr), "%s", locationBuf);
    anyChange = true;
  }

  if (anyChange) {
    display.display();
  }

  renderCache.initialized = true;
  renderCache.showingNoData = false;
}

void registerWeatherApp(AppManager& appManager) {
  App weatherApp = {
    "Weather",             // name
    drawWeather,           // drawFunction
    weatherUpdate,         // updateFunction
    nullptr,               // buttonHandler (not needed)
    false,                 // isSpecial (not the clock app)
    resetWeatherDisplay    // resetFunction
  };
  
  appManager.registerApp(weatherApp);
}