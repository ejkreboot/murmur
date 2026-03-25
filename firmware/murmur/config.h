#pragma once

// ── I2S / DAC ─────────────────────────────────────────────────────────────────
#define I2S_DOUT       12
#define I2S_FSYNC      13
#define I2S_BCLK       14

// ── SD Card ───────────────────────────────────────────────────────────────────
#define SD_MISO        16
#define SD_MOSI        18
#define SD_SCK         17
#define SD_CS           8

// ── I2C / Display ─────────────────────────────────────────────────────────────
#define SDA_PIN         9
#define SCL_PIN        10
#define OLED_ADDR      0x3C
#define SCREEN_WIDTH   128
#define SCREEN_HEIGHT   64

// ── Buttons ───────────────────────────────────────────────────────────────────
#define BTN_NEXT        4
#define BTN_PREV        7
#define BTN_VUP         6
#define BTN_VDOWN      15
#define BTN_PLAY        5

// ── Charge management ─────────────────────────────────────────────────────────
#define CHG_USB_DETECT 38  // HIGH when USB power is present
#define CHG_STAT2      41
#define CHG_STAT1      42
// LBO (Low Battery Output) of MCP73871T is open-drain, routed to GPIO42.
// Goes LOW when V_BAT < 3.1 V; recovers HIGH after V_BAT > 3.25 V (hysteresis).
// Configure as INPUT_PULLUP on the ESP32.
#define CHG_LBO        42

// ── LIS3DH Accelerometer ──────────────────────────────────────────────────────
#define LIS3DH_ADDR    0x19   // SA0 pulled to GND via 100kΩ, but LIS3DH internal pull-up dominates → SA0 high
#define LIS3DH_INT1    40
#define LIS3DH_INT2    39

// ── App settings enums ────────────────────────────────────────────────────────
enum class EqMode     : uint8_t { LOUD = 0, BASS_BOOST, FLAT };
enum class TimerMode  : uint8_t { OFF  = 0, MIN30, MIN60 };
enum class RepeatMode : uint8_t { ALL  = 0, ONE, OFF };
