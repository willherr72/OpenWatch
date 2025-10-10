#include "weather_app.h"
#include "app_manager.h"
#include "secrets.h"
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
  strcpy(gWeatherCtx.weather.location, "Houston, TX");
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
  
  Serial.println("Weather app initialized - waiting for real data");
}

void weatherUpdate() {
  static unsigned long lastDebugTime = 0;
  unsigned long now = millis();
  
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
  
  // Use shorter retry interval if no valid data exists
  unsigned long retryInterval = gWeatherCtx.weather.valid ? gWeatherCtx.FETCH_INTERVAL_MS : 60000; // 1 minute retry if no data
  
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
  Serial.println("=== OPENWEATHERMAP FETCH START ===");
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected - cannot fetch weather data");
    return false;
  }

  WiFiClientSecure client;
  client.setInsecure(); // For simplicity - in production use proper certificates
  HTTPClient http;
  
  // OpenWeatherMap Current Weather API
  String url = "https://api.openweathermap.org/data/2.5/weather?q=";
  url += OPENWEATHER_CITY;
  url += "&appid=";
  url += OPENWEATHER_API_KEY;
  url += "&units=imperial"; // Fahrenheit temperatures
  
  Serial.printf("URL: %s\n", url.c_str());
  http.begin(client, url);
  http.addHeader("User-Agent", "ESP32-Weather-Watch");
  http.setTimeout(10000); // 10 second timeout
  
  Serial.println("Sending HTTP GET request to OpenWeatherMap...");
  int code = http.GET();
  Serial.printf("HTTP Response Code: %d\n", code);
  
  if (code == 200) {
    String payload = http.getString();
    Serial.printf("Weather API Response length: %d bytes\n", payload.length());
    Serial.printf("First 300 chars: %.300s\n", payload.c_str());
    
    // Parse JSON response
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload);
    
    if (!err) {
      Serial.println("=== PARSING OPENWEATHERMAP DATA ===");
      
      // Extract current temperature
      if (doc["main"]["temp"].is<float>()) {
        gWeatherCtx.weather.temperature = doc["main"]["temp"].as<float>();
        Serial.printf("Current temp: %.1fF\n", gWeatherCtx.weather.temperature);
      }
      
      // Extract weather description
      if (doc["weather"][0]["description"].is<const char*>()) {
        String desc = doc["weather"][0]["description"].as<const char*>();
        // Capitalize first letter
        if (desc.length() > 0) {
          desc[0] = toupper(desc[0]);
        }
        strncpy(gWeatherCtx.weather.description, desc.c_str(), sizeof(gWeatherCtx.weather.description)-1);
        gWeatherCtx.weather.description[sizeof(gWeatherCtx.weather.description)-1] = '\0';
        Serial.printf("Description: %s\n", gWeatherCtx.weather.description);
      }
      
      // Extract high/low temperatures (these are min/max for today)
      if (doc["main"]["temp_max"].is<float>()) {
        gWeatherCtx.weather.highTemp = doc["main"]["temp_max"].as<float>();
        Serial.printf("High temp: %.1fF\n", gWeatherCtx.weather.highTemp);
      } else {
        gWeatherCtx.weather.highTemp = gWeatherCtx.weather.temperature + 3;
      }
      
      if (doc["main"]["temp_min"].is<float>()) {
        gWeatherCtx.weather.lowTemp = doc["main"]["temp_min"].as<float>();
        Serial.printf("Low temp: %.1fF\n", gWeatherCtx.weather.lowTemp);
      } else {
        gWeatherCtx.weather.lowTemp = gWeatherCtx.weather.temperature - 3;
      }
      
      // Extract additional data
      if (doc["main"]["humidity"].is<int>()) {
        gWeatherCtx.weather.humidity = doc["main"]["humidity"].as<float>();
      }
      
      if (doc["main"]["pressure"].is<float>()) {
        gWeatherCtx.weather.pressure = doc["main"]["pressure"].as<float>();
      }
      
      if (doc["wind"]["speed"].is<float>()) {
        gWeatherCtx.weather.windSpeed = (int)(doc["wind"]["speed"].as<float>() + 0.5); // Round to nearest mph
      }
      
      // Update location to confirm correct city
      if (doc["name"].is<const char*>()) {
        String cityName = doc["name"].as<const char*>();
        strncpy(gWeatherCtx.weather.location, (cityName + ", TX").c_str(), sizeof(gWeatherCtx.weather.location)-1);
        gWeatherCtx.weather.location[sizeof(gWeatherCtx.weather.location)-1] = '\0';
        Serial.printf("Location: %s\n", gWeatherCtx.weather.location);
      }
      
      gWeatherCtx.weather.lastUpdate = millis();
      gWeatherCtx.weather.valid = true;
      
      Serial.printf("=== FINAL PARSED VALUES ===\n");
      Serial.printf("Current: %.1fF, High: %.1fF, Low: %.1fF\n", 
                    gWeatherCtx.weather.temperature, gWeatherCtx.weather.highTemp, gWeatherCtx.weather.lowTemp);
      Serial.printf("Description: %s\n", gWeatherCtx.weather.description);
      Serial.printf("Location: %s\n", gWeatherCtx.weather.location);
      Serial.printf("Humidity: %.0f%%, Pressure: %.0fhPa, Wind: %dmph\n", 
                    gWeatherCtx.weather.humidity, gWeatherCtx.weather.pressure, gWeatherCtx.weather.windSpeed);
      
      http.end();
      Serial.println("=== OPENWEATHERMAP FETCH END (SUCCESS) ===");
      return true;
    } else {
      Serial.printf("JSON parse error: %s\n", err.c_str());
    }
  } else {
    Serial.printf("OpenWeatherMap HTTP failure - code: %d\n", code);
    if (code > 0) {
      String errorResponse = http.getString();
      Serial.printf("Error response: %s\n", errorResponse.c_str());
    }
  }
  
  http.end();
  Serial.println("=== OPENWEATHERMAP FETCH END (FAILED) ===");
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