#pragma once
#include <FastLED.h>
// пины
#define SOUND_R A2  // аналоговый пин вход аудио, правый канал
#define SOUND_L A1  // аналоговый пин вход аудио, левый канал
// аналоговый пин вход аудио для режима с частотами (через кондер)
#define SOUND_R_FREQ A3
#define MLED_PIN 13  // пин светодиода режимов
#define LED_PIN 4    // пин DI светодиодной ленты
// 1 - используем потенциометр, 0 - используется внутренний источник
// опорного напряжения 1.1 В
#define POTENT 0
#define POT_GND A0  // пин земля для потенциометра

#define averK 0.006
#define LOOP_DELAY 5  // период основного цикла отрисовки (по умолчанию 5)

#define cbi(sfr, bit) (_SFR_BYTE(sfr) &= ~_BV(bit))
#define sbi(sfr, bit) (_SFR_BYTE(sfr) |= _BV(bit))

struct Global {
  enum Setting {
    ON_OFF,
    MICRO,
    STEREO,
    MODE,
    N_LEDS,
    ENABLED_BRIGHTNESS,
    DISABLED_BRIGHTNESS
  };
  bool isOn = false;
  bool isMicro = false;
  // false - only right channel
  bool isStereo = false;
  uint8_t currentMode = 0;
  uint16_t numLeds = 100;
  uint8_t enabledBrightness = 200;
  uint8_t disabledBrightness = 30;
  uint8_t disabledColor = HUE_PURPLE;

  // степень усиления сигнала (для более "резкой" работы)
  // TODO set modifyed val or move to define
  static constexpr float expCoeffincient = 1.4;
  uint16_t halfLedsNum;
};

struct VuAnalyzer {
  // коэффициент перевода для палитры
  float colorToLed;
  uint16_t signalThreshold = 15;
  float averageLevel = 50;
  float maxLevel = 100;
  static constexpr float maxMultiplier = 1.8;
  uint8_t rainbowStep = 5;
  float smooth = 0.3;
  bool isRainbowOn = false;
  uint8_t rainbowColor = 0;
  unsigned long rainbowTimer;

  float rAverage = 0;
  float lAverage = 0;

  static constexpr uint8_t additionalThreshold = 13;
};

struct FrequencyAnalyzer {
  uint16_t signalThreshold = 40;
  bool isFullTransform = false;
  struct LowMediumHighTransform {
    uint8_t colors[3] = {HUE_RED, HUE_GREEN, HUE_YELLOW};

    float smooth = 0.8;
    float flashMultiplier = 1.2;
    uint8_t bright[3];
    float averageFrequency[3];
    bool isFlash[3];
    static const uint8_t step = 20;
    uint8_t mode;
    struct RunningFrequency {
      uint8_t speed = 10;
      unsigned long runDelay;
      int8_t mode = 0;
    } runningFrequency;
    struct OneLine {
      uint8_t mode;
    } oneLine;
  } lmhTransform;

  struct FullTransform {
    uint8_t smooth = 2;
    uint8_t lowFrequencyHue = HUE_RED;
    uint8_t hueStep = 5;

    float ledsForFreq;
    float maxFrequency = 0;
    uint8_t frequency[30];
  } fullTransform;

  static const uint8_t additionalThreshold = 3;
};

struct Strobe {
  unsigned long previousFlashTime;
  uint8_t hue = HUE_YELLOW;
  uint8_t saturation = 0;
  uint8_t bright = 0;
  uint8_t brightStep = 100;
  uint8_t duty = 20;
  uint16_t cycleDelay = 100;  // период вспышек, миллисекунды
};

struct Backlight {
  uint8_t mode = 0;
  uint8_t defaultHue = 150;
  // mode 1
  uint8_t defaultSaturation = 200;

  // mode 2
  uint8_t colorChangeDelay = 100;
  unsigned long colorChangeTime;
  uint8_t currentColor;

  // mode 3
  uint8_t rainbowColorChangeStep = 3;
  uint8_t rainbowStep = 5;
};