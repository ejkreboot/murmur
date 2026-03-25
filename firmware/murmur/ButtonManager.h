#pragma once
#include <Arduino.h>

static constexpr uint32_t DEBOUNCE_MS      = 40;    // debounce filter window
static constexpr uint32_t PLAY_PRIORITY_MS = 100;   // non-play cancels play/pause within ±100 ms
static constexpr uint32_t PLAY_HOLD_MS     = 3000;  // hold duration that triggers restart
static constexpr uint32_t DOUBLE_TAP_MS    = 400;   // window for double-tap detection
static constexpr uint32_t PREV_HOLD_MS     = 1000;  // prev long-press opens playlist nav
static constexpr uint32_t NEXT_HOLD_MS     = 1000;  // next long-press opens chapter nav

struct Button {
  uint8_t  pin;
  bool     lastState;     // last stable (debounced) state; HIGH = released (pull-up)
  bool     currentRaw;    // latest raw digitalRead value
  uint32_t lastChangeMs;  // millis() of last raw edge
  bool     pressed;       // single-shot flag: true for one poll() cycle
};

class ButtonManager {
public:
  ButtonManager(uint8_t pinNext, uint8_t pinPrev,
                uint8_t pinVup,  uint8_t pinVdown,
                uint8_t pinPlay);

  void begin();
  void poll();  // call every loop iteration

  bool next()             const { return _next.pressed;       }
  bool prev()             const { return _prev.pressed;       }
  bool vup()              const { return _vup.pressed;        }
  bool vdown()            const { return _vdown.pressed;      }
  bool play()             const { return _play.pressed;       }  // fires on deferred single tap
  bool doubleTap()        const { return _playDoubleTapped;   }  // fires on second tap within window
  bool sleepRequested()   const { return _sleepRequested;     }
  bool prevHeld()         const { return _prevHeld;           }  // fires once on 1-second hold
  bool nextHeld()         const { return _nextHeld;           }  // fires once on 1-second hold

private:
  Button _next, _prev, _vup, _vdown, _play;

  uint32_t _lastNonPlayAt     = 0;   // millis() of most recent non-play button event (for priority)
  uint32_t _playHoldStart     = 0;   // millis() of play falling edge (for hold duration)
  bool     _sleepRequested    = false;
  bool     _playDoubleTapped  = false;
  uint32_t _pendingPlayTapAt  = 0;   // millis() of first tap (0 = none pending)

  // Prev button hold tracking (handled outside _nonPlay loop)
  uint32_t _prevHoldStart     = 0;   // millis() of prev press edge (0 = not held)
  bool     _prevHoldFired     = false; // true if hold event already fired for current press
  bool     _prevHeld          = false; // single-shot: long-press event

  // Next button hold tracking (mirrors prev)
  uint32_t _nextHoldStart     = 0;
  bool     _nextHoldFired     = false;
  bool     _nextHeld          = false;

  Button* _nonPlay[2];  // convenience pointers: vup, vdown (next & prev handled separately)

  void _initButton(Button& b);

  // Update debounce state; returns fall=true on new press edge, rise=true on new release edge.
  void _pollEdges(Button& b, bool& fell, bool& rose);

  // Cancel pending play tap if it overlaps with a non-play event at 'now'; record _lastNonPlayAt.
  void _cancelPlay(uint32_t now);
};
