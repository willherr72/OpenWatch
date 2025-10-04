#pragma once
#include <Adafruit_SSD1306.h>
#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>

// Weather data structure
struct WeatherData {
  char location[32];
  char description[32];
  float temperature;
  float highTemp; // day's high (F)
  float lowTemp;  // day's low (F)
  float humidity;
  float pressure;
  int windSpeed;
  char windDirection[8];
  bool valid;
  unsigned long lastUpdate;
};

// Weather app context
struct WeatherAppContext {
  WeatherData weather;
  unsigned long lastFetchTime = 0;
  const unsigned long FETCH_INTERVAL_MS = 300000; // 5 minutes
  bool fetching = false;
};

extern WeatherAppContext gWeatherCtx;

// API functions
void weatherInit();
void weatherUpdate();
bool fetchWeatherData();

// Display function
void drawWeather(Adafruit_SSD1306 &display);