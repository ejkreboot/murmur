#include "AccelManager.h"
#include <Wire.h>
#include <math.h>

// LIS3DH register addresses not covered by the Adafruit library's public API.
static constexpr uint8_t REG_CTRL3    = 0x22;  // Interrupt routing to pins
static constexpr uint8_t REG_CTRL5    = 0x24;  // Latch / 4D control
static constexpr uint8_t REG_INT1_CFG = 0x30;  // Interrupt 1 configuration
static constexpr uint8_t REG_INT1_SRC = 0x31;  // Interrupt 1 source (read clears latch)
static constexpr uint8_t REG_INT1_THS = 0x32;  // Interrupt 1 threshold
static constexpr uint8_t REG_INT1_DUR = 0x33;  // Interrupt 1 duration

// The newer Adafruit_LIS3DH (BusIO-based) removed writeRegister8/readRegister8
// from the public API.  Use raw I2C via Wire instead.
// Raw I2C helpers — address is stored in AccelManager and passed in.
static void writeReg(uint8_t addr, uint8_t reg, uint8_t val) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  Wire.write(val);
  Wire.endTransmission();
}

static uint8_t readReg(uint8_t addr, uint8_t reg) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  Wire.endTransmission(false);
  Wire.requestFrom(addr, (uint8_t)1);
  return Wire.available() ? Wire.read() : 0;
}

volatile bool AccelManager::_flatEventFlag = false;

void IRAM_ATTR AccelManager::_isrFlat() {
  _flatEventFlag = true;
}

AccelManager::AccelManager(uint8_t int1Pin, uint8_t int2Pin)
  : _int1Pin(int1Pin), _int2Pin(int2Pin) {}

bool AccelManager::begin() {
  if (!_lis.begin(LIS3DH_ADDR)) {
    return false;
  }
  _i2cAddr = LIS3DH_ADDR;

  _lis.setRange(LIS3DH_RANGE_2_G);
  _lis.setDataRate(LIS3DH_DATARATE_10_HZ);  // 1 duration tick = 100 ms

  // CTRL_REG5: latch INT1 asserted until INT1_SRC is read.
  writeReg(_i2cAddr, REG_CTRL5, 0x08);

  // INT1_CFG: 6D mode (bit 6), Z-high / face-up (bit 5), Z-low / face-down (bit 4).
  // Fires when the device settles into any horizontal orientation.
  writeReg(_i2cAddr, REG_INT1_CFG, 0x70);

  // INT1_THS: ~750 mg at ±2 g scale (1 LSB ≈ 15.6 mg → 0x30 = 48 × 15.6 mg).
  writeReg(_i2cAddr, REG_INT1_THS, 0x30);

  // INT1_DURATION: 20 ticks × 100 ms/tick = 2 seconds of sustained orientation.
  writeReg(_i2cAddr, REG_INT1_DUR, 0x14);

  // CTRL_REG3: route IA1 (interrupt 1 activity) to the INT1 pin.
  writeReg(_i2cAddr, REG_CTRL3, 0x40);

  // Read back registers to confirm writes landed.
  uint8_t ctrl3  = readReg(_i2cAddr, REG_CTRL3);
  uint8_t ctrl5  = readReg(_i2cAddr, REG_CTRL5);
  uint8_t cfg    = readReg(_i2cAddr, REG_INT1_CFG);
  uint8_t ths    = readReg(_i2cAddr, REG_INT1_THS);
  uint8_t dur    = readReg(_i2cAddr, REG_INT1_DUR);

  // Clear any stale interrupt before attaching the ISR.
  readReg(_i2cAddr, REG_INT1_SRC);

  pinMode(_int1Pin, INPUT);
  attachInterrupt(digitalPinToInterrupt(_int1Pin), _isrFlat, RISING);

  return true;
}

bool AccelManager::consumeFlatEvent() {
  if (!_flatEventFlag) return false;
  _flatEventFlag = false;
  uint8_t src = readReg(_i2cAddr, REG_INT1_SRC);  // releases hardware latch
  return true;
}

void AccelManager::clearFlatEvent() {
  noInterrupts();
  _flatEventFlag = false;
  interrupts();
  readReg(_i2cAddr, REG_INT1_SRC);  // release latch
}

bool AccelManager::isFlat() {
  sensors_event_t event;
  _lis.getEvent(&event);
  float az = fabsf(event.acceleration.z);
  float ax = fabsf(event.acceleration.x);
  float ay = fabsf(event.acceleration.y);

  // Print live readings once per second.
  static uint32_t _lastPrintMs = 0;
  uint32_t now = millis();
  if (now - _lastPrintMs >= 1000) {
    _lastPrintMs = now;
    bool flat = az > 5.9f && az > ax * 1.5f && az > ay * 1.5f;
  }

  // Flat when Z exceeds ~0.6 g (5.9 m/s²) and is at least 1.5× larger than
  // both lateral axes, indicating the device is lying close to horizontal.
  return az > 5.9f && az > ax * 1.5f && az > ay * 1.5f;
}
