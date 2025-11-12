#include "CST816S.h"

CST816SDriver::CST816SDriver()
    : wire_(nullptr), sdaPin_(-1), sclPin_(-1), rstPin_(-1), intPin_(-1),
      activeAddress_(CST816_ADDR), mode_(CST816_MODE_POINT), i2cReady_(false),
      loggedFailure_(false), lastResetMs_(0) {}

bool CST816SDriver::begin(TwoWire &wire, int sdaPin, int sclPin, int rstPin, int intPin, uint8_t mode) {
  wire_ = &wire;
  sdaPin_ = sdaPin;
  sclPin_ = sclPin;
  rstPin_ = rstPin;
  intPin_ = intPin;
  mode_ = mode;

  // Configure INT pin if provided
  if (intPin_ >= 0) {
    pinMode(intPin_, INPUT);
  }

  // Configure and perform reset
  if (rstPin_ >= 0) {
    pinMode(rstPin_, OUTPUT);
    reset();
    delay(50);  // Wait for reset to complete
  } else {
    // No hardware reset available - CST816S should be held high by pull-up
    Serial.println("CST816T: No hardware reset pin, waiting for chip boot...");
    delay(100);  // Give chip more time to boot after power-on
    
    // Try to wake the chip by writing to the wake register
    Serial.println("CST816T: Attempting software wake-up...");
    writeRegister(0xFE, 0x01);  // Disable auto-sleep / wake up
    delay(50);
  }

  // I2C should already be initialized by caller with proper settings
  // Don't reconfigure I2C here to avoid conflicts
  
  // Try to detect chip with retries
  int attempts = 0;
  const int MAX_ATTEMPTS = 5;  // More attempts
  bool detected = false;
  
  Serial.println("CST816T: Attempting to detect chip...");
  while (attempts < MAX_ATTEMPTS && !detected) {
    attempts++;
    Serial.printf("CST816T: Detection attempt %d/%d\n", attempts, MAX_ATTEMPTS);
    if (whoAmI()) {
      detected = true;
      break;
    }
    if (attempts < MAX_ATTEMPTS) {
      delay(100);  // Longer delay between retries
    }
  }
  
  if (!detected) {
    Serial.println("CST816T: Chip detection failed after retries");
    i2cReady_ = false;
    return false;
  }

  // Read and display firmware version
  uint8_t revision = readRevision();
  Serial.printf("CST816T: FW version 0x%02X\n", revision);

  // Disable auto-sleep to keep touch active
  stopAutoSleep();

  // Set touch mode
  setMode(mode_);

  i2cReady_ = true;
  loggedFailure_ = false;  // Reset failure flag
  return true;
}

void CST816SDriver::reset() {
  if (rstPin_ < 0) return;

  Serial.println("CST816T: Hardware reset");
  digitalWrite(rstPin_, LOW);
  delay(10);
  digitalWrite(rstPin_, HIGH);
  delay(50);
}

void CST816SDriver::wakeUp() {
  if (rstPin_ < 0) return;

  digitalWrite(rstPin_, LOW);
  delay(10);
  digitalWrite(rstPin_, HIGH);
  delay(50);

  // Send wake command
  writeRegister(0xFE, 0x01);
}

bool CST816SDriver::whoAmI() {
  uint8_t chipId;
  Serial.printf("CST816T: Reading chip ID from register 0x%02X...\n", CST816_REG_CHIP_ID);
  if (readRegister(CST816_REG_CHIP_ID, chipId)) {
    Serial.printf("CST816T: Read chip ID: 0x%02X\n", chipId);
    if (chipId == 0xB5) {  // CST816T chip ID
      Serial.println("CST816T: ✓ Chip ID matches (0xB5)");
      return true;
    }
    // Some variants might have different IDs but still work
    Serial.printf("CST816T: Note - Chip ID 0x%02X (expected 0xB5)\n", chipId);
    if (chipId != 0x00 && chipId != 0xFF) {
      Serial.println("CST816T: ✓ Accepting non-standard chip ID");
      return true;  // Accept non-default values
    } else {
      Serial.println("CST816T: ✗ Invalid chip ID (0x00 or 0xFF indicates no response)");
      return false;
    }
  }
  Serial.println("CST816T: ✗ Failed to read chip ID register");
  return false;
}

uint8_t CST816SDriver::readRevision() {
  uint8_t revision = 0;
  readRegister(CST816_REG_FW_VER, revision);
  return revision;
}

uint8_t CST816SDriver::readChipID() {
  uint8_t chipId = 0;
  readRegister(CST816_REG_CHIP_ID, chipId);
  return chipId;
}

void CST816SDriver::stopAutoSleep() {
  // Disable automatic sleep mode
  writeRegister(CST816_REG_DIS_AUTO_SLEEP, 0x01);
}

void CST816SDriver::setMode(uint8_t mode) {
  mode_ = mode;

  if (mode == CST816_MODE_POINT) {
    // Point mode - fast touch detection
    writeRegister(CST816_REG_IRQ_CTL, CST816_IRQ_EN_TOUCH);
    writeRegister(CST816_REG_NOR_SCAN_PER, 0x01);  // 10ms scan period
    writeRegister(CST816_REG_IRQ_PULSE_WIDTH, 0x0F);  // 1.5ms interrupt pulse
  } else if (mode == CST816_MODE_MIXED) {
    // Mixed mode - touch and gesture (including double-click)
    writeRegister(CST816_REG_IRQ_CTL, CST816_IRQ_EN_TOUCH | CST816_IRQ_EN_MOTION);
    writeRegister(CST816_REG_NOR_SCAN_PER, 0x01);  // 10ms scan period
    writeRegister(CST816_REG_IRQ_PULSE_WIDTH, 0x0F);  // 1.5ms interrupt pulse
    // Enable all gestures
    writeRegister(CST816_REG_MOTION_MASK, 0x07);  // Enable all motion detection (bit 0,1,2)
  } else {
    // Gesture mode
    writeRegister(CST816_REG_IRQ_CTL, CST816_IRQ_EN_MOTION);
    writeRegister(CST816_REG_NOR_SCAN_PER, 0x01);
    writeRegister(CST816_REG_IRQ_PULSE_WIDTH, 0x01);
    // Enable all gestures
    writeRegister(CST816_REG_MOTION_MASK, 0x07);  // Enable all motion detection (bit 0,1,2)
  }
}

bool CST816SDriver::dataReady() const {
  if (intPin_ < 0) return true;  // No INT pin, assume ready
  return digitalRead(intPin_) == LOW;  // INT is active low
}

bool CST816SDriver::readTouch(CST816SRawPoint &point) {
  if (!i2cReady_) return false;

  uint8_t data[7];
  if (!readRegisters(CST816_REG_GESTURE, data, 7)) {
    return false;
  }

  // Parse gesture
  point.gesture = data[0];

  // Parse finger count (0 = no touch, 1 = touch)
  uint8_t fingerNum = data[1];
  point.touching = (fingerNum > 0);

  // Parse coordinates (12-bit values)
  point.x = ((data[2] & 0x0F) << 8) | data[3];
  point.y = ((data[4] & 0x0F) << 8) | data[5];

  return true;
}

bool CST816SDriver::readRaw(uint16_t &rawX, uint16_t &rawY, bool &touching) {
  CST816SRawPoint point;
  if (readTouch(point)) {
    rawX = point.x;
    rawY = point.y;
    touching = point.touching;
    return true;
  }
  return false;
}

void CST816SDriver::resetState() {
  // Nothing specific needed for reset state
}

bool CST816SDriver::readRegister(uint8_t reg, uint8_t &value) {
  wire_->beginTransmission(activeAddress_);
  wire_->write(reg);
  if (wire_->endTransmission(false) != 0) {
    return false;
  }

  if (wire_->requestFrom(activeAddress_, (uint8_t)1) != 1) {
    return false;
  }

  value = wire_->read();
  return true;
}

bool CST816SDriver::readRegisters(uint8_t reg, uint8_t *buffer, size_t length) {
  // Try reading with retry logic for better reliability
  const int MAX_RETRIES = 2;
  
  for (int attempt = 0; attempt < MAX_RETRIES; attempt++) {
    wire_->beginTransmission(activeAddress_);
    wire_->write(reg);
    
    if (wire_->endTransmission(false) != 0) {
      if (attempt == MAX_RETRIES - 1 && !loggedFailure_) {
        Serial.println("CST816T: I2C write failed after retries");
        loggedFailure_ = true;
      }
      delayMicroseconds(100);  // Brief delay before retry
      continue;
    }

    size_t received = wire_->requestFrom(activeAddress_, (uint8_t)length);
    if (received != length) {
      if (attempt == MAX_RETRIES - 1 && !loggedFailure_) {
        Serial.printf("CST816T: I2C read failed (got %d, expected %d)\n", received, length);
        loggedFailure_ = true;
      }
      delayMicroseconds(100);  // Brief delay before retry
      continue;
    }

    // Success - read the data
    for (size_t i = 0; i < length; i++) {
      buffer[i] = wire_->read();
    }
    
    loggedFailure_ = false;  // Reset failure flag on success
    return true;
  }
  
  return false;  // All retries failed
}

bool CST816SDriver::writeRegister(uint8_t reg, uint8_t value) {
  wire_->beginTransmission(activeAddress_);
  wire_->write(reg);
  wire_->write(value);
  return (wire_->endTransmission() == 0);
}

void CST816SDriver::resetBus() {
  unsigned long now = millis();
  if (now - lastResetMs_ < 50) {
    return;  // Rate limit resets
  }
  lastResetMs_ = now;

  Serial.println("CST816T: Resetting I2C bus");
  wire_->end();
  delay(10);
  wire_->begin(sdaPin_, sclPin_);
  wire_->setClock(400000);
  wire_->setTimeout(50);

  if (rstPin_ >= 0) {
    reset();
  }

  delay(50);
  i2cReady_ = whoAmI();
}
