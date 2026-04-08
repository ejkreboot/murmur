#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <vector>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <U8g2_for_Adafruit_GFX.h>
#include "logo_bitmap.h"
#include "config.h"
#include "ID3Parser.h"

class DisplayManager {
public:
  DisplayManager(uint8_t sdaPin, uint8_t sclPin,
                 uint8_t oledAddr,
                 uint8_t screenW, uint8_t screenH);

  bool begin();

  // Show currently playing/paused track.
  // trackName    : filename (shown without extension; scrolls if too wide)
  // index        : 0-based current track index
  // total        : total track count
  // playing      : true = show play icon, false = show pause icon
  // playlistName : active folder name; empty string = All Songs (no label shown)
  void showTrack(const String& trackName, int index, int total, bool playing,
                 const String& playlistName = "",
                 const String& chapterName = "");

  // Advance scroll animation — call every loop() when no overlay is active.
  void update();

  // Update the volume bar shown in the status bar (call on volume change).
  void showVolume(float volume);   // 0.0 – 1.0

  // Show the settings menu. selectedItem is 0=EQ, 1=Timer, 2=Repeat.
  // Call whenever the selection or any value changes.
  void showMenu(int selectedItem, EqMode eq, TimerMode timer, RepeatMode repeat);

  // Show the playlist navigator.
  // selectedIndex : 0 = All Songs, 1..N = folders[0..N-1]
  // folders       : list of folder names from AudioPlayer::getFolders()
  void showPlaylistMenu(int selectedIndex, const std::vector<String>& folders);

  // Show the chapter navigator for audiobooks.
  // selectedIndex : 0..N-1 = chapter index
  // chapters      : chapter list from AudioPlayer::getChapters()
  void showChapterMenu(int selectedIndex, const std::vector<ChapterInfo>& chapters);

  // Power off the OLED: clear framebuffer, flush, then send DISPLAYOFF command.
  void turnOff();

  // Power the OLED back on (DISPLAYON). Pre-draw content before calling this
  // so the display comes up with the correct frame already in OLED RAM.
  void turnOn();

  bool isOff() const { return !_displayOn; }

  // Show sleep message centred on screen.
  // Call before turnOff() / deep sleep.
  void showPowerOff();

  // Show a low-battery screen. Pass charging=true to overlay a lightning bolt
  // and "Charging..." sub-label when USB power is present.
  void showLowBattery(bool charging);

  // Scroll the startup logo bitmap across the display, then return.
  // Adafruit_SSD1306 pushes its entire framebuffer atomically on display(),
  // so each frame is already double-buffered (no visible tearing).
  void showLogoScroll();

private:
  Adafruit_SSD1306      _display;
  U8G2_FOR_ADAFRUIT_GFX _u8g2;
  uint8_t _sdaPin, _sclPin;
  uint8_t _addr;
  uint8_t _w, _h;

  // ── Track screen state ────────────────────────────────────────────────────
  String   _trackName;
  String   _playlistName;    // empty = All Songs (no label)
  String   _chapterName;     // empty = no chapter label
  int      _trackIndex    = 0;
  int      _trackTotal    = 0;
  bool     _isPlaying     = true;
  float    _volume       = 0.5f;
  bool     _onTrackScreen = false;
  bool     _displayOn    = true;
  uint32_t _lastRedrawMs = 0;        // throttle: millis() of last I2C push
  static constexpr uint32_t REDRAW_MIN_MS = 30;  // minimum interval between I2C pushes

  // ── Scroll state ──────────────────────────────────────────────────────────
  enum ScrollPhase { HOLD_START, SCROLLING, HOLD_END };
  ScrollPhase _scrollPhase    = HOLD_START;
  int         _scrollOffset   = 0;    // current pixel offset (0 = leftmost)
  int         _textPixelW     = 0;    // measured track name width in pixels
  uint32_t    _scrollUntil    = 0;    // millis() deadline for hold phases
  uint32_t    _lastScrollTick = 0;

  static constexpr uint32_t SCROLL_HOLD_MS  = 1500; // pause at start/end
  static constexpr uint32_t SCROLL_STEP_MS  = 50;   // ms per step
  static constexpr int      SCROLL_STEP_PX  = 2;    // pixels per step

  // ── Logo scroll constants ────────────────────────────────────────────────
  static constexpr uint32_t LOGO_HOLD_MS   = 600;  // pause at each end
  static constexpr uint32_t LOGO_STEP_MS   = 20;   // ms between scroll steps (×2 speed)
  static constexpr int      LOGO_STEP_PX   = 2;    // pixels per step

  // ── Internal helpers ─────────────────────────────────────────────────────
  void   _redrawTrack();
  void   _drawPlayIcon(int x, int y);   // right-pointing triangle
  void   _drawPauseIcon(int x, int y);  // two vertical bars
  String _stripExtension(const String& filename);
};
