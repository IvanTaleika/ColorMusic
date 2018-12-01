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

  struct Settings {
    bool isMicro = false;
    bool isStereo = false;  // false - only right channel
    uint16_t numLeds = 100;
    uint8_t enabledBrightness = 200;
    uint8_t disabledHue = HUE_PURPLE;
    uint8_t disabledBrightness = 30;
  } settings;

  // NOT CONFIGURABLE
  uint8_t soundRightPin = WIRE_RIGHT_PIN;
  uint8_t soundFrequencyPin = WIRE_FREQUENCY_PIN;
  // степень усиления сигнала (для более "резкой" работы)
  static constexpr float expCoeffincient = 1.4;
  uint16_t halfLedsNum;
};

struct VuAnalyzer {
  struct Settings {
    uint8_t rainbowStep = 5;
    uint8_t smooth = 3;
    bool isRainbowOn = false;
  } settings;
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

struct LowMediumHighFrequency {
  struct Settings {
    uint8_t mode;

    uint8_t colors[3] = {HUE_RED, HUE_GREEN, HUE_YELLOW};
    uint8_t smooth = 0.8;
    uint8_t step = 20;

    uint8_t speed = 10;
    uint8_t runningFrequencyMode = 0;

    uint8_t oneLineMode;
  } settings;

  // NOT configurable
  float flashMultiplier = 1.2;
  bool isFlash[3];
  uint8_t bright[3];
  uint8_t lastFrequency[3];
  unsigned long runDelay = 0;
};

struct FullRangeFrequency {
  struct Settings {
    uint8_t lowFrequencyHue = HUE_RED;
    uint8_t smooth = 2;
    uint8_t hueStep = 5;
  } settings;

  // NOT CONFIGURABLE
  float ledsForFreq;
  float maxFrequency = 0;
  uint8_t frequency[30];
};

struct FrequencyAnalyzer {
  // NOT CONFIGURABLE
  static const uint8_t additionalThreshold = 3;
  uint16_t signalThreshold;
};

struct Strob {
  struct Settings {
    uint8_t hue = HUE_YELLOW;
    uint8_t saturation = 0;
    uint8_t maxBrighness = 255;
    uint8_t brightStep = 100;
    uint8_t duty = 20;
    uint16_t flashPeriod = 100;  // период вспышек, миллисекунды
  } settings;

  // NOT CONFIGURABLE
  unsigned long previousFlashTime;
  uint8_t brightness = 0;
};

struct Backlight {
  struct Settings {
    uint8_t mode = 0;
    CHSV color = {150, 200, 255};

    // mode 2
    uint8_t colorChangeDelay = 100;

    // mode 3
    uint8_t rainbowColorStep = 3;
    uint8_t rainbowStep = 5;
  } settings;

  // NOT CONFIGURABLE
  unsigned long colorChangeTime;
  uint8_t currentHue;
};