#include "ButtonManager.h"

ButtonManager::ButtonManager(uint8_t pinNext, uint8_t pinPrev,
                             uint8_t pinVup,  uint8_t pinVdown,
                             uint8_t pinPlay)
{
  _next  = { pinNext,  HIGH, HIGH, 0, false };
  _prev  = { pinPrev,  HIGH, HIGH, 0, false };
  _vup   = { pinVup,   HIGH, HIGH, 0, false };
  _vdown = { pinVdown, HIGH, HIGH, 0, false };
  _play  = { pinPlay,  HIGH, HIGH, 0, false };
}

void ButtonManager::begin() {
  _initButton(_next);
  _initButton(_prev);
  _initButton(_vup);
  _initButton(_vdown);
  _initButton(_play);
}

void ButtonManager::_initButton(Button& b) {
  pinMode(b.pin, INPUT_PULLUP);
  b.lastState    = HIGH;
  b.currentRaw   = HIGH;
  b.lastChangeMs = 0;
  b.pressed      = false;
}

// ── _pollEdges ────────────────────────────────────────────────────────────────
// Update debounce state for button b.
// Sets fell=true on a newly-stable HIGH→LOW transition (press).
// Sets rose=true on a newly-stable LOW→HIGH transition (release).
// At most one of fell/rose is true per call.
void ButtonManager::_pollEdges(Button& b, bool& fell, bool& rose) {
  fell = false;
  rose = false;
  b.pressed = false;  // clear single-shot flag

  bool raw = digitalRead(b.pin);
  if (raw != b.currentRaw) {
    b.currentRaw   = raw;
    b.lastChangeMs = millis();
  }

  if ((millis() - b.lastChangeMs) >= DEBOUNCE_MS) {
    if (b.lastState == HIGH && b.currentRaw == LOW) {
      fell = true;
    } else if (b.lastState == LOW && b.currentRaw == HIGH) {
      rose = true;
    }
    b.lastState = b.currentRaw;
  }
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
  _pollEdges(_play, fell, rose);

  if (fell) {
    _playHoldStart = now;
  }

  if (rose) {
    uint32_t held = now - _playHoldStart;
    if (held >= PLAY_HOLD_MS) {
      _sleepRequested   = true;
      _pendingPlayTapAt = 0;
    } else {
      // Double-tap: second tap within DOUBLE_TAP_MS of the first
      if (_pendingPlayTapAt != 0 && (now - _pendingPlayTapAt) <= DOUBLE_TAP_MS) {
        _playDoubleTapped = true;
        _pendingPlayTapAt = 0;
      } else {
        _pendingPlayTapAt = now;  // arm deferred single-tap timer
      }
    }
  }

  // Deferred single-tap: fire once the double-tap window has elapsed.
  if (_pendingPlayTapAt != 0 && (now - _pendingPlayTapAt) > DOUBLE_TAP_MS) {
    _play.pressed     = true;
    _pendingPlayTapAt = 0;
  }

  // ── Prev button (hold-aware: short press → prev, long press → prevHeld) ────
  _pollEdges(_prev, fell, rose);
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
  _pollEdges(_next, fell, rose);
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
  _pollEdges(_vup, fell, rose);
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
  _pollEdges(_vdown, fell, rose);
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
