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

  _nonPlay[0] = &_vup;
  _nonPlay[1] = &_vdown;
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

// ── _cancelPlay ───────────────────────────────────────────────────────────────
// Called whenever a non-play button event fires.  Cancels any pending play
// single-tap whose timestamp falls within PLAY_PRIORITY_MS of 'now', and
// records the time so that a play tap armed shortly after is also suppressed.
void ButtonManager::_cancelPlay(uint32_t now) {
  if (_pendingPlayTapAt != 0 &&
      (uint32_t)abs((int32_t)(now - _pendingPlayTapAt)) <= PLAY_PRIORITY_MS) {
    _pendingPlayTapAt = 0;
  }
  _lastNonPlayAt = now;
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
      _pendingPlayTapAt = 0;  // cancel any pending single-tap
    } else {
      // Check for double-tap: second tap within DOUBLE_TAP_MS of the first
      if (_pendingPlayTapAt != 0 && (now - _pendingPlayTapAt) <= DOUBLE_TAP_MS) {
        _playDoubleTapped = true;
        _pendingPlayTapAt = 0;
      } else {
        // Non-play buttons take priority: suppress the tap if one fired recently
        if (_lastNonPlayAt == 0 || (now - _lastNonPlayAt) > PLAY_PRIORITY_MS) {
          _pendingPlayTapAt = now;  // arm deferred single-tap timer
        }
      }
    }
  }

  // Deferred single-tap: fire once the double-tap window has elapsed.
  // Also cancel if a non-play event landed close to the original tap.
  if (_pendingPlayTapAt != 0 && (now - _pendingPlayTapAt) > DOUBLE_TAP_MS) {
    bool nonPlayNear = (_lastNonPlayAt != 0) &&
                       ((uint32_t)abs((int32_t)(_lastNonPlayAt - _pendingPlayTapAt))
                        <= PLAY_PRIORITY_MS);
    if (nonPlayNear) {
      _pendingPlayTapAt = 0;  // non-play wins — discard the tap
    } else {
      _play.pressed    = true;
      _pendingPlayTapAt = 0;
    }
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
    _prevHoldFired = true;   // fire once per hold; suppress rose-based press
    _cancelPlay(now);
  }
  if (rose && !_prevHoldFired) {
    _prev.pressed = true;    // short press: deliver on release
    _cancelPlay(now);
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
    _cancelPlay(now);
  }
  if (rose && !_nextHoldFired) {
    _next.pressed = true;    // short press: deliver on release
    _cancelPlay(now);
  }
  if (rose) {
    _nextHoldStart = 0;
  }

  // ── Non-play buttons (vup, vdown): fire immediately on press ──────────────
  for (Button* b : _nonPlay) {
    _pollEdges(*b, fell, rose);  // clears b->pressed, updates debounce state
    if (fell) {
      b->pressed = true;
      _cancelPlay(now);
    }
  }
}
