#pragma once
#include <Arduino.h>
#include <esp_timer.h>

static constexpr uint8_t  DEBOUNCE_SAMPLES      = 25;    // ISR ticks (1 kHz) ≈ 25 ms
static constexpr uint32_t PLAY_HOLD_MS          = 3000;  // hold duration that triggers sleep
static constexpr uint32_t DOUBLE_TAP_MS         = 250;   // window for double-tap detection
static constexpr uint32_t PREV_HOLD_MS          = 1000;  // prev long-press opens playlist nav
static constexpr uint32_t NEXT_HOLD_MS          = 1000;  // next long-press opens chapter nav
static constexpr uint32_t VOL_INITIAL_DELAY_MS  = 400;   // delay before volume auto-repeat starts
static constexpr uint32_t VOL_REPEAT_MS         = 120;   // interval between auto-repeat steps

struct Button {
  uint8_t  pin;

  // ISR-maintained debounce state (written only by ISR)
  volatile bool     isrRaw;          // latest GPIO read
  volatile uint8_t  isrStableCount;  // consecutive matching samples
  volatile bool     isrStable;       // debounced stable state
  volatile bool     isrFell;         // latched HIGH→LOW edge
  volatile bool     isrRose;         // latched LOW→HIGH edge

  // poll()-maintained
  bool     lastState;     // last consumed stable state; HIGH = released
  bool     pressed;       // single-shot flag: true for one poll() cycle
};

class ButtonManager {
public:
  ButtonManager(uint8_t pinNext, uint8_t pinPrev,
                uint8_t pinVup,  uint8_t pinVdown,
                uint8_t pinPlay);

  void begin();
  void poll();  // call every loop iteration

  // Called by hardware timer ISR — samples all GPIOs and debounces.
  void IRAM_ATTR isrPoll();

  bool next()             const { return _next.pressed;       }
  bool prev()             const { return _prev.pressed;       }
  bool vup()              const { return _vup.pressed;        }
  bool vdown()            const { return _vdown.pressed;      }
  bool play()             const { return _play.pressed;       }  // fires immediately on tap release
  bool doubleTap()        const { return _playDoubleTapped;   }  // fires on second tap within window
  bool sleepRequested()   const { return _sleepRequested;     }
  bool prevHeld()         const { return _prevHeld;           }  // fires once on 1-second hold
  bool nextHeld()         const { return _nextHeld;           }  // fires once on 1-second hold

private:
  Button _next, _prev, _vup, _vdown, _play;
  esp_timer_handle_t _timer = nullptr;

  uint32_t _playHoldStart     = 0;   // millis() of play falling edge (for hold duration)
  bool     _sleepRequested    = false;
  bool     _playDoubleTapped  = false;
  uint32_t _lastPlayTapAt     = 0;   // millis() of previous tap (for double-tap detection)

  // Prev button hold tracking
  uint32_t _prevHoldStart     = 0;
  bool     _prevHoldFired     = false;
  bool     _prevHeld          = false;

  // Next button hold tracking
  uint32_t _nextHoldStart     = 0;
  bool     _nextHoldFired     = false;
  bool     _nextHeld          = false;

  // Volume button hold-to-repeat tracking
  uint32_t _vupHoldStart      = 0;   // 0 = not held
  uint32_t _vupRepeatAt       = 0;   // millis() of next auto-repeat
  uint32_t _vdownHoldStart    = 0;
  uint32_t _vdownRepeatAt     = 0;

  void _initButton(Button& b);

  // Consume ISR-latched edges for button b into fell/rose.
  void _consumeEdges(Button& b, bool& fell, bool& rose);
};
