#pragma once
#define FASTLED_ALLOW_INTERRUPTS 1
#include <FastLED.h>
// пины
#define MLED_PIN 13  // пин светодиода режимов
#define LED_PIN 4    // пин DI светодиодной ленты
#define averK 0.006
#define LOOP_DELAY 5  // период основного цикла отрисовки (по умолчанию 5)

#define cbi(sfr, bit) (_SFR_BYTE(sfr) &= ~_BV(bit))
#define sbi(sfr, bit) (_SFR_BYTE(sfr) |= _BV(bit))

struct Global {
  static const uint8_t WIRE_LEFT_PIN = A1;
  static const uint8_t WIRE_RIGHT_PIN = A3;
  static const uint8_t WIRE_FREQUENCY_PIN = A2;
  static const uint8_t MICRO_RIGHT_PIN = A5;
  static const uint8_t MICRO_FREQUENCY_PIN = A4;

  enum Setting {
    ON_OFF,
    MICRO,
    STEREO,
    MODE,
    N_LEDS,
    ENABLED_BRIGHTNESS,
    DISABLED_COLOR,
    UPDATE_THRESHOLD
  };
  bool isOn = false;
  bool isMicro = false;
  bool isStereo = false;  // false - only right channel
  uint8_t currentMode = 0;
  uint16_t numLeds = 100;
  uint8_t enabledBrightness = 200;
  uint8_t disabledBrightness = 30;
  uint8_t disabledHue = HUE_PURPLE;

  // NOT CONFIGURABLE
  uint8_t soundRightPin = WIRE_RIGHT_PIN;
  uint8_t soundFrequencyPin = WIRE_FREQUENCY_PIN;
  // степень усиления сигнала (для более "резкой" работы)
  // TODO set modifyed val or move to define
  static constexpr float expCoeffincient = 1.4;
  uint16_t halfLedsNum;
};

struct VuAnalyzer {
  enum Setting { SMOOTH, IS_RAINBOW, RAINBOW_STEP };
  uint8_t rainbowStep = 5;
  float smooth = 0.3;
  bool isRainbowOn = false;

  static constexpr float maxMultiplier = 1.8;
  // NOT CONFIGURABLE
  uint8_t rainbowColor = 0;
  unsigned long rainbowTimer;
  float rAverage = 0;
  float lAverage = 0;
  static constexpr uint8_t additionalThreshold = 13;
  // коэффициент перевода для палитры
  float colorToLed;
  uint16_t signalThreshold;
  float averageLevel = 50;
  float maxLevel = 100;
};

struct FrequencyAnalyzer {
  enum Setting {
    IS_FULL_TRANSFORM,
    LMH_MODE,
    LHM_LOW_HUE,
    LHM_MEDIUM_HUE,
    LHM_HIGH_HUE,
    LHM_SMOOTH,
    LHM_STEP,
    ONE_LINE_MODE,
    RUNNING_MODE,
    RUNNING_SPEED,
    FULL_LOW_HUE,
    FULL_SMOOTH,
    FULL_HUE_STEP
  };

  bool isFullTransform = false;

  struct LowMediumHighTransform {
    uint8_t mode;

    struct OneLine {
      uint8_t mode;
    } oneLine;

    struct RunningFrequency {
      uint8_t speed = 10;
      int8_t mode = 0;
      // NOT configurable
      unsigned long runDelay = 0;
    } runningFrequency;

    uint8_t colors[3] = {HUE_RED, HUE_GREEN, HUE_YELLOW};
    float smooth = 0.8; 
    uint8_t step = 20;

    // NOT configurable
    float flashMultiplier = 1.2;
    bool isFlash[3];
    uint8_t bright[3];
    float averageFrequency[3];
  } lmhTransform;

  struct FullTransform {
    uint8_t lowFrequencyHue = HUE_RED; 
    uint8_t smooth = 2;  
    uint8_t hueStep = 5;

    // NOT CONFIGURABLE
    float ledsForFreq;
    float maxFrequency = 0;
    uint8_t frequency[30];
  } fullTransform;

  // NOT CONFIGURABLE
  static const uint8_t additionalThreshold = 3;
  uint16_t signalThreshold;
};

struct Strobe {
  enum Setting { COLOR, BRIGHTNESS_STEP, DUTY, CYCLE_PERIOD };
  uint8_t hue = HUE_YELLOW;
  uint8_t saturation = 0;
  uint8_t maxBrighness = 255;
  uint8_t brightStep = 100;
  uint8_t duty = 20;
  uint16_t cyclePeriod = 100;  // период вспышек, миллисекунды

  // NOT CONFIGURABLE
  unsigned long previousFlashTime;
  uint8_t brightness = 0;
};

struct Backlight {
  enum Setting {
    MODE,
    COLOR,
    COLOR_CHANGE_DELAY,
    RAINBOW_COLOR_STEP,
    RAINBOW_STEP
  };
  uint8_t mode = 0;
  CHSV color = {150, 200, 255};

  // mode 2
  uint8_t colorChangeDelay = 100;

  // mode 3
  uint8_t rainbowColorStep = 3;
  uint8_t rainbowStep = 5;

  // NOT CONFIGURABLE
  unsigned long colorChangeTime;
  uint8_t currentHue;
};