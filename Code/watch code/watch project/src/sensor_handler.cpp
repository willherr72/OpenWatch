/**
 * @file sensor_handler.cpp
 * @brief Sensor management implementation for smartwatch
 */

#include "sensor_handler.h"
#include <Wire.h>
#include <WiFi.h>
#include <Adafruit_BMP280.h>
#include <Adafruit_BNO055.h>
#include <Adafruit_Sensor.h>
#include <MAX30105.h>
#include <TinyGPSPlus.h>
#include <HardwareSerial.h>

/* Sensor objects */
static Adafruit_BMP280 bmp280;
static Adafruit_BNO055 bno055 = Adafruit_BNO055(55);
static MAX30105 max30102;
static TinyGPSPlus gps;
static HardwareSerial gpsSerial(1);  // Use UART1

/* Sensor availability flags */
static bool bmp280_available = false;
static bool bno055_available = false;
static bool max30102_available = false;
static bool gps_available = false;

/* Sensor data storage */
static BMP280Data bmp280_data = {0, 0, false};
static BNO055Data bno055_data = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, false};
static MAX30102Data max30102_data = {0, 0, false};
static GPSData gps_data = {0, 0, 0, 0, 0, false};

/* Streaming state */
static bool streaming_enabled = false;

/* Update tracking */
static unsigned long last_sensor_update = 0;
static const unsigned long SENSOR_UPDATE_INTERVAL = 200;  // 200ms

/* Heart rate calculation variables */
static const int HR_SAMPLE_SIZE = 4;
static long hr_values[HR_SAMPLE_SIZE];
static int hr_value_count = 0;
static byte hr_sample_counter = 0;

/**
 * @brief Initialize BMP280 sensor
 */
static bool init_bmp280() {
    Serial.println(F("[Sensors] Initializing BMP280..."));
    
    if (bmp280.begin(ALTM_I2C_ADDR)) {
        /* Configure BMP280 */
        bmp280.setSampling(Adafruit_BMP280::MODE_NORMAL,     /* Operating Mode */
                          Adafruit_BMP280::SAMPLING_X2,      /* Temp. oversampling */
                          Adafruit_BMP280::SAMPLING_X16,     /* Pressure oversampling */
                          Adafruit_BMP280::FILTER_X16,       /* Filtering */
                          Adafruit_BMP280::STANDBY_MS_500);  /* Standby time */
        
        Serial.println(F("[Sensors] ✓ BMP280 initialized"));
        return true;
    } else {
        Serial.println(F("[Sensors] ✗ BMP280 not found"));
        return false;
    }
}

/**
 * @brief Initialize BNO055 sensor
 */
static bool init_bno055() {
    Serial.println(F("[Sensors] Initializing BNO055..."));
    
    if (bno055.begin()) {
        delay(50);
        
        /* Set to NDOF mode (9-axis fusion) */
        bno055.setMode(OPERATION_MODE_NDOF);
        delay(20);
        
        /* Use external crystal for better accuracy */
        bno055.setExtCrystalUse(true);
        
        Serial.println(F("[Sensors] ✓ BNO055 initialized in NDOF mode"));
        return true;
    } else {
        Serial.println(F("[Sensors] ✗ BNO055 not found"));
        return false;
    }
}

/**
 * @brief Initialize MAX30102 sensor
 */
static bool init_max30102() {
    Serial.println(F("[Sensors] Initializing MAX30102..."));
    
    if (max30102.begin(Wire, I2C_SPEED_STANDARD, MAX30102_I2C_ADDR)) {
        /* Configure MAX30102 for heart rate and SpO2 */
        byte ledBrightness = 60;    // 0=Off to 255=50mA
        byte sampleAverage = 4;     // 1, 2, 4, 8, 16, 32
        byte ledMode = 2;           // 1 = Red only, 2 = Red + IR, 3 = Red + IR + Green
        byte sampleRate = 100;      // 50, 100, 200, 400, 800, 1000, 1600, 3200
        int pulseWidth = 411;       // 69, 118, 215, 411
        int adcRange = 4096;        // 2048, 4096, 8192, 16384
        
        max30102.setup(ledBrightness, sampleAverage, ledMode, sampleRate, pulseWidth, adcRange);
        
        Serial.println(F("[Sensors] ✓ MAX30102 initialized"));
        return true;
    } else {
        Serial.println(F("[Sensors] ✗ MAX30102 not found"));
        return false;
    }
}

/**
 * @brief Initialize GPS
 */
static bool init_gps() {
    Serial.println(F("[Sensors] Initializing GPS (Quectel LC76G)..."));
    
    /* Enable GPS */
    pinMode(GPS_ENABLE_PIN, OUTPUT);
    digitalWrite(GPS_ENABLE_PIN, HIGH);
    delay(100);
    
    /* Initialize UART for GPS */
    gpsSerial.begin(115200, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
    delay(100);
    
    Serial.println(F("[Sensors] ✓ GPS UART initialized"));
    Serial.println(F("[Sensors] GPS will acquire satellites (may take 30-60s)"));
    return true;
}

/**
 * @brief Read BMP280 data
 */
static void read_bmp280() {
    if (!bmp280_available) {
        bmp280_data.valid = false;
        return;
    }
    
    bmp280_data.temperature = bmp280.readTemperature();
    bmp280_data.pressure = bmp280.readPressure() / 100.0F;  // Convert Pa to hPa
    
    /* Validate readings */
    if (isnan(bmp280_data.temperature) || isnan(bmp280_data.pressure)) {
        bmp280_data.valid = false;
    } else {
        bmp280_data.valid = true;
    }
}

/**
 * @brief Read BNO055 data
 */
static void read_bno055() {
    if (!bno055_available) {
        bno055_data.valid = false;
        return;
    }
    
    /* Get raw sensor data - these are separate I2C transactions */
    imu::Vector<3> accel = bno055.getVector(Adafruit_BNO055::VECTOR_ACCELEROMETER);
    delay(2);  // Small delay between vector reads
    
    imu::Vector<3> gyro = bno055.getVector(Adafruit_BNO055::VECTOR_GYROSCOPE);
    delay(2);
    
    imu::Vector<3> mag = bno055.getVector(Adafruit_BNO055::VECTOR_MAGNETOMETER);
    delay(2);
    
    bno055_data.accel_x = accel.x();
    bno055_data.accel_y = accel.y();
    bno055_data.accel_z = accel.z();
    
    bno055_data.gyro_x = gyro.x();
    bno055_data.gyro_y = gyro.y();
    bno055_data.gyro_z = gyro.z();
    
    bno055_data.mag_x = mag.x();
    bno055_data.mag_y = mag.y();
    bno055_data.mag_z = mag.z();
    
    /* Get fused orientation data (Euler angles) */
    imu::Vector<3> euler = bno055.getVector(Adafruit_BNO055::VECTOR_EULER);
    
    bno055_data.yaw = euler.x();      // Azimuth (0-360 degrees)
    bno055_data.pitch = euler.y();    // Inclination (-180 to 180 degrees)
    bno055_data.roll = euler.z();     // Toolface (-90 to 90 degrees)
    
    /* Check if we're getting non-zero readings (sensor is responding) */
    bool hasData = (accel.x() != 0 || accel.y() != 0 || accel.z() != 0 ||
                    gyro.x() != 0 || gyro.y() != 0 || gyro.z() != 0);
    
    /* Mark as valid if sensor is responding, even if not fully calibrated */
    bno055_data.valid = hasData;
}

/**
 * @brief Read MAX30102 data with averaging
 */
static void read_max30102() {
    if (!max30102_available) {
        max30102_data.valid = false;
        return;
    }
    
    long irValue = max30102.getIR();
    long redValue = max30102.getRed();
    
    /* Check if finger is detected */
    if (irValue < 50000) {
        max30102_data.valid = false;
        max30102_data.heartRate = 0;
        max30102_data.spo2 = 0;
        hr_value_count = 0;
        return;
    }
    
    /* Simple beat detection using derivative of IR signal */
    static long lastIR = 0;
    static long lastLastIR = 0;
    static unsigned long lastBeatTime = 0;
    
    /* Detect peak: current value higher than previous and next */
    if (lastIR > lastLastIR && lastIR > irValue) {
        /* Peak detected */
        unsigned long now = millis();
        long delta = now - lastBeatTime;
        
        if (delta > 300 && delta < 2000) {  // Reasonable heart beat interval (30-200 BPM)
            float beatsPerMinute = 60000.0 / delta;
            
            if (beatsPerMinute > 30 && beatsPerMinute < 200) {  // Valid heart rate range
                hr_values[hr_value_count % HR_SAMPLE_SIZE] = (long)beatsPerMinute;
                hr_value_count++;
                
                /* Calculate average after collecting samples */
                if (hr_value_count >= HR_SAMPLE_SIZE) {
                    long avgBPM = 0;
                    for (int i = 0; i < HR_SAMPLE_SIZE; i++) {
                        avgBPM += hr_values[i];
                    }
                    avgBPM /= HR_SAMPLE_SIZE;
                    
                    max30102_data.heartRate = avgBPM;
                    max30102_data.valid = true;
                }
            }
        }
        lastBeatTime = now;
    }
    
    lastLastIR = lastIR;
    lastIR = irValue;
    
    /* SpO2 calculation - simplified estimation based on IR/Red ratio */
    if (redValue > 0 && irValue > 0 && max30102_data.valid) {
        float ratio = (float)redValue / (float)irValue;
        /* Simplified SpO2 estimation (not medically accurate) */
        max30102_data.spo2 = (int)(110 - 25 * ratio);
        
        /* Clamp to realistic range */
        if (max30102_data.spo2 < 80) max30102_data.spo2 = 80;
        if (max30102_data.spo2 > 100) max30102_data.spo2 = 100;
    }
    
    hr_sample_counter++;
}

/**
 * @brief Read GPS data
 */
static void read_gps() {
    /* Feed GPS parser with available data */
    static int bytesReceived = 0;
    while (gpsSerial.available() > 0) {
        char c = gpsSerial.read();
        gps.encode(c);
        bytesReceived++;
    }
    
    /* Update GPS data if we have a valid fix */
    if (gps.location.isValid()) {
        gps_data.latitude = gps.location.lat();
        gps_data.longitude = gps.location.lng();
        gps_data.altitude = gps.altitude.meters();
        gps_data.speed = gps.speed.kmph();
        gps_data.satellites = gps.satellites.value();
        gps_data.valid = true;
        gps_available = true;
    } else {
        /* No fix yet, but store satellite count if available */
        gps_data.satellites = gps.satellites.value();
        gps_data.valid = false;
        
        /* Mark GPS as receiving data if we've gotten bytes */
        if (bytesReceived > 0) {
            gps_available = true;
        }
    }
}

/**
 * @brief Initialize all sensors
 */
bool sensor_handler_init() {
    Serial.println(F("=== Sensor Initialization ==="));
    
    /* Initialize each sensor and track availability */
    bmp280_available = init_bmp280();
    delay(50);
    
    bno055_available = init_bno055();
    delay(50);
    
    max30102_available = init_max30102();
    delay(50);
    
    gps_available = init_gps();
    delay(50);
    
    /* Report results */
    int sensor_count = 0;
    if (bmp280_available) sensor_count++;
    if (bno055_available) sensor_count++;
    if (max30102_available) sensor_count++;
    if (gps_available) sensor_count++;
    
    Serial.printf("[Sensors] Initialized %d/%d sensors\n", sensor_count, 4);
    Serial.println(F("=== Sensor Init Complete ==="));
    
    return sensor_count > 0;
}

/**
 * @brief Check if sensor streaming is enabled
 */
bool sensor_handler_is_streaming() {
    return streaming_enabled;
}

/**
 * @brief Enable or disable sensor streaming
 */
void sensor_handler_set_streaming(bool enabled) {
    streaming_enabled = enabled;
    if (enabled) {
        Serial.println(F("\n[Sensors] === STREAMING ENABLED ==="));
    } else {
        Serial.println(F("\n[Sensors] === STREAMING DISABLED ==="));
    }
}

/**
 * @brief Update sensor readings (call periodically)
 */
void sensor_handler_update() {
    unsigned long now = millis();
    
    /* Rate limit updates to 200ms intervals */
    if (now - last_sensor_update < SENSOR_UPDATE_INTERVAL) {
        /* Still feed GPS parser continuously */
        if (gps_available) {
            while (gpsSerial.available() > 0) {
                gps.encode(gpsSerial.read());
            }
        }
        return;
    }
    
    last_sensor_update = now;
    
    /* Read all available sensors with spacing to avoid I2C bus contention */
    if (bmp280_available) {
        read_bmp280();
        delay(5);  // Small delay between I2C transactions
    }
    
    if (bno055_available) {
        read_bno055();
        delay(10);  // Larger delay after BNO055 (multiple I2C reads)
    }
    
    if (max30102_available) {
        read_max30102();
        delay(5);
    }
    
    if (gps_available) {
        read_gps();  // UART, no I2C
    }
    
    /* Print data if streaming is enabled */
    if (streaming_enabled) {
        sensor_handler_print_data();
    }
}

/**
 * @brief Get BMP280 data
 */
BMP280Data sensor_handler_get_bmp280() {
    return bmp280_data;
}

/**
 * @brief Get BNO055 data
 */
BNO055Data sensor_handler_get_bno055() {
    return bno055_data;
}

/**
 * @brief Get MAX30102 data
 */
MAX30102Data sensor_handler_get_max30102() {
    return max30102_data;
}

/**
 * @brief Get GPS data
 */
GPSData sensor_handler_get_gps() {
    return gps_data;
}

/**
 * @brief Print all sensor data to serial console
 */
void sensor_handler_print_data() {
    Serial.println(F("\n=== SENSOR DATA ==="));
    
    /* BMP280 */
    Serial.print(F("BMP280: "));
    if (bmp280_available && bmp280_data.valid) {
        Serial.printf("Temp: %.2f°C, Pressure: %.2f hPa\n", 
                     bmp280_data.temperature, bmp280_data.pressure);
    } else {
        Serial.println(F("No data"));
    }
    
    /* BNO055 */
    Serial.print(F("BNO055: "));
    if (bno055_available && bno055_data.valid) {
        Serial.printf("\n  Accel: X=%.2f Y=%.2f Z=%.2f m/s²\n",
                     bno055_data.accel_x, bno055_data.accel_y, bno055_data.accel_z);
        Serial.printf("  Gyro:  X=%.2f Y=%.2f Z=%.2f rad/s\n",
                     bno055_data.gyro_x, bno055_data.gyro_y, bno055_data.gyro_z);
        Serial.printf("  Mag:   X=%.2f Y=%.2f Z=%.2f µT\n",
                     bno055_data.mag_x, bno055_data.mag_y, bno055_data.mag_z);
        Serial.printf("  Orient: Pitch=%.2f° (inc), Yaw=%.2f° (azm), Roll=%.2f° (toolface)\n",
                     bno055_data.pitch, bno055_data.yaw, bno055_data.roll);
    } else {
        Serial.println(F("No data"));
    }
    
    /* MAX30102 */
    Serial.print(F("MAX30102: "));
    if (max30102_available && max30102_data.valid) {
        Serial.printf("HR: %d BPM, SpO2: %d%%\n", 
                     max30102_data.heartRate, max30102_data.spo2);
    } else {
        Serial.println(F("No data"));
    }
    
    /* GPS */
    Serial.print(F("GPS: "));
    if (gps_available && gps_data.valid) {
        Serial.printf("Lat: %.6f, Lon: %.6f, Alt: %.1fm, Speed: %.1f km/h, Sats: %d\n",
                     gps_data.latitude, gps_data.longitude, gps_data.altitude,
                     gps_data.speed, gps_data.satellites);
    } else if (gps_available) {
        /* GPS is receiving data but no fix yet */
        if (gps.satellites.isValid() && gps_data.satellites > 0) {
            Serial.printf("Waiting for fix... Satellites: %d\n", gps_data.satellites);
        } else if (gps.charsProcessed() > 0) {
            Serial.println(F("Receiving data, searching for satellites..."));
        } else {
            Serial.println(F("Connected, waiting for NMEA data..."));
        }
    } else {
        Serial.println(F("No data"));
    }
    
    /* WiFi Signal Strength */
    Serial.print(F("WiFi: "));
    if (WiFi.status() == WL_CONNECTED) {
        int32_t rssi = WiFi.RSSI();
        Serial.printf("%d dBm", rssi);
        
        /* Add signal quality indicator */
        if (rssi > -50) {
            Serial.println(F(" (Excellent)"));
        } else if (rssi > -60) {
            Serial.println(F(" (Good)"));
        } else if (rssi > -70) {
            Serial.println(F(" (Fair)"));
        } else {
            Serial.println(F(" (Weak)"));
        }
    } else {
        Serial.println(F("Disabled"));
    }
    
    /* BLE Signal Strength (placeholder for future implementation) */
    Serial.print(F("BLE: "));
    Serial.println(F("Disabled"));
    
    Serial.println(F("==================\n"));
}

/**
 * @brief Check if BMP280 is available
 */
bool sensor_handler_bmp280_available() {
    return bmp280_available;
}

/**
 * @brief Check if BNO055 is available
 */
bool sensor_handler_bno055_available() {
    return bno055_available;
}

/**
 * @brief Check if MAX30102 is available
 */
bool sensor_handler_max30102_available() {
    return max30102_available;
}

/**
 * @brief Check if GPS is available
 */
bool sensor_handler_gps_available() {
    return gps_available;
}

