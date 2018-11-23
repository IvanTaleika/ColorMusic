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
#define MLED_ON HIGH
#define LED_PIN 4   // пин DI светодиодной ленты
#define POT_GND A0  // пин земля для потенциометра

SoftwareSerial bluetoothSerial(4, 3);  // RX | TX
#define START_BYTE '$'
#define END_BYTE '^'
// лента
#define NUM_LEDS 102  // количество светодиодов  //В приложении

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
  // NOT setted
  uint16_t numLeds = 100;
  uint16_t halfLedsNum = numLeds / 2;
} GLOBAL;

struct Backlight {
  uint8_t mode = 0;
  uint8_t defaultColor = 150;
  // mode 1
  uint8_t defaultSaturation = 200;

  // mode 2
  uint8_t colorChangeDelay = 100;
  unsigned long colorChangeTime;
  uint8_t currentColor;

  // mode 3
  uint8_t rainbowColorChangeStep = 3;
  uint8_t rainbowStep = 5;
} BACKLIGHT;

// режим стробоскопа
struct Strobe {
  unsigned long previousFlashTime;
  uint8_t color = HUE_YELLOW;
  uint8_t saturation = 0;
  uint8_t bright = 0;
  uint8_t brightStep = 100;
  uint8_t duty = 20;
  uint16_t cycleDelay = 100;  // период вспышек, миллисекунды
} STROBE;

struct VuAnalyzer {
  // коэффициент перевода для палитры
  float colorToLed;
  uint16_t signalThreshold = 15;
  uint16_t averageSoundLevel = 50;
  uint16_t maxSoundLevel = 100;

  uint8_t rainbowStep = 5;
  float smooth = 0.3;
  bool isRainbowOn = false;
  uint8_t rainbowColor = 0;
  unsigned long rainbowTimer;

  static const uint8_t additionalThreshold = 13;
  void setAutoThreshold() {
    int maxLevel = 0;  // максимум
    int level;
    for (uint8_t i = 0; i < 200; i++) {
      level = analogRead(SOUND_R);  // делаем 200 измерений
      if (level > maxLevel) {       // ищем максимумы
        maxLevel = level;           // запоминаем
      }
      delay(4);  // ждём 4мс
    }
    // нижний порог как максимум тишины  некая величина
    signalThreshold = maxLevel + additionalThreshold;
  }
} VU;

void analyzeAudio() {
  for (int i = 0; i < FHT_N; i++) {
    fht_input[i] = analogRead(SOUND_R_FREQ);  // put real data into bins
  }
  fht_window();   // window the data for better frequency response
  fht_reorder();  // reorder the data before doing the fht
  fht_run();      // process the data in the fht
  fht_mag_log();  // take the output of the fht
}
struct FrequencyAnalyzer {
  uint16_t signalThreshold = 40;
  uint8_t mode;
  struct RunningFrequency {
    uint8_t speed = 10;
    unsigned long runDelay;
    int8_t mode;
  } runningFrequency;

  struct FullTransform {
    uint8_t smooth;
    uint8_t lowFrequencyHue = HUE_RED;
    uint8_t hueStep = 5;

    float ledsForFreq;
    float maxFrequency;
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
} FREQUENCY;

// отрисовка
#define MODE 1       // режим при запуске
#define MAIN_LOOP 5  // период основного цикла отрисовки (по умолчанию 5)
unsigned long main_timer;
// 1 - используем потенциометр, 0 - используется внутренний источник
// опорного напряжения 1.1 В
#define POTENT 0
// разрешить настройку нижнего порога шумов при запуске (по умолч. 0)
#define AUTO_LOW_PASS 0
// шкала громкости
// коэффициент громкости (максимальное равно срднему * этот коэф) (по
// умолчанию 1.8)
#define MAX_COEF 1.8

// режим цветомузыки
// коэффициент плавности анимации частот (по умолчанию 0.8)
float SMOOTH_FREQ = 0.8;
// коэффициент порога для "вспышки" цветомузыки (по умолчанию 1.5)
float MAX_COEF_FREQ = 1.2;
// шаг уменьшения яркости в режиме цветомузыки (чем больше, тем быстрее
// гаснет)
#define SMOOTH_STEP 20
#define LOW_COLOR HUE_RED      // цвет низких частот
#define MID_COLOR HUE_GREEN    // цвет средних
#define HIGH_COLOR HUE_YELLOW  // цвет высоких

// ------------------------------ ДЛЯ РАЗРАБОТЧИКОВ
// --------------------------------

CRGB leds[NUM_LEDS];  // GLOBAL
// градиент-палитра от зелёного к красному
DEFINE_GRADIENT_PALETTE(soundlevel_gp){
    0,   0,   255, 0,  // green
    100, 255, 255, 0,  // yellow
    150, 255, 100, 0,  // orange
    200, 255, 50,  0,  // red
    255, 255, 0,   0   // red
};
CRGBPalette32 myPal = soundlevel_gp;

uint8_t Rlenght, Llenght;
float RsoundLevel, RsoundLevel_f;
float LsoundLevel, LsoundLevel_f;

float averK = 0.006;  // GLOBAL

int RcurrentLevel, LcurrentLevel;
int frequency[3];
float colorMusic_f[3], colorMusic_aver[3];
bool colorMusicFlash[3];
int thisBright[3];

bool running_flag[3];

#define cbi(sfr, bit) (_SFR_BYTE(sfr) &= ~_BV(bit))
#define sbi(sfr, bit) (_SFR_BYTE(sfr) |= _BV(bit))
// ------------------------------ ДЛЯ РАЗРАБОТЧИКОВ
// --------------------------------

void setup() {
  Serial.begin(9600);

  pinMode(MLED_PIN, OUTPUT);  //Режим пина для светодиода режима на выход
  digitalWrite(MLED_PIN, !MLED_ON);  //Выключение светодиода режима

  pinMode(POT_GND, OUTPUT);
  digitalWrite(POT_GND, LOW);

  initLeds();

  // TODO for wire

  if (POTENT) {
    analogReference(EXTERNAL);
  } else {
    analogReference(INTERNAL);
  }

  sbi(ADCSRA, ADPS2);
  cbi(ADCSRA, ADPS1);
  sbi(ADCSRA, ADPS0);

  if (AUTO_LOW_PASS) {  // если разрешена автонастройка нижнего порога шумов
    setThreshold();
  }
}
void loop() {
  checkBluetooth();
  if (GLOBAL.isOn && millis() - main_timer > MAIN_LOOP) {
    processSound();
    FastLED.show();
    if (GLOBAL.currentMode != 7) {  // 7 режиму не нужна очистка!!!
      FastLED.clear();  // очистить массив пикселей
    }
    main_timer = millis();  // сбросить таймер
  }
}

void initLeds() {
  // TODO: 2811?
  FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS)
      .setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(GLOBAL.enabledBrightness);
  GLOBAL.halfLedsNum = GLOBAL.numLeds / 2;
  VU.colorToLed = (float)255 / GLOBAL.halfLedsNum;
  FREQUENCY.fullTransform.ledsForFreq = GLOBAL.halfLedsNum / 30;
}

void processSound() {
  // сбрасываем значения
  RsoundLevel = 0;
  LsoundLevel = 0;
  switch (GLOBAL.currentMode) {
    case 0:
      processLevel();
    case 1:
      processFrequency();
    case 2:
      processStrobe();
    case 3:
      backlightAnimation();
  }
}

void processLevel() {
  for (uint8_t i = 0; i < 100; i++) {     // делаем 100 измерений
    RcurrentLevel = analogRead(SOUND_R);  // с правого
    if (RsoundLevel < RcurrentLevel) {
      RsoundLevel = RcurrentLevel;  // ищем максимальное
    }
    if (!GLOBAL.isMono || !GLOBAL.isMicro) {
      LcurrentLevel = analogRead(SOUND_L);  // c левого канала
      if (LsoundLevel < LcurrentLevel) {
        LsoundLevel = LcurrentLevel;  // ищем максимальное
      }
    }
  }
  RsoundLevel = calcSoundLevel(RsoundLevel);
  // фильтр скользящее среднее
  RsoundLevel_f = RsoundLevel * VU.smooth + RsoundLevel_f * (1 - VU.smooth);
  // For stereo
  if (!GLOBAL.isMono || !GLOBAL.isMicro) {
    LsoundLevel = calcSoundLevel(LsoundLevel);
    LsoundLevel_f = LsoundLevel * VU.smooth + LsoundLevel_f * (1 - VU.smooth);
  } else {
    LsoundLevel_f = RsoundLevel_f;  // если моно, то левый = правому
  }
  // если значение выше порога - начинаем самое интересное
  // TODO WTF is 15?
  if (RsoundLevel_f > 15 && LsoundLevel_f > 15) {
    // расчёт общей средней громкости с обоих каналов, фильтрация.
    // Фильтр очень медленный, сделано специально для автогромкости
    VU.averageSoundLevel = (float)(RsoundLevel_f + LsoundLevel_f) / 2 * averK +
                           VU.averageSoundLevel * (1 - averK);
    // принимаем максимальную громкость шкалы как среднюю, умноженную на
    // некоторый коэффициент MAX_COEF
    VU.maxSoundLevel = VU.averageSoundLevel * MAX_COEF;
    // преобразуем сигнал в длину ленты (где GLOBAL.halfLedsNum это половина
    // количества светодиодов)
    Rlenght = map(RsoundLevel_f, 0, VU.maxSoundLevel, 0, GLOBAL.halfLedsNum);
    Llenght = map(LsoundLevel_f, 0, VU.maxSoundLevel, 0, GLOBAL.halfLedsNum);
    // ограничиваем до макс. числа светодиодов
    Rlenght = constrain(Rlenght, 0, GLOBAL.halfLedsNum);
    Llenght = constrain(Llenght, 0, GLOBAL.halfLedsNum);
    vuAnimation();  // отрисовать
  } else if (GLOBAL.disabledBrightness > 5) {
    silence();
  }
}

void fullFrequencyTransform() {
  uint8_t freq_max = 0;
  uint8_t* frequency = FREQUENCY.fullTransform.frequency;
  for (uint8_t i = 0; i < 30; i++) {
    if (fht_log_out[i + 2] > freq_max) {
      freq_max = fht_log_out[i + 2];
    }
    if (freq_max < 5) {
      freq_max = 5;
    }

    if (frequency[i] < fht_log_out[i + 2]) {
      frequency[i] = fht_log_out[i + 2];
    }
    if (frequency[i] > FREQUENCY.fullTransform.smooth) {
      frequency[i] -= FREQUENCY.fullTransform.smooth;
    } else {
      frequency[i] = 0;
    }
  }
  FREQUENCY.fullTransform.maxFrequency =
      freq_max * averK + FREQUENCY.fullTransform.maxFrequency * (1 - averK);
}

void lowMediumHighTransform() {
  frequency[0] = 0;
  frequency[1] = 0;
  frequency[2] = 0;
  for (int i = 0; i < 32; i++) {
    if (fht_log_out[i] < FREQUENCY.signalThreshold) {
      fht_log_out[i] = 0;
    }
  }
  // низкие частоты, выборка со 2 по 5 тон (0 и 1 зашумленные!)
  for (uint8_t i = 2; i < 6; i++) {
    if (fht_log_out[i] > frequency[0]) {
      frequency[0] = fht_log_out[i];
    }
  }
  // средние частоты, выборка с 6 по 10 тон
  for (uint8_t i = 6; i < 11; i++) {
    if (fht_log_out[i] > frequency[1]) {
      frequency[1] = fht_log_out[i];
    }
  }
  // высокие частоты, выборка с 11 по 31 тон
  for (uint8_t i = 11; i < 32; i++) {
    if (fht_log_out[i] > frequency[2]) {
      frequency[2] = fht_log_out[i];
    }
  }

  // TODO rewrite
  //Звук с одинаковыми частотами в итоге вырубит отображение вообще
  for (uint8_t i = 0; i < 3; i++) {
    // общая фильтрация
    colorMusic_aver[i] =
        frequency[i] * averK + colorMusic_aver[i] * (1 - averK);
    // локальная
    colorMusic_f[i] =
        frequency[i] * SMOOTH_FREQ + colorMusic_f[i] * (1 - SMOOTH_FREQ);
    if (colorMusic_f[i] > ((float)colorMusic_aver[i] * MAX_COEF_FREQ)) {
      thisBright[i] = 255;
      colorMusicFlash[i] = true;
      running_flag[i] = true;
    } else {
      colorMusicFlash[i] = false;
    }
    if (thisBright[i] >= 0) {
      thisBright[i] -= SMOOTH_STEP;
    }
    if (thisBright[i] < GLOBAL.disabledBrightness) {
      thisBright[i] = GLOBAL.disabledBrightness;
      running_flag[i] = false;
    }
  }
}

void processFrequency() {
  analyzeAudio();
  if (FREQUENCY.mode == 4) {
    fullFrequencyTransform();
  } else {
    lowMediumHighTransform();
  }
  frequencyAnimation();
}

void vuAnimation() {
  uint8_t count = 0;
  uint16_t halfLedsNum = GLOBAL.halfLedsNum;
  float colorToLed = VU.colorToLed;

  if (VU.isRainbowOn) {
    // заливка по палитре радуга
    if (millis() - VU.rainbowTimer > 30) {
      VU.rainbowTimer = millis();
      VU.rainbowColor = VU.rainbowColor + VU.rainbowStep;
    }
    count = 0;
    // RainbowColors_p -  default FastLED pallet
    for (int i = (halfLedsNum - 1); i > ((halfLedsNum - 1) - Rlenght);
         i--, count++) {
      leds[i] = ColorFromPalette(RainbowColors_p,
                                 (count * colorToLed) / 2 - VU.rainbowColor);
    }
    count = 0;
    for (int i = (halfLedsNum); i < (halfLedsNum + Llenght); i++, count++) {
      leds[i] = ColorFromPalette(RainbowColors_p,
                                 (count * colorToLed) / 2 - VU.rainbowColor);
    }
  } else {
    // заливка по палитре " от зелёного к красному"
    for (int i = (halfLedsNum - 1); i > ((halfLedsNum - 1) - Rlenght);
         i--, count++) {
      leds[i] = ColorFromPalette(myPal, (count * colorToLed));
    }
    count = 0;
    for (int i = (GLOBAL.halfLedsNum); i < (GLOBAL.halfLedsNum + Llenght);
         i++, count++) {
      leds[i] = ColorFromPalette(myPal, (count * colorToLed));
    }
  }
  if (GLOBAL.disabledBrightness > 0) {
    colorEmptyLeds();
  }
}

// DONE!
void silence() {
  fill_solid(leds, GLOBAL.numLeds,
             CHSV(GLOBAL.disabledColor, 255, GLOBAL.disabledBrightness));
}
// DONE!
void fillLeds(CHSV color) { fill_solid(leds, GLOBAL.numLeds, color); }

// DONE!
void backlightAnimation() {
  switch (BACKLIGHT.mode) {
    case 0:
      fillLeds(CHSV(BACKLIGHT.defaultColor, BACKLIGHT.defaultSaturation, 255));
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
        leds[i] = CHSV(rainbowStep, 255, 255);
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
    fillLeds(CHSV(STROBE.color, STROBE.saturation, STROBE.bright));
  } else {
    fillLeds(CHSV(GLOBAL.disabledColor, 255, GLOBAL.disabledBrightness));
  }
}

void frequencyAnimation() {
  switch (FREQUENCY.mode) {
    // DONE!
    case 0: {
      uint16_t strip = GLOBAL.numLeds / 5;
      fill_solid(leds, strip, CHSV(HIGH_COLOR, 255, thisBright[2]));
      fill_solid(leds + strip, strip, CHSV(MID_COLOR, 255, thisBright[1]));
      fill_solid(leds + 2 * strip, strip, CHSV(LOW_COLOR, 255, thisBright[0]));
      fill_solid(leds + 3 * strip, strip, CHSV(MID_COLOR, 255, thisBright[1]));
      fill_solid(leds + 4 * strip, strip, CHSV(HIGH_COLOR, 255, thisBright[2]));
      break;
    }
    // DONE!
    case 1: {
      uint16_t strip = GLOBAL.numLeds / 3;
      fill_solid(leds, strip, CHSV(HIGH_COLOR, 255, thisBright[2]));
      fill_solid(leds + strip, strip, CHSV(MID_COLOR, 255, thisBright[1]));
      fill_solid(leds + 2 * strip, strip, CHSV(LOW_COLOR, 255, thisBright[0]));
      break;
    }
    case 2:
      // FIXME
      switch (1) {
        case 0:
          if (colorMusicFlash[2])
            fillLeds(CHSV(HIGH_COLOR, 255, thisBright[2]));
          else if (colorMusicFlash[1])
            fillLeds(CHSV(MID_COLOR, 255, thisBright[1]));
          else if (colorMusicFlash[0])
            fillLeds(CHSV(LOW_COLOR, 255, thisBright[0]));
          else
            silence();
          break;
        case 1:
          if (colorMusicFlash[2])
            fillLeds(CHSV(HIGH_COLOR, 255, thisBright[2]));
          else
            silence();
          break;
        case 2:
          if (colorMusicFlash[1])
            fillLeds(CHSV(MID_COLOR, 255, thisBright[1]));
          else
            silence();
          break;
        case 3:
          if (colorMusicFlash[0])
            fillLeds(CHSV(LOW_COLOR, 255, thisBright[0]));
          else
            silence();
          break;
      }
      break;
    case 3:
      //Показывает обычно просто средние - т.к. цепляет их чаще, низки почти не
      //попадают
      // if moved from end here.
      if (millis() - FREQUENCY.runningFrequency.runDelay >
          FREQUENCY.runningFrequency.speed) {
        FREQUENCY.runningFrequency.runDelay = millis();
        switch (FREQUENCY.runningFrequency.mode) {
          case 0:
            if (running_flag[2])
              leds[GLOBAL.halfLedsNum] = CHSV(HIGH_COLOR, 255, thisBright[2]);
            else if (running_flag[1])
              leds[GLOBAL.halfLedsNum] = CHSV(MID_COLOR, 255, thisBright[1]);
            else if (running_flag[0])
              leds[GLOBAL.halfLedsNum] = CHSV(LOW_COLOR, 255, thisBright[0]);
            else
              leds[GLOBAL.halfLedsNum] =
                  CHSV(GLOBAL.disabledColor, 255, GLOBAL.disabledBrightness);
            break;
          case 1:
            if (running_flag[2])
              leds[GLOBAL.halfLedsNum] = CHSV(HIGH_COLOR, 255, thisBright[2]);
            else
              leds[GLOBAL.halfLedsNum] =
                  CHSV(GLOBAL.disabledColor, 255, GLOBAL.disabledBrightness);
            break;
          case 2:
            if (running_flag[1])
              leds[GLOBAL.halfLedsNum] = CHSV(MID_COLOR, 255, thisBright[1]);
            else
              leds[GLOBAL.halfLedsNum] =
                  CHSV(GLOBAL.disabledColor, 255, GLOBAL.disabledBrightness);
            break;
          case 3:
            if (running_flag[0])
              leds[GLOBAL.halfLedsNum] = CHSV(LOW_COLOR, 255, thisBright[0]);
            else
              leds[GLOBAL.halfLedsNum] =
                  CHSV(GLOBAL.disabledColor, 255, GLOBAL.disabledBrightness);
            break;
        }
        leds[GLOBAL.halfLedsNum - 1] = leds[GLOBAL.halfLedsNum];
        for (uint16_t i = 0; i < GLOBAL.halfLedsNum - 1; i++) {
          leds[i] = leds[i + 1];
          leds[GLOBAL.numLeds - i - 1] = leds[i];
        }
      }
      break;
      // DONE!
    case 4:
      uint8_t hue = FREQUENCY.fullTransform.lowFrequencyHue;
      uint8_t stripSize;
      uint8_t bright;
      uint16_t position = 0;
      for (uint8_t i = 0; i < 30; i++) {
        stripSize = (float)i * FREQUENCY.fullTransform.ledsForFreq + 0.5;
        bright = map(FREQUENCY.fullTransform.frequency[30 - 1 - i], 0,
                     FREQUENCY.fullTransform.maxFrequency, 0, 255);
        fill_solid(leds + position, stripSize, CHSV(hue, 255, bright));
        fill_solid(&(leds[GLOBAL.numLeds - stripSize - position - 1]),
                   stripSize, CHSV(hue, 255, bright));
        hue += FREQUENCY.fullTransform.hueStep;
        position += stripSize;
      }
      break;
  }
}

void colorEmptyLeds() {
  CHSV this_dark = CHSV(GLOBAL.disabledColor, 255, GLOBAL.disabledBrightness);
  for (int i = ((GLOBAL.halfLedsNum - 1) - Rlenght); i > 0; i--) {
    leds[i] = this_dark;
  }
  for (int i = GLOBAL.halfLedsNum + Llenght; i < NUM_LEDS; i++) {
    leds[i] = this_dark;
  }
}

float calcSoundLevel(float level) {
  level = map(level, VU.signalThreshold, 1023, 0,
              500);  // фильтруем по нижнему порогу шумов
  level = constrain(level, 0, 500);  // ограничиваем диапазон
  level = pow(level, GLOBAL.expCoeffincient);  // возводим в степень (для
                                               // большей чёткости работы)
  return level;
}

void setThreshold() {
  delay(1000);  // ждём инициализации АЦП
  VU.setAutoThreshold();
  FREQUENCY.setAutoThreshold();
}

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
