#include "ButtonManager.h"

// ── ISR trampoline ────────────────────────────────────────────────────────────
static void IRAM_ATTR timerTrampoline(void* arg) {
  static_cast<ButtonManager*>(arg)->isrPoll();
}

ButtonManager::ButtonManager(uint8_t pinNext, uint8_t pinPrev,
                             uint8_t pinVup,  uint8_t pinVdown,
                             uint8_t pinPlay)
{
  _next  = { pinNext,  HIGH, 0, HIGH, false, false, HIGH, false };
  _prev  = { pinPrev,  HIGH, 0, HIGH, false, false, HIGH, false };
  _vup   = { pinVup,   HIGH, 0, HIGH, false, false, HIGH, false };
  _vdown = { pinVdown, HIGH, 0, HIGH, false, false, HIGH, false };
  _play  = { pinPlay,  HIGH, 0, HIGH, false, false, HIGH, false };
}

void ButtonManager::begin() {
  _initButton(_next);
  _initButton(_prev);
  _initButton(_vup);
  _initButton(_vdown);
  _initButton(_play);

  // Start a 1 kHz hardware timer for ISR-based GPIO sampling.
  esp_timer_create_args_t args = {};
  args.callback = timerTrampoline;
  args.arg      = this;
  args.name     = "btn";
  esp_timer_create(&args, &_timer);
  esp_timer_start_periodic(_timer, 1000);  // 1000 µs = 1 kHz
}

void ButtonManager::_initButton(Button& b) {
  pinMode(b.pin, INPUT_PULLUP);
  b.isrRaw         = HIGH;
  b.isrStableCount = DEBOUNCE_SAMPLES;  // start as "stable HIGH"
  b.isrStable      = HIGH;
  b.isrFell        = false;
  b.isrRose        = false;
  b.lastState      = HIGH;
  b.pressed        = false;
}

// ── isrPoll (called at 1 kHz from hardware timer) ─────────────────────────────
// Debounces all 5 buttons and latches edges.  digitalRead() is safe on ESP32.
void IRAM_ATTR ButtonManager::isrPoll() {
  Button* btns[] = { &_next, &_prev, &_vup, &_vdown, &_play };
  for (int i = 0; i < 5; i++) {
    Button& b = *btns[i];
    bool raw = digitalRead(b.pin);
    if (raw == b.isrRaw) {
      if (b.isrStableCount < 255) b.isrStableCount++;
    } else {
      b.isrRaw = raw;
      b.isrStableCount = 0;
    }
    if (b.isrStableCount == DEBOUNCE_SAMPLES && b.isrRaw != b.isrStable) {
      if (b.isrRaw == LOW) b.isrFell = true;
      else                  b.isrRose = true;
      b.isrStable = b.isrRaw;
    }
  }
}

// ── _consumeEdges ─────────────────────────────────────────────────────────────
// Atomically consume ISR-latched edges for a single button.
void ButtonManager::_consumeEdges(Button& b, bool& fell, bool& rose) {
  fell = false;
  rose = false;
  b.pressed = false;

  noInterrupts();
  fell = b.isrFell;
  rose = b.isrRose;
  b.isrFell = false;
  b.isrRose = false;
  bool stable = b.isrStable;
  interrupts();

  b.lastState = stable;
}

// ── poll ──────────────────────────────────────────────────────────────────────
void ButtonManager::poll() {
  uint32_t now = millis();
  _sleepRequested   = false;
  _playDoubleTapped = false;
  _prevHeld         = false;
  _nextHeld         = false;

  bool fell, rose;

  // ── Play button ────────────────────────────────────────────────────────────
  // Single tap fires immediately on release.  If a second release arrives
  // within DOUBLE_TAP_MS of the first, doubleTap() also fires (both in the
  // same poll cycle).
  _consumeEdges(_play, fell, rose);

  if (fell) {
    _playHoldStart = now;
  }

  if (rose) {
    uint32_t held = now - _playHoldStart;
    if (held >= PLAY_HOLD_MS) {
      _sleepRequested = true;
    } else {
      _play.pressed = true;   // dispatch immediately
      if (_lastPlayTapAt != 0 && (now - _lastPlayTapAt) <= DOUBLE_TAP_MS) {
        _playDoubleTapped = true;
        _lastPlayTapAt    = 0;
      } else {
        _lastPlayTapAt = now;
      }
    }
  }

  // ── Prev button (hold-aware: short press → prev, long press → prevHeld) ────
  _consumeEdges(_prev, fell, rose);
  if (fell) {
    _prevHoldStart = now;
    _prevHoldFired = false;
  }
  if (!_prevHoldFired && _prevHoldStart != 0 && _prev.lastState == LOW
      && (now - _prevHoldStart) >= PREV_HOLD_MS) {
    _prevHeld      = true;
    _prevHoldFired = true;
  }
  if (rose && !_prevHoldFired) {
    _prev.pressed = true;
  }
  if (rose) {
    _prevHoldStart = 0;
  }

  // ── Next button (hold-aware: short press → next, long press → nextHeld) ────
  _consumeEdges(_next, fell, rose);
  if (fell) {
    _nextHoldStart = now;
    _nextHoldFired = false;
  }
  if (!_nextHoldFired && _nextHoldStart != 0 && _next.lastState == LOW
      && (now - _nextHoldStart) >= NEXT_HOLD_MS) {
    _nextHeld      = true;
    _nextHoldFired = true;
  }
  if (rose && !_nextHoldFired) {
    _next.pressed = true;
  }
  if (rose) {
    _nextHoldStart = 0;
  }

  // ── Volume Up (immediate on press + auto-repeat while held) ────────────────
  _consumeEdges(_vup, fell, rose);
  if (fell) {
    _vup.pressed  = true;            // fire immediately on press
    _vupHoldStart = now;
    _vupRepeatAt  = now + VOL_INITIAL_DELAY_MS;
  }
  if (_vupHoldStart != 0 && _vup.lastState == LOW && now >= _vupRepeatAt) {
    _vup.pressed = true;             // auto-repeat step
    _vupRepeatAt = now + VOL_REPEAT_MS;
  }
  if (rose) {
    _vupHoldStart = 0;
  }

  // ── Volume Down (immediate on press + auto-repeat while held) ──────────────
  _consumeEdges(_vdown, fell, rose);
  if (fell) {
    _vdown.pressed  = true;
    _vdownHoldStart = now;
    _vdownRepeatAt  = now + VOL_INITIAL_DELAY_MS;
  }
  if (_vdownHoldStart != 0 && _vdown.lastState == LOW && now >= _vdownRepeatAt) {
    _vdown.pressed = true;
    _vdownRepeatAt = now + VOL_REPEAT_MS;
  }
  if (rose) {
    _vdownHoldStart = 0;
  }
}
