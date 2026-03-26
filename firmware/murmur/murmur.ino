//  murmur.ino
//  Murmur audio player — KorTech
//  Minimal first pass: SD track list → playback → OLED display → volume control

#include <Arduino.h>
#include <Preferences.h>
#include <esp_sleep.h>
#include <driver/rtc_io.h>
#include <WiFi.h>
#include "AudioTools.h"
#include "AudioTools/AudioCodecs/CodecMP3Helix.h"

#include "config.h"
#include "ButtonManager.h"
#include "DisplayManager.h"
#include "AudioPlayer.h"
#include "AccelManager.h"

// ── Module instances ──────────────────────────────────────────────────────────
ButtonManager buttons(BTN_NEXT, BTN_PREV, BTN_VUP, BTN_VDOWN, BTN_PLAY);
DisplayManager display(SDA_PIN, SCL_PIN, OLED_ADDR, SCREEN_WIDTH, SCREEN_HEIGHT);
MurmurPlayer  player(SD_CS, SD_SCK, SD_MISO, SD_MOSI, I2S_BCLK, I2S_FSYNC, I2S_DOUT);
AccelManager  accel(LIS3DH_INT1, LIS3DH_INT2);

// ── Preferences (NVS) ────────────────────────────────────────────────────────
static Preferences prefs;
static constexpr char  PREF_NS[]      = "murmur";
static constexpr char  PREF_VOL[]     = "volume";
static constexpr char  PREF_EQ[]      = "eq";
static constexpr char  PREF_TIMER[]   = "timer";
static constexpr char  PREF_REPEAT[]  = "repeat";
static constexpr char  PREF_BM_PATH[] = "bm_path";  // audiobook bookmark: track path
static constexpr char  PREF_BM_POS[]  = "bm_pos";   // audiobook bookmark: byte offset
static constexpr float DEFAULT_VOLUME = 0.3f;   // safe default on first boot

static constexpr float VOL_STEP = 0.05f;

// ── App state ─────────────────────────────────────────────────────────────────
enum class AppState { PLAYING, IN_MENU, IN_PLAYLIST, IN_CHAPTERS };
static AppState   appState        = AppState::PLAYING;
static int        menuItem        = 0;           // 0=EQ, 1=Timer, 2=Repeat
static int        playlistSel     = 0;           // 0=All Songs, 1..N=folder index
static int        chapterSel      = 0;           // chapter menu selection index
static EqMode     eqMode          = EqMode::FLAT;
static TimerMode  timerMode       = TimerMode::OFF;
static RepeatMode repeatMode      = RepeatMode::ALL;
static uint32_t   timerStartMs    = 0;         // millis() when timer was armed (0 = inactive)

// ── Display sleep state ──────────────────────────────────────────────────────────
static bool     displaySleeping     = false;
static uint32_t lastOrientPollMs    = 0;
static constexpr uint32_t ORIENT_POLL_MS = 100;  // poll rate while display is off
// ── Low battery state ─────────────────────────────────────────────────────────────────
static bool     lowBattery      = false;
static bool     lowBattCharging = false;
static int      lastChapterIdx  = -1;            // for detecting chapter boundary crossings
static uint32_t lastChapterCheckMs = 0;
static constexpr uint32_t CHAPTER_CHECK_MS = 1000;  // chapter update poll interval
// ── Helpers ───────────────────────────────────────────────────────────────────
void refreshTrackDisplay() {
  String chapterName = player.isAudiobook() ? player.getChapterName() : "";
  display.showTrack(player.currentName(),
                    player.currentIndex(),
                    player.trackCount(),
                    player.isPlaying(),
                    player.getPlaylistName(),
                    chapterName);
  lastChapterIdx = player.getCurrentChapterIndex();
}

void adjustVolume(float delta) {
  float v = player.getVolume() + delta;
  player.setVolume(v);
  prefs.putFloat(PREF_VOL, player.getVolume());  // persist immediately
  display.showVolume(player.getVolume());
}

// Save audiobook bookmark (current track path + byte offset) to NVS.
void saveBookmark() {
  if (!player.isAudiobook() || !player.isPlaying()) return;
  String path = player.currentName();
  size_t pos  = player.getFilePosition();
  prefs.putString(PREF_BM_PATH, path);
  prefs.putULong(PREF_BM_POS, (uint32_t)pos);
}

// ── setup ─────────────────────────────────────────────────────────────────────
void setup() {
  // ── Power: disable unused radios and lower CPU clock ──────────────────────
  // WiFi and BT controllers are powered by default; explicitly shut them down.
  // NOTE: do NOT enable automatic light sleep or DFS — both break I2S DMA.
  WiFi.mode(WIFI_OFF);        // ~20–80 mA saving depending on modem idle state
  btStop();                   // de-initialise BT controller
  setCpuFrequencyMhz(240);    // 240→160 MHz: safe for Helix MP3; APLL is independent

  delay(200);  // let USB serial settle

  // Display first so user sees something immediately
  if (!display.begin()) {
    while (true) delay(1000);
  }

  buttons.begin();

  if (!player.begin()) {
    // No SD card or no tracks — show message and poll for recovery.
    display.showTrack("No SD Card", 0, 0, false);
    uint32_t lastSdPoll = millis();
    while (true) {
      buttons.poll();
      if (buttons.sleepRequested()) {
        ESP.restart();
      }
      // Periodically retry SD mount — card may have been inserted.
      if (millis() - lastSdPoll >= 2000) {
        lastSdPoll = millis();
        if (SD.begin(SD_CS, SPI)) {
          ESP.restart();
        }
      }
      delay(10);
    }
  }

  // Restore last volume from NVS, falling back to a conservative default.
  prefs.begin(PREF_NS, false);
  player.setVolume(prefs.getFloat(PREF_VOL, DEFAULT_VOLUME));
  display.showVolume(player.getVolume());  // seed volume state before first draw

  // Restore settings from NVS.
  eqMode     = (EqMode)    prefs.getUChar(PREF_EQ,     (uint8_t)EqMode::FLAT);
  timerMode  = (TimerMode) prefs.getUChar(PREF_TIMER,  (uint8_t)TimerMode::OFF);
  repeatMode = (RepeatMode)prefs.getUChar(PREF_REPEAT, (uint8_t)RepeatMode::ALL);
  player.setEQ(eqMode);
  player.setRepeatMode(repeatMode);
  // Timer resets on every boot — do not restore timerStartMs.

  // Restore audiobook bookmark: if the current track matches the saved bookmark,
  // seek to the saved position so the user resumes where they left off.
  {
    String bmPath = prefs.getString(PREF_BM_PATH, "");
    uint32_t bmPos = prefs.getULong(PREF_BM_POS, 0);
    if (bmPath.length() > 0 && bmPos > 0 && player.currentName() == bmPath) {
      player.seekToPosition(bmPos);
    }
  }

  refreshTrackDisplay();

  if (!accel.begin()) {
  }

  // Configure battery monitor pins.
  // CHG_LBO is open-drain on MCP73871T — INPUT_PULLUP ensures a defined HIGH when battery is OK.
  pinMode(CHG_LBO,        INPUT_PULLUP);
  pinMode(CHG_USB_DETECT, INPUT);
}

// ── loop ──────────────────────────────────────────────────────────────────────
void loop() {
  // ── Low battery: monitor-only mode (replaces all normal operation) ───────────
  // Audio and SPI are already shut down; just refresh the USB indicator.
  // When LBO goes high the battery has recovered — full restart.
  if (lowBattery) {
    bool usb = (digitalRead(CHG_USB_DETECT) == HIGH);
    if (usb != lowBattCharging) {
      lowBattCharging = usb;
      display.showLowBattery(lowBattCharging);
    }
    if (digitalRead(CHG_LBO) == HIGH) {
      ESP.restart();
    }
    delay(50);
    return;
  }

  buttons.poll();

  // ── LBO transition: battery just went low ─────────────────────────────────
  if (digitalRead(CHG_LBO) == LOW) {
    lowBattery      = true;
    lowBattCharging = (digitalRead(CHG_USB_DETECT) == HIGH);
    player.shutdown();
    display.showLowBattery(lowBattCharging);
    return;
  }

  player.update();   // feeds audio pipeline

  // ── SD card removal detection ──────────────────────────────────────────────
  if (player.sdError()) {
    display.showTrack("SD Removed", 0, 0, false);
    // Poll for SD reinsertion; allow play-hold restart.
    while (true) {
      buttons.poll();
      if (buttons.sleepRequested()) {
        ESP.restart();
      }
      if (SD.begin(SD_CS, SPI)) {
        ESP.restart();
      }
      delay(500);
    }
  }

  // Check if playback stopped at end-of-playlist (RepeatMode::OFF)
  if (player.stoppedAtEnd()) {
    refreshTrackDisplay();
  }

  // ── Sleep timer ────────────────────────────────────────────────────────────
  if (timerMode != TimerMode::OFF && timerStartMs != 0) {
    uint32_t limitMs = (timerMode == TimerMode::MIN30) ? 30UL * 60000 : 60UL * 60000;
    if (millis() - timerStartMs >= limitMs) {
      if (player.isPlaying()) {
        player.togglePause();
        refreshTrackDisplay();
      }
      timerMode  = TimerMode::OFF;
      timerStartMs = 0;
      prefs.putUChar(PREF_TIMER, (uint8_t)TimerMode::OFF);
    }
  }

  // ── Button events ───────────────────────────────────────────────────────────
  // Double-tap always toggles the settings menu, regardless of current state.
  if (buttons.doubleTap()) {
    if (appState == AppState::PLAYING) {
      appState = AppState::IN_MENU;
      menuItem = 0;
      display.showMenu(menuItem, eqMode, timerMode, repeatMode);
    } else {
      appState = AppState::PLAYING;
      refreshTrackDisplay();
    }
  }

  // Long press on the Back (prev) button opens the playlist navigator.
  if (buttons.prevHeld() && appState == AppState::PLAYING) {
    appState    = AppState::IN_PLAYLIST;
    // Restore selection to the current active playlist.
    const String& active = player.getPlaylistName();
    if (active.isEmpty()) {
      playlistSel = 0;
    } else {
      playlistSel = 0;
      const auto& folders = player.getFolders();
      for (int i = 0; i < (int)folders.size(); i++) {
        if (folders[i] == active) { playlistSel = i + 1; break; }
      }
    }
    display.showPlaylistMenu(playlistSel, player.getFolders());
  }

  // Long press on the Next button opens the chapter navigator (audiobook only).
  if (buttons.nextHeld() && appState == AppState::PLAYING && player.isAudiobook()) {
    appState   = AppState::IN_CHAPTERS;
    chapterSel = max(0, player.getCurrentChapterIndex());
    display.showChapterMenu(chapterSel, player.getChapters());
  }

  if (appState == AppState::IN_MENU) {
    if (buttons.vdown()) {
      menuItem = (menuItem + 1) % 4;
      display.showMenu(menuItem, eqMode, timerMode, repeatMode);
    }
    if (buttons.vup()) {
      menuItem = (menuItem + 3) % 4;  // (i - 1 + 4) % 4
      display.showMenu(menuItem, eqMode, timerMode, repeatMode);
    }
    if (buttons.play()) {
      bool stayInMenu = true;
      switch (menuItem) {
        case 0:  // EQ: cycle Loud → Bass Boost → Flat → Loud
          eqMode = (EqMode)(((int)eqMode + 1) % 3);
          player.setEQ(eqMode);
          prefs.putUChar(PREF_EQ, (uint8_t)eqMode);
          break;
        case 1:  // Timer: cycle Off → 30 min → 60 min → Off
          timerMode = (TimerMode)(((int)timerMode + 1) % 3);
          timerStartMs = (timerMode != TimerMode::OFF) ? millis() : 0;
          prefs.putUChar(PREF_TIMER, (uint8_t)timerMode);
          break;
        case 2:  // Repeat: cycle All → One → Off → All
          repeatMode = (RepeatMode)(((int)repeatMode + 1) % 3);
          player.setRepeatMode(repeatMode);
          prefs.putUChar(PREF_REPEAT, (uint8_t)repeatMode);
          break;
        case 3:  // Exit
          appState = AppState::PLAYING;
          refreshTrackDisplay();
          stayInMenu = false;
          break;
      }
      if (stayInMenu) {
        display.showMenu(menuItem, eqMode, timerMode, repeatMode);
      }
    }
  } else if (appState == AppState::IN_PLAYLIST) {
    int totalItems = 1 + (int)player.getFolders().size();
    if (buttons.vdown()) {
      playlistSel = min(playlistSel + 1, totalItems - 1);
      display.showPlaylistMenu(playlistSel, player.getFolders());
    }
    if (buttons.vup()) {
      playlistSel = max(playlistSel - 1, 0);
      display.showPlaylistMenu(playlistSel, player.getFolders());
    }
    if (buttons.play()) {
      // Confirm selection: switch playlist and return to playing screen.
      saveBookmark();  // persist audiobook position before changing playlist
      if (playlistSel == 0) {
        player.setPlaylist("");  // All Songs
      } else {
        player.setPlaylist(player.getFolders()[playlistSel - 1]);
      }
      appState = AppState::PLAYING;
      refreshTrackDisplay();
    }
    if (buttons.prev()) {
      // Back out without changing the active playlist.
      appState = AppState::PLAYING;
      refreshTrackDisplay();
    }
  } else if (appState == AppState::IN_CHAPTERS) {
    int totalChapters = (int)player.getChapters().size();
    if (buttons.vdown()) {
      chapterSel = min(chapterSel + 1, totalChapters - 1);
      display.showChapterMenu(chapterSel, player.getChapters());
    }
    if (buttons.vup()) {
      chapterSel = max(chapterSel - 1, 0);
      display.showChapterMenu(chapterSel, player.getChapters());
    }
    if (buttons.play()) {
      player.seekToChapter(chapterSel);
      appState = AppState::PLAYING;
      refreshTrackDisplay();
    }
    if (buttons.next() || buttons.prev()) {
      // Back out without seeking.
      appState = AppState::PLAYING;
      refreshTrackDisplay();
    }
  } else {

    if (buttons.next()) {
      saveBookmark();
      player.next();
      refreshTrackDisplay();
    }

    if (buttons.prev()) {
      saveBookmark();
      player.prev();
      refreshTrackDisplay();
    }

    if (buttons.play()) {
      if (player.isPlaying()) saveBookmark();  // save before pausing
      player.togglePause();
      refreshTrackDisplay();
    }
  }

  if (buttons.sleepRequested()) {
    saveBookmark();  // persist audiobook position before sleep
    // Show farewell message, blank display, then enter deepest sleep that
    // can still be woken by the play button (EXT1 on RTC GPIO, active LOW).
    display.showPowerOff();
    delay(1500);
    display.turnOff();
    rtc_gpio_pullup_en((gpio_num_t)BTN_PLAY);
    rtc_gpio_pulldown_dis((gpio_num_t)BTN_PLAY);
    esp_sleep_enable_ext1_wakeup(1ULL << BTN_PLAY, ESP_EXT1_WAKEUP_ANY_LOW);
    esp_deep_sleep_start();
    // Wake from deep sleep triggers a full restart — setup() runs again.
  }

  if (appState == AppState::PLAYING) {
    if (buttons.vup()) {
      adjustVolume(+VOL_STEP);
    }
    if (buttons.vdown()) {
      adjustVolume(-VOL_STEP);
    }
  }

  // ── Scroll animation — skip while display sleep or menu is active ─────────────
  if (!displaySleeping && appState == AppState::PLAYING) {
    display.update();
  }

  // ── Chapter boundary tracking (audiobook only) ─────────────────────────────
  if (appState == AppState::PLAYING && player.isAudiobook() && player.isPlaying()) {
    uint32_t now = millis();
    if (now - lastChapterCheckMs >= CHAPTER_CHECK_MS) {
      lastChapterCheckMs = now;
      int ch = player.getCurrentChapterIndex();
      if (ch >= 0 && ch != lastChapterIdx) {
        lastChapterIdx = ch;
        refreshTrackDisplay();
      }
    }
  }

  // ── Display sleep (accelerometer 6D orientation) ───────────────────────────
  // Sleep: hardware interrupt fires after device is flat for 2 s.
  if (!displaySleeping && accel.consumeFlatEvent()) {
    refreshTrackDisplay();       // pre-draw track screen into OLED RAM
    display.turnOff();           // clear + DISPLAYOFF
    displaySleeping = true;
  }

  // Wake: poll orientation every 100 ms; turn on as soon as device is raised.
  if (displaySleeping) {
    uint32_t now = millis();
    if (now - lastOrientPollMs >= ORIENT_POLL_MS) {
      lastOrientPollMs = now;
      if (!accel.isFlat()) {
        accel.clearFlatEvent();  // discard any queued flat event from while sleeping
        if (appState == AppState::IN_MENU) {
          display.showMenu(menuItem, eqMode, timerMode, repeatMode);
        } else if (appState == AppState::IN_PLAYLIST) {
          display.showPlaylistMenu(playlistSel, player.getFolders());
        } else if (appState == AppState::IN_CHAPTERS) {
          display.showChapterMenu(chapterSel, player.getChapters());
        } else {
          refreshTrackDisplay();   // draw content before DISPLAYON to avoid blank flash
        }
        display.turnOn();
        displaySleeping = false;
      }
    }
  }
}
