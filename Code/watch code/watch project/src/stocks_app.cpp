#include "stocks_app.h"
#include <time.h>

// Predefined stock symbols to track
const char* STOCK_SYMBOLS[STOCK_COUNT] = {"QQQ"};

StockAppContext gStockCtx;

void stocksInit() {
  // Initialize stock data
  for (int i = 0; i < STOCK_COUNT; i++) {
    // Clear the symbol array first
    memset(gStockCtx.stocks[i].symbol, 0, sizeof(gStockCtx.stocks[i].symbol));
    
    // Copy the symbol using strcpy instead of strncpy
    strcpy(gStockCtx.stocks[i].symbol, STOCK_SYMBOLS[i]);
    
    gStockCtx.stocks[i].price = 0.0;
    gStockCtx.stocks[i].change = 0.0;
    gStockCtx.stocks[i].valid = false;
    gStockCtx.stocks[i].lastUpdate = 0;
    gStockCtx.stocks[i].chartIndex = 0;
    gStockCtx.stocks[i].dayHigh = 0.0;
    gStockCtx.stocks[i].dayLow = 999999.0;
    gStockCtx.stocks[i].dayStartPrice = 0.0;
    
    // Initialize chart data array
    for (int j = 0; j < CHART_POINTS; j++) {
      gStockCtx.stocks[i].chartData[j] = 0.0;
    }
    
    // Debug output
    Serial.printf("Initialized stock %d: symbol='%s' (len=%d)\n", 
      i, gStockCtx.stocks[i].symbol, strlen(gStockCtx.stocks[i].symbol));
    Serial.printf("Source symbol: '%s'\n", STOCK_SYMBOLS[i]);
  }
}

void stocksUpdate() {
  unsigned long now = millis();
  if (!gStockCtx.fetching && (now - gStockCtx.lastFetchTime > gStockCtx.FETCH_INTERVAL_MS)) {
    if (WiFi.status() == WL_CONNECTED) {
      gStockCtx.fetching = true;
      if (fetchStockData()) {
        gStockCtx.lastFetchTime = now;
      }
      gStockCtx.fetching = false;
    }
  }
}

bool fetchStockData() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected - skipping stock update");
    return false;
  }
  
  HTTPClient http;
  WiFiClientSecure client;
  
  // Skip SSL certificate verification for simplicity
  client.setInsecure();
  
  // Use Yahoo Finance API (free, no key required)
  // Format: https://query1.finance.yahoo.com/v8/finance/chart/SYMBOL
  
  for (int i = 0; i < STOCK_COUNT; i++) {
    String url = "https://query1.finance.yahoo.com/v8/finance/chart/";
    url += STOCK_SYMBOLS[i];
    
    Serial.printf("Fetching %s from %s\n", STOCK_SYMBOLS[i], url.c_str());
    
    http.begin(client, url);
    http.addHeader("User-Agent", "Mozilla/5.0 (ESP32)");
  http.setTimeout(1500); // shorten timeout to improve UI responsiveness
    
    int httpResponseCode = http.GET();
    
    if (httpResponseCode == 200) {
      String payload = http.getString();
      Serial.printf("Response length: %d\n", payload.length());
      
      JsonDocument doc;
      DeserializationError error = deserializeJson(doc, payload);
      
      if (!error) {
        // Parse Yahoo Finance JSON response using proper ArduinoJson v7 syntax
        if (doc["chart"]["result"][0]["meta"]["regularMarketPrice"].is<float>() &&
            doc["chart"]["result"][0]["meta"]["previousClose"].is<float>()) {
          
          float currentPrice = doc["chart"]["result"][0]["meta"]["regularMarketPrice"];
          float previousClose = doc["chart"]["result"][0]["meta"]["previousClose"];
          
          if (currentPrice > 0 && previousClose > 0) {
            gStockCtx.stocks[i].price = currentPrice;
            gStockCtx.stocks[i].change = currentPrice - previousClose;
            gStockCtx.stocks[i].valid = true;
            gStockCtx.stocks[i].lastUpdate = millis();
            
            // Set day start price on first valid update
            if (gStockCtx.stocks[i].dayStartPrice == 0.0) {
              gStockCtx.stocks[i].dayStartPrice = currentPrice;
            }
            
            // Update chart data (circular buffer) - store price change from day start
            float dayChange = currentPrice - gStockCtx.stocks[i].dayStartPrice;
            gStockCtx.stocks[i].chartData[gStockCtx.stocks[i].chartIndex] = dayChange;
            gStockCtx.stocks[i].chartIndex = (gStockCtx.stocks[i].chartIndex + 1) % CHART_POINTS;
            
            // Update day high/low
            if (currentPrice > gStockCtx.stocks[i].dayHigh) {
              gStockCtx.stocks[i].dayHigh = currentPrice;
            }
            if (currentPrice < gStockCtx.stocks[i].dayLow) {
              gStockCtx.stocks[i].dayLow = currentPrice;
            }
            
            Serial.printf("Updated %s: $%.2f (%.2f) dayChange: %.2f\n", 
              STOCK_SYMBOLS[i], currentPrice, gStockCtx.stocks[i].change, dayChange);
          }
        } else {
          Serial.printf("Price data not found for %s\n", STOCK_SYMBOLS[i]);
        }
      } else {
        Serial.printf("JSON parse error for %s: %s\n", STOCK_SYMBOLS[i], error.c_str());
      }
    } else {
      Serial.printf("HTTP error for %s: %d\n", STOCK_SYMBOLS[i], httpResponseCode);
      // Try fallback to HTTP (non-secure) if HTTPS fails
      if (httpResponseCode < 0) {
        http.end();
        WiFiClient plainClient;
        String httpUrl = "http://query1.finance.yahoo.com/v8/finance/chart/";
        httpUrl += STOCK_SYMBOLS[i];
        
        Serial.printf("Trying HTTP fallback for %s\n", STOCK_SYMBOLS[i]);
        http.begin(plainClient, httpUrl);
        http.addHeader("User-Agent", "Mozilla/5.0 (ESP32)");
        int fallbackCode = http.GET();
        Serial.printf("HTTP fallback result: %d\n", fallbackCode);
      }
    }
    
    http.end();
    // delay(200); // Removed blocking delay to allow button responsiveness
  }
  
  return true;
}

void drawStocks(Adafruit_SSD1306 &display) {
  stocksUpdate();
  
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  
  // Draw current time in top right (small text)
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    char timeStr[10];
    strftime(timeStr, sizeof(timeStr), "%I:%M", &timeinfo);
    display.setTextSize(1);
    int16_t x1, y1; uint16_t w, h;
    display.getTextBounds(timeStr, 0, 0, &x1, &y1, &w, &h);
    display.setCursor(128 - w, 0);
    display.print(timeStr);
  }
  
  // Title
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println(F("Stocks"));
  display.drawLine(0, 10, 90, 10, SSD1306_WHITE); // Shorter line to not overlap time
  
  // Display stock data
  for (int i = 0; i < STOCK_COUNT; i++) {
    if (gStockCtx.stocks[i].valid) {
      // Debug output
      Serial.printf("Drawing stock %d: symbol='%s', price=%.2f\n", 
        i, gStockCtx.stocks[i].symbol, gStockCtx.stocks[i].price);
      
      // Symbol and price on one line - hard-code QQQ for testing
      display.setTextSize(1);
      display.setCursor(0, 14);
      
      // Try hard-coded symbol first to isolate the issue
      display.print("QQQ");
      display.print(": $");
      display.print(gStockCtx.stocks[i].price, 2);
      
      // Change on second line
      display.setTextSize(1);
      display.setCursor(0, 24);
      display.print("Change: ");
      if (gStockCtx.stocks[i].change >= 0) {
        display.print("+");
      }
      display.print(gStockCtx.stocks[i].change, 2);
      
      // Draw price chart
      drawPriceChart(display, i, 0, 35, 128, 25);
      
    } else {
      // Loading state
      display.setTextSize(1);
      display.setCursor(0, 14);
      display.printf("%s: Loading...", gStockCtx.stocks[i].symbol);
    }
  }
  
  display.display();
}

void drawPriceChart(Adafruit_SSD1306 &display, int stockIndex, int x, int y, int width, int height) {
  StockData &stock = gStockCtx.stocks[stockIndex];
  
  // Find the range of day changes in the chart data
  float minChange = 0.0;
  float maxChange = 0.0;
  int validPoints = 0;
  
  // Count valid points and find min/max from the circular buffer
  for (int i = 0; i < CHART_POINTS; i++) {
    int dataIndex = (stock.chartIndex + i) % CHART_POINTS;
    if (stock.chartData[dataIndex] != 0.0 || validPoints > 0) {
      float change = stock.chartData[dataIndex];
      if (validPoints == 0) {
        minChange = maxChange = change;
      } else {
        if (change < minChange) minChange = change;
        if (change > maxChange) maxChange = change;
      }
      validPoints++;
    }
  }
  
  // Ensure we have some range and zero is included
  if (validPoints > 0) {
    float absMax = max(abs(minChange), abs(maxChange));
    if (absMax < 0.5) absMax = 0.5; // Minimum range of $1
    minChange = -absMax;
    maxChange = absMax;
  } else {
    minChange = -1.0;
    maxChange = 1.0;
  }
  
  float changeRange = maxChange - minChange;
  
  // Draw chart border
  display.drawRect(x, y, width, height, SSD1306_WHITE);
  
  // Plot price change points - scrolling left to right
  if (validPoints > 1) {
    int plotWidth = width - 2;
    int plotHeight = height - 2;
    
    // Draw line chart - read from circular buffer in chronological order
    int prevX = -1, prevY = -1;
    int pointsToShow = min(validPoints, CHART_POINTS);
    
    for (int i = 0; i < pointsToShow; i++) {
      // Calculate the data index - start from oldest data
      int dataIndex = (stock.chartIndex + CHART_POINTS - pointsToShow + i) % CHART_POINTS;
      float change = stock.chartData[dataIndex];
      
      // Calculate position - spread across chart width
      int chartX = x + 1 + (i * plotWidth) / (pointsToShow - 1);
      int chartY = y + 1 + plotHeight/2 - ((change - 0) * plotHeight/2) / (maxChange);
      
      // Clamp Y to chart bounds
      chartY = max(y + 1, min(y + plotHeight, chartY));
      
      if (prevX >= 0 && prevY >= 0) {
        display.drawLine(prevX, prevY, chartX, chartY, SSD1306_WHITE);
      }
      
      prevX = chartX;
      prevY = chartY;
    }
  } else {
    // Not enough data - show message
    display.setTextSize(1);
    display.setCursor(x + 2, y + height/2 - 4);
    display.print("Building chart...");
  }
}