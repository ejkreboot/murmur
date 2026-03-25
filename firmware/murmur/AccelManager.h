#pragma once
#include <Arduino.h>
#include <Adafruit_LIS3DH.h>
#include <Adafruit_Sensor.h>
#include "config.h"

// LIS3DH accelerometer — 6D orientation-based display sleep/wake.
//
// INT1 (hardware) fires after the device has been horizontal for ≥2 seconds,
// using the LIS3DH's built-in 6D position recognition engine and duration
// filter.  Wake detection is done by polling isFlat() from the main loop.
class AccelManager {
public:
  AccelManager(uint8_t int1Pin, uint8_t int2Pin);

  // Initialize the LIS3DH and configure 6D flat-detection interrupt.
  // Wire must already be initialised before calling begin().
  // Returns false if the sensor is not found on the bus.
  bool begin();

  // Returns true (once) when the flat interrupt has fired.
  // Clears the hardware interrupt latch by reading INT1_SRC.
  bool consumeFlatEvent();

  // Discard any queued flat event and release the hardware latch.
  // Call this when waking the display so a stale flag cannot cause an
  // immediate re-sleep.
  void clearFlatEvent();

  // Poll current orientation; returns true if the device is flat (Z dominant).
  // Safe to call from loop() — performs one I2C register read.
  bool isFlat();

private:
  Adafruit_LIS3DH _lis;
  uint8_t _int1Pin, _int2Pin;
  uint8_t _i2cAddr = LIS3DH_ADDR;

  static volatile bool _flatEventFlag;
  static void IRAM_ATTR _isrFlat();
};
