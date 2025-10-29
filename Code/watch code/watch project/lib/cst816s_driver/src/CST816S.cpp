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
  }

  // Initialize I2C
  Serial.printf("CST816T: Initializing I2C on SDA=%d, SCL=%d\n", sdaPin_, sclPin_);
  wire_->begin(sdaPin_, sclPin_);
  wire_->setClock(100000);  // 100kHz (slower but more reliable)
  wire_->setTimeout(100);    // 100ms timeout
  
  Serial.println("CST816T: I2C initialized, waiting for sensor...");
  delay(100);  // Allow touch controller to initialize

  // Verify chip is present
  Serial.println("CST816T: Attempting chip detection...");
  if (!whoAmI()) {
    Serial.println("CST816T: Chip detection failed - sensor not responding");
    return false;
  }

  // Read and display firmware version
  uint8_t revision = readRevision();
  Serial.printf("CST816T: Initialized, FW version: 0x%02X\n", revision);

  // Disable auto-sleep
  stopAutoSleep();

  // Set touch mode
  setMode(mode_);

  i2cReady_ = true;
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
  if (readRegister(CST816_REG_CHIP_ID, chipId)) {
    if (chipId == 0xB5) {  // CST816T chip ID
      Serial.println("CST816T: Chip detected successfully");
      return true;
    } else {
      Serial.printf("CST816T: Unexpected chip ID: 0x%02X (expected 0xB5)\n", chipId);
    }
  }
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
  wire_->beginTransmission(activeAddress_);
  wire_->write(reg);
  if (wire_->endTransmission(false) != 0) {
    if (!loggedFailure_) {
      Serial.println("CST816T: I2C write failed");
      loggedFailure_ = true;
    }
    return false;
  }

  size_t received = wire_->requestFrom(activeAddress_, (uint8_t)length);
  if (received != length) {
    if (!loggedFailure_) {
      Serial.printf("CST816T: I2C read failed (got %d, expected %d)\n", received, length);
      loggedFailure_ = true;
    }
    return false;
  }

  for (size_t i = 0; i < length; i++) {
    buffer[i] = wire_->read();
  }

  loggedFailure_ = false;  // Reset failure flag on success
  return true;
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
