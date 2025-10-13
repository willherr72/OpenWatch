#pragma once
#include <Arduino.h>
#include <Wire.h>

// CST816T Register Definitions
#define CST816_ADDR           0x15

// Register addresses
#define CST816_REG_GESTURE    0x01
#define CST816_REG_FINGER_NUM 0x02
#define CST816_REG_XPOS_H     0x03
#define CST816_REG_XPOS_L     0x04
#define CST816_REG_YPOS_H     0x05
#define CST816_REG_YPOS_L     0x06
#define CST816_REG_CHIP_ID    0xA7
#define CST816_REG_PROJ_ID    0xA8
#define CST816_REG_FW_VER     0xA9
#define CST816_REG_MOTION_MASK 0xEC
#define CST816_REG_IRQ_PULSE_WIDTH 0xED
#define CST816_REG_NOR_SCAN_PER 0xEE
#define CST816_REG_MOTION_SL_ANGLE 0xEF
#define CST816_REG_IRQ_CTL    0xFA
#define CST816_REG_DIS_AUTO_SLEEP 0xFE

// Gesture codes
#define CST816_GESTURE_NONE   0x00
#define CST816_GESTURE_UP     0x01
#define CST816_GESTURE_DOWN   0x02
#define CST816_GESTURE_LEFT   0x03
#define CST816_GESTURE_RIGHT  0x04
#define CST816_GESTURE_CLICK  0x05
#define CST816_GESTURE_DOUBLE_CLICK 0x0B
#define CST816_GESTURE_LONG_PRESS   0x0C

// Touch modes
#define CST816_MODE_GESTURE   0
#define CST816_MODE_POINT     1
#define CST816_MODE_MIXED     2

// IRQ Control bits
#define CST816_IRQ_EN_TEST    0x80
#define CST816_IRQ_EN_TOUCH   0x40
#define CST816_IRQ_EN_CHANGE  0x20
#define CST816_IRQ_EN_MOTION  0x10

// Motion mask bits
#define CST816_MOTION_EN_CON_LR    0x04
#define CST816_MOTION_EN_CON_UD    0x02
#define CST816_MOTION_EN_DCLICK    0x01

struct CST816SRawPoint {
  uint16_t x;
  uint16_t y;
  bool touching;
  uint8_t gesture;
};

class CST816SDriver {
public:
  CST816SDriver();
  
  // Initialize the touch driver
  bool begin(TwoWire &wire, int sdaPin, int sclPin, int rstPin = -1, int intPin = -1,
             uint8_t mode = CST816_MODE_POINT);
  
  // Read raw touch coordinates
  bool readRaw(uint16_t &rawX, uint16_t &rawY, bool &touching);
  
  // Read touch data with gesture
  bool readTouch(CST816SRawPoint &point);
  
  // Reset the touch controller
  void reset();
  
  // Wake up from sleep
  void wakeUp();
  
  // Check if touch data is ready (via INT pin)
  bool dataReady() const;
  
  // Get chip information
  bool whoAmI();
  uint8_t readRevision();
  uint8_t readChipID();
  
  // Configuration
  void setMode(uint8_t mode);
  void stopAutoSleep();
  
  // State management
  void resetState();
  bool isReady() const { return i2cReady_; }
  uint8_t activeAddress() const { return activeAddress_; }

private:
  bool readRegister(uint8_t reg, uint8_t &value);
  bool readRegisters(uint8_t reg, uint8_t *buffer, size_t length);
  bool writeRegister(uint8_t reg, uint8_t value);
  void resetBus();

  TwoWire *wire_;
  int sdaPin_;
  int sclPin_;
  int rstPin_;
  int intPin_;
  uint8_t activeAddress_;
  uint8_t mode_;
  bool i2cReady_;
  bool loggedFailure_;
  unsigned long lastResetMs_;
};
