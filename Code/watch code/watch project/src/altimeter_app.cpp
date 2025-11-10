#include "altimeter_app.h"
#include "app_manager.h"
#include <Adafruit_BMP280.h>
#include <Wire.h>

namespace {
bool altimeterNeedsRedraw = true;
Adafruit_BMP280 bmp; // I2C
bool sensorInitialized = false;
float temperature = 0.0;
float pressure = 0.0;
float altitude = 0.0;
unsigned long lastUpdate = 0;
const unsigned long UPDATE_INTERVAL = 1000; // Update every 1 second
}

void resetAltimeterDisplay(Gc9Display &display) {
  altimeterNeedsRedraw = true;
  display.fillScreen(COLOR_BLACK);
}

void updateAltimeter() {
  unsigned long now = millis();
  if (now - lastUpdate < UPDATE_INTERVAL) {
    return;
  }
  lastUpdate = now;

  // Initialize sensor on first update if not already done
  if (!sensorInitialized) {
    Serial.println(F("[Altimeter] Initializing BMP280..."));
    
    // Scan I2C bus to see what's there
    Serial.println(F("[Altimeter] Scanning I2C bus..."));
    byte error, address;
    int nDevices = 0;
    for(address = 1; address < 127; address++) {
      Wire.beginTransmission(address);
      error = Wire.endTransmission();
      if (error == 0) {
        Serial.printf("[Altimeter] I2C device found at 0x%02X\n", address);
        nDevices++;
      }
    }
    if (nDevices == 0) {
      Serial.println(F("[Altimeter] No I2C devices found!"));
    } else {
      Serial.printf("[Altimeter] Found %d I2C device(s)\n", nDevices);
    }
    
    // Try 0x76 first (SD0 connected to GND)
    Serial.println(F("[Altimeter] Trying BMP280 at 0x76..."));
    if (bmp.begin(0x76)) {
      Serial.println(F("[Altimeter] BMP280 initialized at 0x76"));
      sensorInitialized = true;
      
      // Configure sensor
      bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,     // Operating Mode
                      Adafruit_BMP280::SAMPLING_X2,     // Temp. oversampling
                      Adafruit_BMP280::SAMPLING_X16,    // Pressure oversampling
                      Adafruit_BMP280::FILTER_X16,      // Filtering
                      Adafruit_BMP280::STANDBY_MS_500); // Standby time
    } else {
      // Try 0x77 (SD0 connected to VCC)
      Serial.println(F("[Altimeter] Failed at 0x76, trying 0x77..."));
      if (bmp.begin(0x77)) {
        Serial.println(F("[Altimeter] BMP280 initialized at 0x77"));
        sensorInitialized = true;
        
        // Configure sensor
        bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,     // Operating Mode
                        Adafruit_BMP280::SAMPLING_X2,     // Temp. oversampling
                        Adafruit_BMP280::SAMPLING_X16,    // Pressure oversampling
                        Adafruit_BMP280::FILTER_X16,      // Filtering
                        Adafruit_BMP280::STANDBY_MS_500); // Standby time
      } else {
        Serial.println(F("[Altimeter] ERROR: Could not find BMP280 at 0x76 or 0x77"));
        return;
      }
    }
  }

  if (sensorInitialized) {
    temperature = bmp.readTemperature();
    pressure = bmp.readPressure() / 100.0F; // Convert to hPa
    altitude = bmp.readAltitude(1013.25); // Sea level pressure in hPa
    altimeterNeedsRedraw = true;
  }
}

void drawAltimeter(Gc9Display &display) {
  if (!altimeterNeedsRedraw) {
    return;
  }

  display.fillScreen(COLOR_BLACK);
  display.setTextColor(COLOR_WHITE);

  // Title
  display.setTextSize(2);
  const char *title = "Altimeter";
  int16_t x1, y1; uint16_t w, h;
  display.getTextBounds(title, 0, 0, &x1, &y1, &w, &h);
  int16_t x = (display.width() - static_cast<int16_t>(w)) / 2;
  display.setCursor(x, 20);
  display.print(title);

  if (!sensorInitialized) {
    // Show error message
    display.setTextSize(1);
    display.setTextColor(COLOR_RED);
    const char *error = "Sensor Error";
    display.getTextBounds(error, 0, 0, &x1, &y1, &w, &h);
    x = (display.width() - static_cast<int16_t>(w)) / 2;
    display.setCursor(x, 100);
    display.print(error);
    
    const char *error2 = "Check I2C 0x76";
    display.getTextBounds(error2, 0, 0, &x1, &y1, &w, &h);
    x = (display.width() - static_cast<int16_t>(w)) / 2;
    display.setCursor(x, 120);
    display.print(error2);
  } else {
    // Display altitude
    display.setTextSize(3);
    display.setTextColor(COLOR_CYAN);
    char altStr[16];
    snprintf(altStr, sizeof(altStr), "%.1f m", altitude);
    display.getTextBounds(altStr, 0, 0, &x1, &y1, &w, &h);
    x = (display.width() - static_cast<int16_t>(w)) / 2;
    display.setCursor(x, 70);
    display.print(altStr);

    // Display pressure
    display.setTextSize(2);
    display.setTextColor(COLOR_GREEN);
    char pressStr[16];
    snprintf(pressStr, sizeof(pressStr), "%.1f hPa", pressure);
    display.getTextBounds(pressStr, 0, 0, &x1, &y1, &w, &h);
    x = (display.width() - static_cast<int16_t>(w)) / 2;
    display.setCursor(x, 130);
    display.print(pressStr);

    // Display temperature
    display.setTextSize(2);
    display.setTextColor(COLOR_ORANGE);
    char tempStr[16];
    snprintf(tempStr, sizeof(tempStr), "%.1f C", temperature);
    display.getTextBounds(tempStr, 0, 0, &x1, &y1, &w, &h);
    x = (display.width() - static_cast<int16_t>(w)) / 2;
    display.setCursor(x, 170);
    display.print(tempStr);
  }

  display.display();
  altimeterNeedsRedraw = false;
}

void registerAltimeterApp(AppManager& appManager) {
  App altimeterApp = {
    "Altimeter",           // name
    drawAltimeter,         // drawFunction
    updateAltimeter,       // updateFunction (reads sensor)
    nullptr,               // buttonHandler (not needed)
    false,                 // isSpecial (not the clock app)
    resetAltimeterDisplay  // resetFunction
  };
  
  appManager.registerApp(altimeterApp);
}
