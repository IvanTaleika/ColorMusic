/*
   Версия 3.0 Управление с андроид смартфона!
   //TODO add android application load path
   Крутейшая свето-цветомузыка на Arduino и адресной светодиодной ленте WS2812b.
   Данная версия поддерживает около 410 светодиодов!

   Автор проекта: AlexGyver
   Страница проекта: http://alexgyver.ru/colormusic/
   GitHub: https://github.com/AlexGyver/ColorMusic
   Автор версии 3.0: IvanTaleika
   GitHub: https://github.com/IvanTaleika/ColorMusic
*/

/*
  Цвета для HSV
  HUE_RED
  HUE_ORANGE
  HUE_YELLOW
  HUE_GREEN
  HUE_AQUA
  HUE_BLUE
  HUE_PURPLE
  HUE_PINK
*/

// --------------------------- НАСТРОЙКИ ---------------------------
// Serial.print("");
// Serial.println();
#define FHT_N 64   // ширина спектра х2
#define LOG_OUT 1  // FOR FHT.h lib
#include <EEPROMex.h>
#include <FHT.h>  // преобразование Хартли
#include <SoftwareSerial.h>
#define FASTLED_ALLOW_INTERRUPTS 1
#include "FastLED.h"

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

SoftwareSerial bluetoothSerial(4, 3);  // RX | TX
#define START_BYTE '$'
#define END_BYTE '^'

#define averK 0.006
#define LOOP_DELAY 5  // период основного цикла отрисовки (по умолчанию 5)

#define cbi(sfr, bit) (_SFR_BYTE(sfr) &= ~_BV(bit))
#define sbi(sfr, bit) (_SFR_BYTE(sfr) |= _BV(bit))

void analyzeAudio();

struct Global {
  bool isOn = true;
  bool isMicro = false;
  // 1 - только один канал (ПРАВЫЙ!!!!! SOUND_R!!!!!), 0 - два канала
  bool isMono = true;
  uint8_t enabledBrightness = 200;
  uint8_t disabledBrightness = 30;
  uint8_t currentMode = 0;
  // степень усиления сигнала (для более "резкой" работы)
  float expCoeffincient = 1;
  uint8_t disabledColor = HUE_PURPLE;

  uint16_t numLeds = 100;
  uint16_t halfLedsNum;
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

// режим стробоскопа
struct Strobe {
  unsigned long previousFlashTime;
  uint8_t hue = HUE_YELLOW;
  uint8_t saturation = 0;
  uint8_t bright = 0;
  uint8_t brightStep = 100;
  uint8_t duty = 20;
  uint16_t cycleDelay = 100;  // период вспышек, миллисекунды
};

struct VuAnalyzer {
  // коэффициент перевода для палитры
  float colorToLed;
  uint16_t signalThreshold = 15;
  float averageLevel = 50;
  float maxLevel = 100;
  static const float maxMultiplier = 1.8;
  uint8_t rainbowStep = 5;
  float smooth = 0.3;
  bool isRainbowOn = false;
  uint8_t rainbowColor = 0;
  unsigned long rainbowTimer;

  float rAverage = 0;
  float lAverage = 0;

  static const uint8_t additionalThreshold = 13;
  void setAutoThreshold() {
    int maxLevel = 0;  // максимум
    int level;
    for (uint8_t i = 0; i < 200; i++) {
      level = analogRead(SOUND_R);  // делаем 200 измерений
      if (level > maxLevel && level < 150) {  // ищем максимумы
        maxLevel = level;                     // запоминаем
      }
      delay(4);  // ждём 4мс
    }
    // нижний порог как максимум тишины  некая величина
    signalThreshold = maxLevel + additionalThreshold;
  }
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
  void setAutoThreshold() {
    int maxLevel = 0;  // максимум
    int level;
    for (uint8_t i = 0; i < 100; i++) {  // делаем 100 измерений
      analyzeAudio();                    // разбить в спектр
      for (uint8_t j = 2; j < 32; j++) {  // первые 2 канала - хлам
        level = fht_log_out[j];
        if (level > maxLevel) {  // ищем максимумы
          maxLevel = level;      // запоминаем
        }
      }
      delay(4);  // ждём 4мс
    }
    // нижний порог как максимум тишины некая величина
    signalThreshold = maxLevel + additionalThreshold;
  }
};

// градиент-палитра от зелёного к красному
DEFINE_GRADIENT_PALETTE(soundlevel_gp){
    0,   0,   255, 0,  // green
    100, 255, 255, 0,  // yellow
    150, 255, 100, 0,  // orange
    200, 255, 50,  0,  // red
    255, 255, 0,   0   // red
};

CRGBPalette32 MY_PAL = soundlevel_gp;
unsigned long LOOP_TIMER = 0;
CRGB* STRIP_LEDS = nullptr;
Global GLOBAL;
VuAnalyzer VU;
FrequencyAnalyzer FREQUENCY;
Strobe STROBE;
Backlight BACKLIGHT;

void setup() {
  Serial.begin(9600);
  pinMode(MLED_PIN, OUTPUT);  //Режим пина для светодиода режима на выход
  digitalWrite(MLED_PIN, LOW);  //Выключение светодиода режима
  pinMode(POT_GND, OUTPUT);
  digitalWrite(POT_GND, LOW);
  if (POTENT) {
    analogReference(EXTERNAL);
  } else {
    analogReference(INTERNAL);
  }
  sbi(ADCSRA, ADPS2);
  cbi(ADCSRA, ADPS1);
  sbi(ADCSRA, ADPS0);

  initLeds();
  setThreshold();
}

void initLeds() {
  delete[] STRIP_LEDS;
  STRIP_LEDS = new CRGB[GLOBAL.numLeds];
  FastLED.addLeds<WS2811, LED_PIN, GRB>(STRIP_LEDS, GLOBAL.numLeds)
      .setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(GLOBAL.enabledBrightness);
  GLOBAL.halfLedsNum = GLOBAL.numLeds / 2;
  VU.colorToLed = (float)255 / GLOBAL.halfLedsNum;

  FREQUENCY.fullTransform.ledsForFreq = GLOBAL.halfLedsNum / 30;
}

void setThreshold() {
  delay(1000);  // ждём инициализации АЦП
  VU.setAutoThreshold();
  FREQUENCY.setAutoThreshold();
}

void loop() {
  checkBluetooth();
  if (GLOBAL.isOn && millis() - LOOP_TIMER > LOOP_DELAY) {
    processSound();
    FastLED.show();
    if (GLOBAL.currentMode != 7) {  // 7 режиму не нужна очистка!!!
      FastLED.clear();  // очистить массив пикселей
    }
    LOOP_TIMER = millis();  // сбросить таймер
  }
}

void processSound() {
  switch (GLOBAL.currentMode) {
    case 0:
      processLevel();
      return;
    case 1:
      processFrequency();
      return;
    case 2:
      processStrobe();
      return;
    case 3:
      backlightAnimation();
      return;
  }
}

void processLevel() {
  uint16_t rMax = 0, lMax = 0;
  uint16_t rLevel, lLevel;

  for (uint8_t i = 0; i < 100; i++) {
    rLevel = analogRead(SOUND_R);
    if (rMax < rLevel) {
      rMax = rLevel;
    }
    if (!GLOBAL.isMono && !GLOBAL.isMicro) {
      lLevel = analogRead(SOUND_L);
      if (lMax < lLevel) {
        lMax = lLevel;
      }
    }
  }
  // фильтр скользящее среднее
  VU.rAverage = (float)rMax * VU.smooth + VU.rAverage * (1 - VU.smooth);

  if (!GLOBAL.isMono && !GLOBAL.isMicro) {
    lMax = calcSoundLevel(lMax);
    VU.lAverage = (float)lMax * VU.smooth + VU.lAverage * (1 - VU.smooth);
  } else {
    VU.lAverage = VU.rAverage;
  }

  if (VU.rAverage > VU.signalThreshold && VU.lAverage > VU.signalThreshold) {
    // расчёт общей средней громкости с обоих каналов, фильтрация.
    // Фильтр очень медленный, сделано специально для автогромкости
    VU.averageLevel = (float)(VU.rAverage + VU.lAverage) / 2. * averK +
                      VU.averageLevel * (1 - averK);
    // принимаем максимальную громкость шкалы как среднюю, умноженную на
    // некоторый коэффициент maxMultiplier
    VU.maxLevel = VU.averageLevel * VU.maxMultiplier;

    // преобразуем сигнал в длину ленты
    int16_t rLength = map(VU.rAverage, 0, VU.maxLevel, 0, GLOBAL.halfLedsNum);
    int16_t lLength = map(VU.lAverage, 0, VU.maxLevel, 0, GLOBAL.halfLedsNum);
    // ограничиваем до макс. числа светодиодов
    rLength = constrain(rLength, 0, GLOBAL.halfLedsNum);
    lLength = constrain(lLength, 0, GLOBAL.halfLedsNum);

    vuAnimation(rLength, lLength);
  } else if (GLOBAL.disabledBrightness > 0) {
    silence();
  }
}

void vuAnimation(int16_t rLength, int16_t lLength) {
  uint8_t count = 0;
  int16_t halfLedsNum = GLOBAL.halfLedsNum;
  float colorToLed = VU.colorToLed;
  if (VU.isRainbowOn) {
    // заливка по палитре радуга
    if (millis() - VU.rainbowTimer > 30) {
      VU.rainbowTimer = millis();
      VU.rainbowColor = VU.rainbowColor + VU.rainbowStep;
    }
    count = 0;
    // RainbowColors_p -  default FastLED pallet
    for (int i = (halfLedsNum - 1); i > ((halfLedsNum - 1) - rLength);
         i--, count++) {
      STRIP_LEDS[i] = ColorFromPalette(
          RainbowColors_p, (count * colorToLed) / 2 - VU.rainbowColor);
    }
    count = 0;
    for (int i = halfLedsNum; i < (halfLedsNum + lLength); i++, count++) {
      STRIP_LEDS[i] = ColorFromPalette(
          RainbowColors_p, (count * colorToLed) / 2 - VU.rainbowColor);
    }
  } else {
    // заливка по палитре " от зелёного к красному"
    for (int i = (halfLedsNum - 1); i > ((halfLedsNum - 1) - rLength);
         i--, count++) {
      STRIP_LEDS[i] = ColorFromPalette(MY_PAL, (count * colorToLed));
    }
    count = 0;
    for (int i = halfLedsNum; i < (halfLedsNum + lLength); i++, count++) {
      STRIP_LEDS[i] = ColorFromPalette(MY_PAL, (count * colorToLed));
    }
  }
  if (GLOBAL.disabledBrightness > 0) {
    colorEmptyLeds(halfLedsNum - rLength, halfLedsNum - lLength);
  }
}

void colorEmptyLeds(int16_t rDisabled, int16_t lDisabled) {
  CHSV dark = CHSV(GLOBAL.disabledColor, 255, GLOBAL.disabledBrightness);
  if (rDisabled > 0) {
    fill_solid(STRIP_LEDS, rDisabled, dark);
  }
  if (lDisabled > 0) {
    fill_solid(&(STRIP_LEDS[GLOBAL.numLeds - lDisabled]), lDisabled, dark);
  }
}

// DONE!
void backlightAnimation() {
  switch (BACKLIGHT.mode) {
    case 0:
      fillLeds(CHSV(BACKLIGHT.defaultHue, BACKLIGHT.defaultSaturation, 255));
      break;
    case 1:
      if (millis() - BACKLIGHT.colorChangeTime > BACKLIGHT.colorChangeDelay) {
        BACKLIGHT.colorChangeTime = millis();
        BACKLIGHT.currentColor++;
      }
      fillLeds(CHSV(BACKLIGHT.currentColor, BACKLIGHT.defaultSaturation, 255));
      break;
    case 2:
      if (millis() - BACKLIGHT.colorChangeTime > BACKLIGHT.colorChangeDelay) {
        BACKLIGHT.colorChangeTime = millis();
        BACKLIGHT.currentColor += BACKLIGHT.rainbowColorChangeStep;
      }
      uint8_t rainbowStep = BACKLIGHT.currentColor;
      for (uint16_t i = 0; i < GLOBAL.numLeds; i++) {
        STRIP_LEDS[i] = CHSV(rainbowStep, 255, 255);
        rainbowStep += BACKLIGHT.rainbowStep;
      }
      break;
  }
}
// DONE!
void processStrobe() {
  unsigned long delay = millis() - STROBE.previousFlashTime;
  if (delay > STROBE.cycleDelay) {
    STROBE.previousFlashTime = millis();
  } else if (delay > STROBE.cycleDelay * STROBE.duty / 100) {
    if (STROBE.bright > STROBE.brightStep) {
      STROBE.bright -= STROBE.brightStep;
    } else {
      STROBE.bright = 0;
    }
  } else if (STROBE.bright < 255 - STROBE.brightStep) {
    STROBE.bright += STROBE.brightStep;
  } else {
    STROBE.bright = 255;
  }
  strobeAnimation();
}
// DONE!
void strobeAnimation() {
  if (STROBE.bright > 0) {
    fillLeds(CHSV(STROBE.hue, STROBE.saturation, STROBE.bright));
  } else {
    fillLeds(CHSV(GLOBAL.disabledColor, 255, GLOBAL.disabledBrightness));
  }
}

void processFrequency() {
  analyzeAudio();
  if (FREQUENCY.isFullTransform) {
    fullFrequencyTransform();
  } else {
    lmhTransform();
  }
}

void analyzeAudio() {
  for (int i = 0; i < FHT_N; i++) {
    fht_input[i] = analogRead(SOUND_R_FREQ);  // put real data into bins
  }
  fht_window();   // window the data for better frequency response
  fht_reorder();  // reorder the data before doing the fht
  fht_run();      // process the data in the fht
  fht_mag_log();  // take the output of the fht
}

void fullFrequencyTransform() {
  uint8_t freqMax = 0;
  uint8_t* frequency = FREQUENCY.fullTransform.frequency;
  for (uint8_t i = 0; i < 30; i++) {
    if (fht_log_out[i + 2] > freqMax) {
      freqMax = fht_log_out[i + 2];
    }
    if (freqMax < 5) {
      freqMax = 5;
    }

    if (frequency[i] < fht_log_out[i + 2]) {
      frequency[i] = fht_log_out[i + 2];
    } else if (frequency[i] > FREQUENCY.fullTransform.smooth) {
      frequency[i] -= FREQUENCY.fullTransform.smooth;
    } else {
      frequency[i] = 0;
    }
  }
  FREQUENCY.fullTransform.maxFrequency =
      freqMax * averK + FREQUENCY.fullTransform.maxFrequency * (1 - averK);
  fullFrequencyAnimation();
}

void fullFrequencyAnimation() {
  uint8_t hue = FREQUENCY.fullTransform.lowFrequencyHue;
  uint8_t stripSize;
  uint8_t bright;
  uint16_t position = 0;
  for (uint8_t i = 0; i < 30; i++) {
    stripSize = (float)i * FREQUENCY.fullTransform.ledsForFreq + 0.5;
    bright = map(FREQUENCY.fullTransform.frequency[30 - 1 - i], 0,
                 FREQUENCY.fullTransform.maxFrequency, 0, 255);
    fill_solid(STRIP_LEDS + position, stripSize, CHSV(hue, 255, bright));
    fill_solid(&(STRIP_LEDS[GLOBAL.numLeds - stripSize - position - 1]),
               stripSize, CHSV(hue, 255, bright));
    hue += FREQUENCY.fullTransform.hueStep;
    position += stripSize;
  }
}

void lmhTransform() {
  uint8_t frequency[3];
  frequency[0] = 0;
  frequency[1] = 0;
  frequency[2] = 0;

  // низкие частоты, выборка со 2 по 5 тон (0 и 1 зашумленные!)
  for (uint8_t i = 2; i < 6; i++) {
    if (fht_log_out[i] > frequency[0] &&
        fht_log_out[i] > FREQUENCY.signalThreshold) {
      frequency[0] = fht_log_out[i];
    }
  }
  // средние частоты, выборка с 6 по 10 тон
  for (uint8_t i = 6; i < 11; i++) {
    if (fht_log_out[i] > frequency[1] &&
        fht_log_out[i] > FREQUENCY.signalThreshold) {
      frequency[1] = fht_log_out[i];
    }
  }
  // высокие частоты, выборка с 11 по 31 тон
  for (uint8_t i = 11; i < 32; i++) {
    if (fht_log_out[i] > frequency[2] &&
        fht_log_out[i] > FREQUENCY.signalThreshold) {
      frequency[2] = fht_log_out[i];
    }
  }

  //Звук с одинаковыми частотами в итоге вырубит отображение вообще
  float value;
  for (uint8_t i = 0; i < 3; i++) {
    value = frequency[i] * FREQUENCY.lmhTransform.smooth +
            FREQUENCY.lmhTransform.averageFrequency[i] *
                (1 - FREQUENCY.lmhTransform.smooth);
    if (value > FREQUENCY.lmhTransform.averageFrequency[i] *
                    FREQUENCY.lmhTransform.flashMultiplier) {
      FREQUENCY.lmhTransform.bright[i] = 255;
      FREQUENCY.lmhTransform.isFlash[i] = true;
    } else {
      FREQUENCY.lmhTransform.isFlash[i] = false;
    }
    if (FREQUENCY.lmhTransform.bright[i] >= 0) {
      FREQUENCY.lmhTransform.bright[i] -= FREQUENCY.lmhTransform.step;
    }
    if (FREQUENCY.lmhTransform.bright[i] < GLOBAL.disabledBrightness) {
      FREQUENCY.lmhTransform.bright[i] = GLOBAL.disabledBrightness;
    }
    FREQUENCY.lmhTransform.averageFrequency[i] = value;
  }
  lmhFrequencyAnimation();
}

void lmhFrequencyAnimation() {
  switch (FREQUENCY.lmhTransform.mode) {
    case 0: {
      uint16_t stripLenght = GLOBAL.numLeds / 5;
      fill_solid(STRIP_LEDS, stripLenght,
                 CHSV(FREQUENCY.lmhTransform.colors[2], 255,
                      FREQUENCY.lmhTransform.bright[2]));
      fill_solid(STRIP_LEDS + stripLenght, stripLenght,
                 CHSV(FREQUENCY.lmhTransform.colors[1], 255,
                      FREQUENCY.lmhTransform.bright[1]));
      fill_solid(STRIP_LEDS + 2 * stripLenght, stripLenght,
                 CHSV(FREQUENCY.lmhTransform.colors[0], 255,
                      FREQUENCY.lmhTransform.bright[0]));
      fill_solid(STRIP_LEDS + 3 * stripLenght, stripLenght,
                 CHSV(FREQUENCY.lmhTransform.colors[1], 255,
                      FREQUENCY.lmhTransform.bright[1]));
      fill_solid(STRIP_LEDS + 4 * stripLenght, stripLenght,
                 CHSV(FREQUENCY.lmhTransform.colors[2], 255,
                      FREQUENCY.lmhTransform.bright[2]));
      break;
    }
    case 1: {
      uint16_t stripLenght = GLOBAL.numLeds / 3;
      for (uint8_t i = 0; i < 3; i++) {
        fill_solid(STRIP_LEDS + i * stripLenght, stripLenght,
                   CHSV(FREQUENCY.lmhTransform.colors[i], 255,
                        FREQUENCY.lmhTransform.bright[i]));
      }
      break;
    }
    case 2:
      if (FREQUENCY.lmhTransform.oneLine.mode == 4) {
        if (FREQUENCY.lmhTransform.isFlash[2])
          fillLeds(CHSV(FREQUENCY.lmhTransform.colors[2], 255,
                        FREQUENCY.lmhTransform.bright[2]));
        else if (FREQUENCY.lmhTransform.isFlash[1])
          fillLeds(CHSV(FREQUENCY.lmhTransform.colors[1], 255,
                        FREQUENCY.lmhTransform.bright[1]));
        else if (FREQUENCY.lmhTransform.isFlash[0])
          fillLeds(CHSV(FREQUENCY.lmhTransform.colors[0], 255,
                        FREQUENCY.lmhTransform.bright[0]));
        else
          silence();
      } else {
        if (FREQUENCY.lmhTransform.isFlash[FREQUENCY.lmhTransform.oneLine.mode])
          fillLeds(CHSV(FREQUENCY.lmhTransform
                            .colors[FREQUENCY.lmhTransform.oneLine.mode],
                        255,
                        FREQUENCY.lmhTransform
                            .bright[FREQUENCY.lmhTransform.oneLine.mode]));
        else
          silence();
      }
      break;
    case 3:
      //Показывает обычно просто средние - т.к. цепляет их чаще, низки почти не
      //попадают
      // if moved from end here.
      if (millis() - FREQUENCY.lmhTransform.runningFrequency.runDelay >
          FREQUENCY.lmhTransform.runningFrequency.speed) {
        FREQUENCY.lmhTransform.runningFrequency.runDelay = millis();
        if (FREQUENCY.lmhTransform.runningFrequency.mode == 4) {
          if (FREQUENCY.lmhTransform.isFlash[2])
            STRIP_LEDS[GLOBAL.halfLedsNum] =
                CHSV(FREQUENCY.lmhTransform.colors[2], 255,
                     FREQUENCY.lmhTransform.bright[2]);
          else if (FREQUENCY.lmhTransform.isFlash[1])
            STRIP_LEDS[GLOBAL.halfLedsNum] =
                CHSV(FREQUENCY.lmhTransform.colors[1], 255,
                     FREQUENCY.lmhTransform.bright[1]);
          else if (FREQUENCY.lmhTransform.isFlash[0])
            STRIP_LEDS[GLOBAL.halfLedsNum] =
                CHSV(FREQUENCY.lmhTransform.colors[0], 255,
                     FREQUENCY.lmhTransform.bright[0]);
          else
            STRIP_LEDS[GLOBAL.halfLedsNum] =
                CHSV(GLOBAL.disabledColor, 255, GLOBAL.disabledBrightness);
        } else {
          if (FREQUENCY.lmhTransform
                  .isFlash[FREQUENCY.lmhTransform.oneLine.mode]) {
            STRIP_LEDS[GLOBAL.halfLedsNum] =
                CHSV(FREQUENCY.lmhTransform
                         .colors[FREQUENCY.lmhTransform.oneLine.mode],
                     255,
                     FREQUENCY.lmhTransform
                         .bright[FREQUENCY.lmhTransform.oneLine.mode]);
          } else {
            STRIP_LEDS[GLOBAL.halfLedsNum] =
                CHSV(GLOBAL.disabledColor, 255, GLOBAL.disabledBrightness);
          }
        }
        STRIP_LEDS[GLOBAL.halfLedsNum - 1] = STRIP_LEDS[GLOBAL.halfLedsNum];
        for (uint16_t i = 0; i < GLOBAL.halfLedsNum - 1; i++) {
          STRIP_LEDS[i] = STRIP_LEDS[i + 1];
          STRIP_LEDS[GLOBAL.numLeds - i - 1] = STRIP_LEDS[i];
        }
      }
      break;
  }
}

float calcSoundLevel(float level) {
  // фильтруем по нижнему порогу шумов
  level = map(level, VU.signalThreshold, 1023, 0, 500);
  level = constrain(level, 0, 500);  // ограничиваем диапазон
  // возводим в степень (для большей чёткости работы)
  return pow(level, GLOBAL.expCoeffincient);
}

void silence() {
  fill_solid(STRIP_LEDS, GLOBAL.numLeds,
             CHSV(GLOBAL.disabledColor, 255, GLOBAL.disabledBrightness));
}

void fillLeds(CHSV color) { fill_solid(STRIP_LEDS, GLOBAL.numLeds, color); }

bool isReceiving;
bool isMessageReceived;
String bluetoothMessage;
void checkBluetooth() {
  if (bluetoothSerial.available() > 0) {
    readBluetooth();
  }
  if (isMessageReceived) {
    processMessage();
  }
}

void readBluetooth() {
  char incomingByte = bluetoothSerial.read();
  if (isReceiving) {
    if (incomingByte == END_BYTE) {
      isReceiving = false;
      isMessageReceived = true;
    } else {
      bluetoothMessage += incomingByte;
    }
  }
  if (incomingByte == START_BYTE) {
    isReceiving = true;
    bluetoothMessage = "";
  }
}

void processMessage() { isMessageReceived = false; }