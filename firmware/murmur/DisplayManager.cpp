#include "DisplayManager.h"

DisplayManager::DisplayManager(uint8_t sdaPin, uint8_t sclPin,
                               uint8_t oledAddr,
                               uint8_t screenW, uint8_t screenH)
  : _display(screenW, screenH, &Wire, -1),
    _sdaPin(sdaPin), _sclPin(sclPin),
    _addr(oledAddr), _w(screenW), _h(screenH)
{}

bool DisplayManager::begin() {
  Wire.begin(_sdaPin, _sclPin);
  if (!_display.begin(SSD1306_SWITCHCAPVCC, _addr)) {
    return false;
  }
  _u8g2.begin(_display);
  showLogoScroll();
  return true;
}

void DisplayManager::showLogoScroll() {
  // The logo is wider than the screen; scroll it from left to right so the
  // full graphic passes through.  drawBitmap() with a negative x clips to the
  // display bounds — GFX handles it correctly without extra buffer work.
  int maxOffset = (int)MURMUR_LOGO_WIDTH - (int)_w;
  int logoY     = ((int)_h - (int)MURMUR_LOGO_HEIGHT) / 2;
  if (logoY < 0) logoY = 0;

  // Show the left edge first, hold briefly.
  _display.clearDisplay();
  _display.drawBitmap(0, logoY, MURMUR_LOGO_BITMAP,
                      MURMUR_LOGO_WIDTH, MURMUR_LOGO_HEIGHT, SSD1306_WHITE);
  _display.display();
  delay(LOGO_HOLD_MS);

  // Scroll through the rest of the logo.
  for (int offset = LOGO_STEP_PX; offset <= maxOffset; offset += LOGO_STEP_PX) {
    _display.clearDisplay();
    _display.drawBitmap(-offset, logoY, MURMUR_LOGO_BITMAP,
                        MURMUR_LOGO_WIDTH, MURMUR_LOGO_HEIGHT, SSD1306_WHITE);
    _display.display();
    delay(LOGO_STEP_MS);
  }

  // Ensure we land exactly on the last frame, then hold.
  _display.clearDisplay();
  _display.drawBitmap(-maxOffset, logoY, MURMUR_LOGO_BITMAP,
                      MURMUR_LOGO_WIDTH, MURMUR_LOGO_HEIGHT, SSD1306_WHITE);
  _display.display();
  delay(LOGO_HOLD_MS);
}

// ── showTrack ─────────────────────────────────────────────────────────────────
void DisplayManager::showTrack(const String& trackName, int index, int total,
                               bool playing, const String& playlistName,
                               const String& chapterName) {
  _trackName      = trackName;
  _playlistName   = playlistName;
  _chapterName    = chapterName;
  _trackIndex     = index;
  _trackTotal     = total;
  _isPlaying      = playing;
  _onTrackScreen  = true;

  // Reset scroll to start with an initial hold
  _scrollOffset   = 0;
  _scrollPhase    = HOLD_START;
  _scrollUntil    = millis() + SCROLL_HOLD_MS;

  // Measure once so update() and _redrawTrack() share the same value.
  _u8g2.setFont(u8g2_font_helvB14_tr);
  String stripped = _stripExtension(trackName);
  _textPixelW = (int)_u8g2.getUTF8Width(stripped.c_str());

  _redrawTrack();
}

// ── update ────────────────────────────────────────────────────────────────────
void DisplayManager::update() {
  if (!_displayOn) return;
  if (!_onTrackScreen) return;
  if (_textPixelW <= (int)_w) return;  // fits — nothing to scroll

  uint32_t now = millis();

  switch (_scrollPhase) {

    case HOLD_START:
      if (now >= _scrollUntil) {
        _scrollPhase    = SCROLLING;
        _lastScrollTick = now;
      }
      break;

    case SCROLLING:
      if (now - _lastScrollTick >= SCROLL_STEP_MS) {
        _lastScrollTick = now;
        _scrollOffset  += SCROLL_STEP_PX;
        int maxOffset   = _textPixelW - (int)_w;
        if (_scrollOffset >= maxOffset) {
          _scrollOffset = maxOffset;
          _scrollPhase  = HOLD_END;
          _scrollUntil  = now + SCROLL_HOLD_MS;
        }
        _redrawTrack();
      }
      break;

    case HOLD_END:
      if (now >= _scrollUntil) {
        _scrollOffset = 0;
        _scrollPhase  = HOLD_START;
        _scrollUntil  = now + SCROLL_HOLD_MS;
        _redrawTrack();  // snap back to start
      }
      break;
  }
}

// ── _redrawTrack ──────────────────────────────────────────────────────────────
void DisplayManager::_redrawTrack() {
  _display.clearDisplay();

  // ── Play / pause icon (top-left) ───────────────────────────────────────────
  if (_isPlaying) {
    _drawPlayIcon(0, 3);
  } else {
    _drawPauseIcon(0, 3);
  }

  // ── Track counter (right of icon, helvR08) ────────────────────────────────
  // helvR08 ascent ≈ 6 px; icons at y=3 → baseline = 3 + 6 = 9.
  String counter = String(_trackIndex + 1) + "/" + String(_trackTotal);
  _u8g2.setFont(u8g2_font_helvR08_tr);
  _u8g2.setForegroundColor(SSD1306_WHITE);
  _u8g2.setCursor(12, 9);
  _u8g2.print(counter);

  // ── Volume bar (top-right of status bar, ~26 % of display width) ──────────
  // Outline: x=94..127 (34 px wide), y=4..10 (7 px tall); fill tracks _volume.
  _display.drawRect(94, 4, 34, 7, SSD1306_WHITE);
  int fillW = (int)(_volume * 32.0f);
  if (fillW > 32) fillW = 32;
  if (fillW > 0)
    _display.fillRect(95, 5, fillW, 5, SSD1306_WHITE);

  // ── Thin divider under header ──────────────────────────────────────────────
  _display.drawFastHLine(0, 13, _w, SSD1306_WHITE);

  // ── Track name (helvB14, single scrolling line) ────────────────────────────
  // With bottom label: zone is y=14..48; baseline ≈ 36.
  // Without bottom label: zone is y=14..63; baseline at 44 (original).
  const bool hasBottomLabel = !_chapterName.isEmpty() || !_playlistName.isEmpty();
  const int16_t TEXT_Y      = hasBottomLabel ? 36 : 44;

  String name = _stripExtension(_trackName);
  _u8g2.setFont(u8g2_font_helvB14_tr);
  _u8g2.setForegroundColor(SSD1306_WHITE);
  if (_textPixelW <= (int)_w) {
    int16_t tx = ((int16_t)_w - (int16_t)_textPixelW) / 2;
    _u8g2.setCursor(tx, TEXT_Y);
  } else {
    _u8g2.setCursor((int16_t)(-_scrollOffset), TEXT_Y);
  }
  _u8g2.print(name);

  // ── Bottom label (helvR08, bottom strip) ─────────────────────────────
  // Chapter name takes priority over playlist name when both exist.
  const String& bottomLabel = !_chapterName.isEmpty() ? _chapterName : _playlistName;
  if (!bottomLabel.isEmpty()) {
    _display.drawFastHLine(0, 50, _w, SSD1306_WHITE);
    _u8g2.setFont(u8g2_font_helvR08_tr);
    int16_t pw = (int16_t)_u8g2.getUTF8Width(bottomLabel.c_str());
    _u8g2.setCursor(((int16_t)_w - pw) / 2, 61);
    _u8g2.print(bottomLabel.c_str());
  }

  // Throttle I2C pushes: skip if too soon since last push.
  uint32_t now = millis();
  if (now - _lastRedrawMs >= REDRAW_MIN_MS) {
    _display.display();
    _lastRedrawMs = now;
  }
}

// ── Icons ─────────────────────────────────────────────────────────────────────

// Right-pointing filled triangle; tip-to-left-edge width ≈ 9 px, height 9 px
void DisplayManager::_drawPlayIcon(int x, int y) {
  _display.fillTriangle(
    x,     y,      // top-left
    x,     y + 8,  // bottom-left
    x + 8, y + 4,  // mid-right (apex)
    SSD1306_WHITE);
}

// Two vertical bars; total footprint ≈ 9 × 9 px
void DisplayManager::_drawPauseIcon(int x, int y) {
  _display.fillRect(x,     y, 3, 9, SSD1306_WHITE);  // left bar
  _display.fillRect(x + 5, y, 3, 9, SSD1306_WHITE);  // right bar
}

// ── showVolume ────────────────────────────────────────────────────────────────
void DisplayManager::showVolume(float volume) {
  _volume = volume;
  if (_onTrackScreen) _redrawTrack();
}

// ── _stripExtension ───────────────────────────────────────────────────────────
String DisplayManager::_stripExtension(const String& filename) {
  int dot = filename.lastIndexOf('.');
  if (dot > 0) return filename.substring(0, dot);
  return filename;
}

// ── turnOff ───────────────────────────────────────────────────────────────────
void DisplayManager::turnOff() {
  _display.clearDisplay();
  _display.display();
  _display.ssd1306_command(SSD1306_DISPLAYOFF);
  _displayOn = false;
}

// ── turnOn ────────────────────────────────────────────────────────────────────
void DisplayManager::turnOn() {
  _display.ssd1306_command(SSD1306_DISPLAYON);
  _displayOn = true;
}

// ── showLowBattery ─────────────────────────────────────────────────────────────
void DisplayManager::showLowBattery(bool charging) {
  _display.clearDisplay();
  _onTrackScreen = false;

  // ─── Battery outline (68 × 28 px, centred horizontally, upper area) ────────────
  static constexpr int16_t BX = 26, BY = 8, BW = 68, BH = 28;
  _display.drawRect(BX, BY, BW, BH, SSD1306_WHITE);
  // Terminal nub on the right side, vertically centred on the body.
  _display.fillRect(BX + BW, BY + 10, 5, 8, SSD1306_WHITE);

  // ─── Lightning bolt (charging indicator) ───────────────────────────────────
  // Two stacked parallelograms slanting down-left, centred in the battery body.
  if (charging) {
    // Upper bar: (61,12)–(67,12) at top  →  (54,23)–(60,23) at bottom
    _display.fillTriangle(61, 12, 67, 12, 54, 23, SSD1306_WHITE);
    _display.fillTriangle(67, 12, 54, 23, 60, 23, SSD1306_WHITE);
    // Lower bar: (60,21)–(67,21) at top  →  (53,32)–(59,32) at bottom
    _display.fillTriangle(60, 21, 67, 21, 53, 32, SSD1306_WHITE);
    _display.fillTriangle(67, 21, 53, 32, 59, 32, SSD1306_WHITE);
  }

  // ─── Labels ──────────────────────────────────────────────────────────────
  _u8g2.setFont(u8g2_font_helvR08_tr);
  _u8g2.setForegroundColor(SSD1306_WHITE);

  if (charging) {
  const char* line1 = "Charging.";
  int16_t w1 = (int16_t)_u8g2.getUTF8Width(line1);
  _u8g2.setCursor(((int16_t)_w - w1) / 2, charging ? 49 : 52);
  _u8g2.print(line1);
  } else {
    const char* line1 = "Low Battery";
    int16_t w1 = (int16_t)_u8g2.getUTF8Width(line1);
    _u8g2.setCursor(((int16_t)_w - w1) / 2, charging ? 49 : 52);
    _u8g2.print(line1);
  }

  _display.display();
}

// ── showMenu ──────────────────────────────────────────────────────────────────
// Renders a 4-item settings menu. selectedItem = 0..3 (EQ, Timer, Repeat, Exit).
// Selected row is indicated with a ">" cursor; no inversion used.
void DisplayManager::showMenu(int selectedItem, EqMode eq, TimerMode timer, RepeatMode repeat) {
  _onTrackScreen = false;
  _display.clearDisplay();

  // ── Header (tightened to 11 px to fit 4 rows below) ─────────────────────────
  _u8g2.setFont(u8g2_font_helvR08_tr);
  _u8g2.setForegroundColor(SSD1306_WHITE);
  const char* title = "SETTINGS";
  int16_t titleW = (int16_t)_u8g2.getUTF8Width(title);
  _u8g2.setCursor(((int16_t)_w - titleW) / 2, 10);
  _u8g2.print(title);
  _display.drawFastHLine(0, 11, _w, SSD1306_WHITE);

  // ── Menu rows ────────────────────────────────────────────────────────────────
  // 4 rows of 13 px each starting at y=12 → 12 + 4*13 = 64, fits exactly.
  static const char* labels[]       = { "EQ", "Timer", "Repeat", "Exit" };
  static const char* eqLabels[]     = { "Loud", "Bass Boost", "Flat" };
  static const char* timerLabels[]  = { "Off", "30 min", "60 min" };
  static const char* repeatLabels[] = { "All", "One", "Off" };

  const char* values[4] = {
    eqLabels[(int)eq],
    timerLabels[(int)timer],
    repeatLabels[(int)repeat],
    nullptr   // Exit has no value
  };

  static constexpr int ROW_Y      = 12;
  static constexpr int ROW_H      = 13;
  static constexpr int TEXT_OFF_Y = 10;  // baseline: 2 px pad + 8 px ascent

  for (int i = 0; i < 4; i++) {
    int16_t ry = ROW_Y + i * ROW_H;

    // ">" marks the selected row
    if (i == selectedItem) {
      _u8g2.setCursor(2, ry + TEXT_OFF_Y);
      _u8g2.print('>');
    }

    // Label — indented past the indicator column
    _u8g2.setCursor(12, ry + TEXT_OFF_Y);
    _u8g2.print(labels[i]);

    // Value — right-aligned (Exit row has none)
    if (values[i] != nullptr) {
      int16_t vw = (int16_t)_u8g2.getUTF8Width(values[i]);
      _u8g2.setCursor((int16_t)_w - vw - 4, ry + TEXT_OFF_Y);
      _u8g2.print(values[i]);
    }
  }

  _display.display();
}

// ── showPowerOff ──────────────────────────────────────────────────────────────
void DisplayManager::showPowerOff() {
  _display.clearDisplay();

  _u8g2.setFont(u8g2_font_helvB14_tr);
  _u8g2.setForegroundColor(SSD1306_WHITE);

  const char* msg = "Sleeping...";
  int16_t textW = (int16_t)_u8g2.getUTF8Width(msg);
  int16_t x = ((int16_t)_w - textW) / 2;
  // helvB14 ascent ~10 px; place baseline so text is vertically centred
  int16_t y = (int16_t)(_h / 2) + 5;
  _u8g2.setCursor(x, y);
  _u8g2.print(msg);

  _display.display();
  _onTrackScreen = false;
}

// ── showPlaylistMenu ───────────────────────────────────────────────────────────
// Renders the playlist navigator.
// selectedIndex : 0 = "All Songs", 1..N = folders[0..N-1]
// folders       : folder names from AudioPlayer::getFolders()
//
// Layout mirrors showMenu(): 9-px header + divider, then 4 rows of 13 px each.
// A 4-item scroll window is centred on the selected item so the list scrolls
// smoothly regardless of how many folders are present.
void DisplayManager::showPlaylistMenu(int selectedIndex,
                                      const std::vector<String>& folders) {
  _onTrackScreen = false;
  _display.clearDisplay();

  _u8g2.setFont(u8g2_font_helvR08_tr);
  _u8g2.setForegroundColor(SSD1306_WHITE);

  // ── Header ────────────────────────────────────────────────────────────────
  const char* title = "Select Playlist";
  int16_t titleW = (int16_t)_u8g2.getUTF8Width(title);
  _u8g2.setCursor(((int16_t)_w - titleW) / 2, 9);
  _u8g2.print(title);
  _display.drawFastHLine(0, 11, _w, SSD1306_WHITE);

  // ── Scroll window ─────────────────────────────────────────────────────────
  // Total items: 0 = All Songs, 1..N = folders.
  int totalItems   = 1 + (int)folders.size();
  // Keep selected item visible at position 1 (with 1 item above when possible).
  int visibleStart = selectedIndex - 1;
  if (visibleStart < 0) visibleStart = 0;
  if (visibleStart > totalItems - 4) visibleStart = max(0, totalItems - 4);

  static constexpr int ROW_Y    = 12;
  static constexpr int ROW_H    = 13;
  static constexpr int TEXT_OFF = 10;  // baseline within row: 2 px pad + 8 px ascent

  for (int slot = 0; slot < 4; slot++) {
    int itemIndex = visibleStart + slot;
    if (itemIndex >= totalItems) break;

    int16_t ry = ROW_Y + slot * ROW_H;

    if (itemIndex == selectedIndex) {
      _u8g2.setCursor(2, ry + TEXT_OFF);
      _u8g2.print('>');
    }

    _u8g2.setCursor(12, ry + TEXT_OFF);
    if (itemIndex == 0) {
      _u8g2.print("All Songs");
    } else {
      _u8g2.print(folders[itemIndex - 1].c_str());
    }
  }

  _display.display();
}

// ── showChapterMenu ──────────────────────────────────────────────────────────
// Drum-roll / slot-machine picker: selected chapter is large and centered,
// with smaller previous/next chapters above and below.
void DisplayManager::showChapterMenu(int selectedIndex,
                                     const std::vector<ChapterInfo>& chapters) {
  _onTrackScreen = false;
  _display.clearDisplay();
  _u8g2.setForegroundColor(SSD1306_WHITE);

  // ── Header ────────────────────────────────────────────────────────────────
  _u8g2.setFont(u8g2_font_helvR08_tr);
  const char* title = "Chapters";
  int16_t titleW = (int16_t)_u8g2.getUTF8Width(title);
  _u8g2.setCursor(((int16_t)_w - titleW) / 2, 9);
  _u8g2.print(title);
  _display.drawFastHLine(0, 11, _w, SSD1306_WHITE);

  int total = (int)chapters.size();

  // ── Previous chapter (small, indented right) ──────────────────────────────
  if (selectedIndex > 0) {
    _u8g2.setFont(u8g2_font_helvR08_tr);
    _u8g2.setCursor(20, 23);
    _u8g2.print(chapters[selectedIndex - 1].title.c_str());
  }

  // ── Selected chapter (large, with caret) ──────────────────────────────────
  _u8g2.setFont(u8g2_font_helvB14_tr);
  _u8g2.setCursor(2, 40);
  _u8g2.print(">");
  _u8g2.setCursor(16, 40);
  _u8g2.print(chapters[selectedIndex].title.c_str());

  // ── Next chapter (small, indented right) ──────────────────────────────────
  if (selectedIndex < total - 1) {
    _u8g2.setFont(u8g2_font_helvR08_tr);
    _u8g2.setCursor(20, 55);
    _u8g2.print(chapters[selectedIndex + 1].title.c_str());
  }

  _display.display();
}
