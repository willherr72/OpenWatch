#include "weather_app.h"
#include <time.h>

// Ensure ArduinoJson is included for StaticJsonDocument if not already via header
#include <ArduinoJson.h>

WeatherAppContext gWeatherCtx;

void weatherInit() {
  // Initialize weather data with default values
  strcpy(gWeatherCtx.weather.location, "Katy, TX");
  strcpy(gWeatherCtx.weather.description, "Loading...");
  gWeatherCtx.weather.temperature = 70.0;
  gWeatherCtx.weather.highTemp = 72.0;
  gWeatherCtx.weather.lowTemp = 68.0;
  gWeatherCtx.weather.humidity = 50.0;
  gWeatherCtx.weather.pressure = 30.0;
  gWeatherCtx.weather.windSpeed = 0;
  strcpy(gWeatherCtx.weather.windDirection, "N");
  gWeatherCtx.weather.valid = true;  // Set to true immediately
  gWeatherCtx.weather.lastUpdate = 0;
  
  Serial.println("Weather app initialized with default data");
}

void weatherUpdate() {
  unsigned long now = millis();
  
  // Force immediate update on first call
  if (gWeatherCtx.lastFetchTime == 0) {
    Serial.println("First weather update - forcing immediate fetch");
    gWeatherCtx.lastFetchTime = now - gWeatherCtx.FETCH_INTERVAL_MS - 1000;
  }
  
  if (!gWeatherCtx.fetching && (now - gWeatherCtx.lastFetchTime > gWeatherCtx.FETCH_INTERVAL_MS)) {
    Serial.printf("Weather update check: WiFi status = %d\n", WiFi.status());
    if (WiFi.status() == WL_CONNECTED) {
      gWeatherCtx.fetching = true;
      Serial.println("Starting weather fetch...");
      if (fetchWeatherData()) {
        gWeatherCtx.lastFetchTime = now;
        Serial.println("Weather fetch completed successfully");
      } else {
        Serial.println("Weather fetch failed");
      }
      gWeatherCtx.fetching = false;
    } else {
      Serial.println("WiFi not connected for weather update");
    }
  }
}

bool fetchWeatherData() {
  Serial.println("=== WEATHER FETCH START ===");
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected - using fallback data");
    gWeatherCtx.weather.temperature = 75.0;
    gWeatherCtx.weather.highTemp = gWeatherCtx.weather.temperature + 2;
    gWeatherCtx.weather.lowTemp = gWeatherCtx.weather.temperature - 2;
    strcpy(gWeatherCtx.weather.description, "WiFi Disconnected");
    gWeatherCtx.weather.lastUpdate = millis();
    return true;
  }

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  String url = "https://wttr.in/Katy,TX?format=j1"; // JSON format
  Serial.printf("URL: %s\n", url.c_str());
  http.begin(client, url);
  http.addHeader("User-Agent", "ESP32");
  http.setTimeout(12000);
  int code = http.GET();
  Serial.printf("HTTP Weather Code: %d\n", code);
  if (code == 200) {
  String payload = http.getString();
  // Filter to reduce memory usage (ArduinoJson v7 modern API)
  JsonDocument filter;
  filter["current_condition"][0]["temp_F"] = true;
  filter["current_condition"][0]["weatherDesc"][0]["value"] = true;
  filter["weather"][0]["maxtempF"] = true;
  filter["weather"][0]["mintempF"] = true;
  JsonDocument doc; // dynamic; library allocates needed capacity
  DeserializationError err = deserializeJson(doc, payload, DeserializationOption::Filter(filter));
    if (!err) {
      // current temp in Fahrenheit can be derived from current_condition.temp_F
      if (doc["current_condition"][0]["temp_F"].is<const char*>()) {
        gWeatherCtx.weather.temperature = atof(doc["current_condition"][0]["temp_F"].as<const char*>());
      }
      if (doc["current_condition"][0]["weatherDesc"][0]["value"].is<const char*>()) {
        strncpy(gWeatherCtx.weather.description, doc["current_condition"][0]["weatherDesc"][0]["value"].as<const char*>(), sizeof(gWeatherCtx.weather.description)-1);
        gWeatherCtx.weather.description[sizeof(gWeatherCtx.weather.description)-1] = '\0';
      }
      // high/low from first day in weather array (maxtempF, mintempF)
      if (doc["weather"][0]["maxtempF"].is<const char*>()) {
        gWeatherCtx.weather.highTemp = atof(doc["weather"][0]["maxtempF"].as<const char*>());
      } else {
        gWeatherCtx.weather.highTemp = gWeatherCtx.weather.temperature + 2;
      }
      if (doc["weather"][0]["mintempF"].is<const char*>()) {
        gWeatherCtx.weather.lowTemp = atof(doc["weather"][0]["mintempF"].as<const char*>());
      } else {
        gWeatherCtx.weather.lowTemp = gWeatherCtx.weather.temperature - 2;
      }
      gWeatherCtx.weather.lastUpdate = millis();
      gWeatherCtx.weather.valid = true;
      Serial.printf("Parsed weather: now=%.1fF H=%.1fF L=%.1fF %s\n", gWeatherCtx.weather.temperature, gWeatherCtx.weather.highTemp, gWeatherCtx.weather.lowTemp, gWeatherCtx.weather.description);
      http.end();
      Serial.println("=== WEATHER FETCH END ===");
      return true;
    } else {
      Serial.printf("JSON parse error: %s\n", err.c_str());
    }
  } else {
    Serial.println("Weather HTTP failure");
  }
  // fallback
  gWeatherCtx.weather.highTemp = gWeatherCtx.weather.temperature + 2;
  gWeatherCtx.weather.lowTemp = gWeatherCtx.weather.temperature - 2;
  if (strlen(gWeatherCtx.weather.description) == 0 || strcmp(gWeatherCtx.weather.description, "Loading...") == 0) {
    strcpy(gWeatherCtx.weather.description, "No Update");
  }
  gWeatherCtx.weather.valid = true;
  gWeatherCtx.weather.lastUpdate = millis();
  http.end();
  Serial.println("=== WEATHER FETCH END (FALLBACK) ===");
  return true;
}

// Updated draw function accepting display reference declared in header
void drawWeather(Adafruit_SSD1306 &display) {
  if (!gWeatherCtx.weather.valid) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("No weather data");
    display.display();
    return;
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  // Show time at top right
  display.setTextSize(1);
  // App label top-left
  display.setCursor(0, 0);
  display.print("Weather");
  
  char timeBuf[16];
  time_t now = time(nullptr);
  struct tm * timeinfo = localtime(&now);
  strftime(timeBuf, sizeof(timeBuf), "%I:%M", timeinfo);
  if (timeBuf[0] == '0') {
    for (int i = 0; i < 15; i++) {
      timeBuf[i] = timeBuf[i+1];
      if (timeBuf[i] == '\0') break;
    }
  }
  int16_t x1, y1; uint16_t w, h;
  display.getTextBounds(timeBuf, 0, 0, &x1, &y1, &w, &h);
  display.setCursor(128 - w, 0);
  display.print(timeBuf);

  // Temperature large in middle
  display.setTextSize(2);
  char tempBuf[16];
  snprintf(tempBuf, sizeof(tempBuf), "%.0fF", gWeatherCtx.weather.temperature);
  display.getTextBounds(tempBuf, 0, 0, &x1, &y1, &w, &h);
  display.setCursor((128 - w)/2, 12);
  display.print(tempBuf);

  // Description
  display.setTextSize(1);
  display.getTextBounds(gWeatherCtx.weather.description, 0, 0, &x1, &y1, &w, &h);
  display.setCursor((128 - w)/2, 34);
  display.print(gWeatherCtx.weather.description);

  // High / Low
  char hlBuf[32];
  snprintf(hlBuf, sizeof(hlBuf), "H:%.0f L:%.0f", gWeatherCtx.weather.highTemp, gWeatherCtx.weather.lowTemp);
  display.setCursor(0, 56);
  display.print(hlBuf);

  display.display();
}