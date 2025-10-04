#pragma once
#include <Adafruit_SSD1306.h>
#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>

// Stock ticker symbols to display
#define STOCK_COUNT 1
#define CHART_POINTS 60  // Number of price points for chart (60 seconds = 1 minute)
extern const char* STOCK_SYMBOLS[STOCK_COUNT];

// Stock data structure
struct StockData {
  char symbol[8];
  float price;
  float change;
  bool valid;
  unsigned long lastUpdate;
  float chartData[CHART_POINTS];  // Historical prices for chart
  int chartIndex;                 // Current position in circular buffer
  float dayHigh;                  // Day's high price
  float dayLow;                   // Day's low price
  float dayStartPrice;            // Starting price for the day (for change calculation)
};

// Stock app context
struct StockAppContext {
  StockData stocks[STOCK_COUNT];
  int currentIndex = 0;
  unsigned long lastFetchTime = 0;
  const unsigned long FETCH_INTERVAL_MS = 2000; // 2 seconds to reduce blocking
  bool fetching = false;
};

extern StockAppContext gStockCtx;

// API functions
void stocksInit();
void stocksUpdate();
bool fetchStockData();

// Display function
void drawStocks(Adafruit_SSD1306 &display);
void drawPriceChart(Adafruit_SSD1306 &display, int stockIndex, int x, int y, int width, int height);