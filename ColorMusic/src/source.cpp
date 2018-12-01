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
#include "source.h"
#include <FHT.h>  // преобразование Хартли
#include <SoftwareSerial.h>

#define MAX_PACKAGE_AWAIT_TIME 4000
// градиент-палитра от зелёного к красному
DEFINE_GRADIENT_PALETTE(soundlevel_gp){
    0,   0,   255, 0,  // green
    100, 255, 255, 0,  // yellow
    150, 255, 100, 0,  // orange
    200, 255, 50,  0,  // red
    255, 255, 0,   0   // red
};
SoftwareSerial BLUETOOTH_SERIAL(2, 3);  // arduino RX | TX
CRGBPalette32 MY_PAL = soundlevel_gp;
unsigned long PREVIOUS_PACKAGE_TIME;
unsigned long LOOP_TIMER = 0;
uint8_t MODE = 0;
bool IS_STRIP_ON = false;
CRGB* STRIP_LEDS;
Global GLOBAL;
VuAnalyzer VU;
FrequencyAnalyzer FREQUENCY;
Strob STROB;
Backlight BACKLIGHT;
LowMediumHighFrequency LHM_FREQUENCY;
FullRangeFrequency FULL_FREQUENCY;

void setup() {
  Serial.begin(9600);
  BLUETOOTH_SERIAL.begin(9600);
  pinMode(MLED_PIN, OUTPUT);  //Режим пина для светодиода режима на выход
  digitalWrite(MLED_PIN, LOW);  //Выключение светодиода режима

  setReferenceVoltage();

  sbi(ADCSRA, ADPS2);
  cbi(ADCSRA, ADPS1);
  sbi(ADCSRA, ADPS0);

  initLeds();
  setThreshold();
}
void setReferenceVoltage() {
  if (GLOBAL.settings.isMicro) {
    analogReference(DEFAULT);
  } else {
    analogReference(INTERNAL);
  }
}
void initLeds() {
  delete[] STRIP_LEDS;
  STRIP_LEDS = new CRGB[GLOBAL.settings.numLeds];
  FastLED.addLeds<WS2811, LED_PIN, GRB>(STRIP_LEDS, GLOBAL.settings.numLeds)
      .setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(GLOBAL.settings.enabledBrightness);
  GLOBAL.halfLedsNum = GLOBAL.settings.numLeds / 2;
  VU.colorToLed = 255. / GLOBAL.halfLedsNum;
  FULL_FREQUENCY.ledsForFreq = GLOBAL.halfLedsNum / 30.;
}

void setThreshold() {
  delay(1000);  // ждём инициализации АЦП
  setVuThreshold();
  // Serial.print("setVuThreshold=");
  // Serial.println(VU.signalThreshold);
  setFrequencyThreshold();
  // Serial.print("setFrequencyThreshold=");
  // Serial.println(FREQUENCY.signalThreshold);
}

void setVuThreshold() {
  int maxLevel = 0;  // максимум
  int level;
  for (uint8_t i = 0; i < 50; i++) {
    analogRead(GLOBAL.soundRightPin);  // Clear trash in port
  }
  for (uint8_t i = 0; i < 200; i++) {
    level = analogRead(GLOBAL.soundRightPin);  // делаем 200 измерений
    if (level > maxLevel) {                    // ищем максимумы
      maxLevel = level;                        // запоминаем
    }
    delay(4);  // ждём 4мс
  }
  // нижний порог как максимум тишины  некая величина
  VU.signalThreshold = maxLevel + VU.additionalThreshold;
}

void setFrequencyThreshold() {
  int maxLevel = 0;  // максимум
  int level;
  for (uint8_t i = 0; i < 100; i++) {   // делаем 100 измерений
    analyzeAudio();                     // разбить в спектр
    for (uint8_t j = 2; j < 32; j++) {  // первые 2 канала - хлам
      level = fht_log_out[j];

      if (level > maxLevel) {  // ищем максимумы
        maxLevel = level;      // запоминаем
      }
    }
    delay(4);  // ждём 4мс
  }
  // нижний порог как максимум тишины некая величина
  FREQUENCY.signalThreshold = maxLevel + FREQUENCY.additionalThreshold;
}

void loop() {
  checkBluetooth();
  if (IS_STRIP_ON && millis() - LOOP_TIMER > LOOP_DELAY) {
    processSound();
    FastLED.show();
    if (MODE != 7) {    // 7 режиму не нужна очистка!!!
      FastLED.clear();  // очистить массив пикселей
    }
    LOOP_TIMER = millis();  // сбросить таймер
  }
}

void processSound() {
  switch (MODE) {
    case 0:
      processLevel();
      return;
    case 1:
      lmhFrequencyTransform();
      return;
    case 2:
      fullFrequencyTransform();
      return;
    case 3:
      processStrobe();
      return;
    case 4:
      backlightAnimation();
      return;
  }
}

void processLevel() {
  uint16_t rMax = 0, lMax = 0;
  uint16_t rLevel, lLevel;

  for (uint8_t i = 0; i < 100; i++) {
    rLevel = analogRead(GLOBAL.soundRightPin);
    if (rMax < rLevel) {
      rMax = rLevel;
    }
    if (GLOBAL.settings.isStereo && !GLOBAL.settings.isMicro) {
      lLevel = analogRead(Global::WIRE_LEFT_PIN);
      if (lMax < lLevel) {
        lMax = lLevel;
      }
    }
  }
  // Serial.print("rMax=");
  // Serial.println(rMax);
  // Serial.print("lMax=");
  // Serial.println(lMax);
  // фильтр скользящее среднее
  VU.rAverage =
      (rMax * VU.settings.smooth + VU.rAverage * (10 - VU.settings.smooth)) /
      10;

  if (GLOBAL.settings.isStereo && !GLOBAL.settings.isMicro) {
    lMax = calcSoundLevel(lMax);
    VU.lAverage =
        (lMax * VU.settings.smooth + VU.lAverage * (10 - VU.settings.smooth)) /
        10;
  } else {
    VU.lAverage = VU.rAverage;
  }

  // расчёт общей средней громкости с обоих каналов, фильтрация.
  // Фильтр очень медленный, сделано специально для автогромкости
  VU.averageLevel = (float)(VU.rAverage + VU.lAverage) / 2. * averK +
                    VU.averageLevel * (1 - averK);
  if (VU.rAverage > VU.signalThreshold || VU.lAverage > VU.signalThreshold) {
    // принимаем максимальную громкость шкалы как среднюю, умноженную на
    // некоторый коэффициент maxMultiplier
    VU.maxLevel = VU.averageLevel * VU.maxMultiplier;

    // преобразуем сигнал в длину ленты
    uint16_t rLength = map(VU.rAverage, 0, VU.maxLevel, 0, GLOBAL.halfLedsNum);
    uint16_t lLength = map(VU.lAverage, 0, VU.maxLevel, 0, GLOBAL.halfLedsNum);
    // ограничиваем до макс. числа светодиодов
    rLength = constrain(rLength, 0, GLOBAL.halfLedsNum);
    lLength = constrain(lLength, 0, GLOBAL.halfLedsNum);

    vuAnimation(rLength, lLength);
  } else if (GLOBAL.settings.disabledBrightness > 0) {
    silence();
  }
}

void vuAnimation(uint16_t rLength, uint16_t lLength) {
  uint8_t count = 0;
  uint16_t halfLedsNum = GLOBAL.halfLedsNum;
  float colorToLed = VU.colorToLed;
  if (VU.settings.isRainbowOn) {
    // заливка по палитре радуга
    if (millis() - VU.rainbowTimer > 30) {
      VU.rainbowTimer = millis();
      VU.rainbowColor = VU.rainbowColor + VU.settings.rainbowStep;
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
  if (GLOBAL.settings.disabledBrightness > 0) {
    colorEmptyLeds(halfLedsNum - rLength, halfLedsNum - lLength);
  }
}

void colorEmptyLeds(uint16_t rDisabled, uint16_t lDisabled) {
  CHSV dark = CHSV(GLOBAL.settings.disabledHue, 255,
                   GLOBAL.settings.disabledBrightness);
  if (rDisabled > 0) {
    fill_solid(STRIP_LEDS, rDisabled, dark);
  }
  if (lDisabled > 0) {
    fill_solid(&(STRIP_LEDS[GLOBAL.settings.numLeds - lDisabled]), lDisabled,
               dark);
  }
}

void backlightAnimation() {
  switch (BACKLIGHT.settings.mode) {
    case 0:
      fillLeds(BACKLIGHT.settings.color);
      break;
    case 1:
      if (millis() - BACKLIGHT.colorChangeTime >
          BACKLIGHT.settings.colorChangeDelay) {
        BACKLIGHT.colorChangeTime = millis();
        BACKLIGHT.currentHue++;
      }
      fillLeds(CHSV(BACKLIGHT.currentHue, BACKLIGHT.settings.color.s,
                    BACKLIGHT.settings.color.v));
      break;
    case 2:
      if (millis() - BACKLIGHT.colorChangeTime >
          BACKLIGHT.settings.colorChangeDelay) {
        BACKLIGHT.colorChangeTime = millis();
        BACKLIGHT.currentHue += BACKLIGHT.settings.rainbowColorStep;
      }
      uint8_t rainbowStep = BACKLIGHT.currentHue;
      for (uint16_t i = 0; i < GLOBAL.settings.numLeds; i++) {
        STRIP_LEDS[i] = CHSV(rainbowStep, BACKLIGHT.settings.color.s,
                             BACKLIGHT.settings.color.v);
        rainbowStep += BACKLIGHT.settings.rainbowStep;
      }
      break;
  }
}
void processStrobe() {
  unsigned long delay = millis() - STROB.previousFlashTime;
  if (delay > STROB.settings.flashPeriod) {
    STROB.previousFlashTime = millis();
  } else if (delay > STROB.settings.flashPeriod * STROB.settings.duty / 100) {
    if (STROB.brightness > STROB.settings.brightStep) {
      STROB.brightness -= STROB.settings.brightStep;
    } else {
      STROB.brightness = 0;
    }
  } else if (STROB.brightness <
             STROB.settings.maxBrighness - STROB.settings.brightStep) {
    STROB.brightness += STROB.settings.brightStep;
  } else {
    STROB.brightness = STROB.settings.maxBrighness;
  }
  strobeAnimation();
}
void strobeAnimation() {
  if (STROB.brightness > 0) {
    fillLeds(
        CHSV(STROB.settings.hue, STROB.settings.saturation, STROB.brightness));
  } else {
    fillLeds(CHSV(GLOBAL.settings.disabledHue, 255,
                  GLOBAL.settings.disabledBrightness));
  }
}

void analyzeAudio() {
  for (int i = 0; i < FHT_N; i++) {
    fht_input[i] =
        analogRead(GLOBAL.soundFrequencyPin);  // put real data into bins
  }
  fht_window();   // window the data for better frequency response
  fht_reorder();  // reorder the data before doing the fht
  fht_run();      // process the data in the fht
  fht_mag_log();  // take the output of the fht
}

void fullFrequencyTransform() {
  uint8_t freqMax = 0;
  uint8_t* frequency = FULL_FREQUENCY.frequency;
  for (uint8_t i = 0; i < 30; i++) {
    if (fht_log_out[i + 2] > freqMax) {
      freqMax = fht_log_out[i + 2];
    }
    if (freqMax < 5) {
      freqMax = 5;
    }

    if (frequency[i] < fht_log_out[i + 2]) {
      frequency[i] = fht_log_out[i + 2];
    } else if (frequency[i] > FULL_FREQUENCY.settings.smooth) {
      frequency[i] -= FULL_FREQUENCY.settings.smooth;
    } else {
      frequency[i] = 0;
    }
  }
  FULL_FREQUENCY.maxFrequency =
      freqMax * averK + FULL_FREQUENCY.maxFrequency * (1 - averK);
  fullFrequencyAnimation();
}

void fullFrequencyAnimation() {
  uint8_t hue = FULL_FREQUENCY.settings.lowFrequencyHue;
  uint8_t stripSize;
  uint8_t bright;
  uint16_t position = 0;
  for (uint8_t i = 0; i < 30; i++) {
    stripSize = (float)i * FULL_FREQUENCY.ledsForFreq + 0.5;
    bright = map(FULL_FREQUENCY.frequency[30 - 1 - i], 0,
                 FULL_FREQUENCY.maxFrequency, 0, 255);
    fill_solid(STRIP_LEDS + position, stripSize, CHSV(hue, 255, bright));
    fill_solid(
        &(STRIP_LEDS[GLOBAL.settings.numLeds - stripSize - position - 1]),
        stripSize, CHSV(hue, 255, bright));
    hue += FULL_FREQUENCY.settings.hueStep;
    position += stripSize;
  }
}

void lmhFrequencyTransform() {
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
  uint8_t value;
  for (uint8_t i = 0; i < 3; i++) {
    value = (frequency[i] * LHM_FREQUENCY.settings.smooth +
             LHM_FREQUENCY.lastFrequency[i] *
                 (10 - LHM_FREQUENCY.settings.smooth)) /
            10;
    if (value >
        LHM_FREQUENCY.lastFrequency[i] * LHM_FREQUENCY.flashMultiplier) {
      LHM_FREQUENCY.bright[i] = 255;
      LHM_FREQUENCY.isFlash[i] = true;
    } else {
      LHM_FREQUENCY.isFlash[i] = false;
      if (LHM_FREQUENCY.bright[i] < LHM_FREQUENCY.settings.step ||
          LHM_FREQUENCY.bright[i] < GLOBAL.settings.disabledBrightness) {
        LHM_FREQUENCY.bright[i] = GLOBAL.settings.disabledBrightness;
      } else {
        LHM_FREQUENCY.bright[i] -= LHM_FREQUENCY.settings.step;
      }
    }
    LHM_FREQUENCY.lastFrequency[i] = value;
  }
  lmhFrequencyAnimation();
}

void lmhFrequencyAnimation() {
  switch (LHM_FREQUENCY.settings.mode) {
    case 0: {
      uint16_t stripLenght = GLOBAL.settings.numLeds / 5;
      fill_solid(
          STRIP_LEDS, stripLenght,
          CHSV(LHM_FREQUENCY.settings.colors[2], 255, LHM_FREQUENCY.bright[2]));
      fill_solid(
          STRIP_LEDS + stripLenght, stripLenght,
          CHSV(LHM_FREQUENCY.settings.colors[1], 255, LHM_FREQUENCY.bright[1]));
      fill_solid(
          STRIP_LEDS + 2 * stripLenght, stripLenght,
          CHSV(LHM_FREQUENCY.settings.colors[0], 255, LHM_FREQUENCY.bright[0]));
      fill_solid(
          STRIP_LEDS + 3 * stripLenght, stripLenght,
          CHSV(LHM_FREQUENCY.settings.colors[1], 255, LHM_FREQUENCY.bright[1]));
      fill_solid(
          STRIP_LEDS + 4 * stripLenght, stripLenght,
          CHSV(LHM_FREQUENCY.settings.colors[2], 255, LHM_FREQUENCY.bright[2]));
      break;
    }
    case 1: {
      uint16_t stripLenght = GLOBAL.settings.numLeds / 3;
      for (uint8_t i = 0; i < 3; i++) {
        fill_solid(STRIP_LEDS + i * stripLenght, stripLenght,
                   CHSV(LHM_FREQUENCY.settings.colors[i], 255,
                        LHM_FREQUENCY.bright[i]));
      }
      break;
    }
    case 2:
      if (LHM_FREQUENCY.settings.oneLineMode == 3) {
        if (LHM_FREQUENCY.isFlash[2])
          fillLeds(CHSV(LHM_FREQUENCY.settings.colors[2], 255,
                        LHM_FREQUENCY.bright[2]));
        else if (LHM_FREQUENCY.isFlash[1])
          fillLeds(CHSV(LHM_FREQUENCY.settings.colors[1], 255,
                        LHM_FREQUENCY.bright[1]));
        else if (LHM_FREQUENCY.isFlash[0])
          fillLeds(CHSV(LHM_FREQUENCY.settings.colors[0], 255,
                        LHM_FREQUENCY.bright[0]));
        else
          silence();
      } else {
        if (LHM_FREQUENCY.isFlash[LHM_FREQUENCY.settings.oneLineMode])
          fillLeds(CHSV(
              LHM_FREQUENCY.settings.colors[LHM_FREQUENCY.settings.oneLineMode],
              255, LHM_FREQUENCY.bright[LHM_FREQUENCY.settings.oneLineMode]));
        else
          silence();
      }
      break;
    case 3:
      //Показывает обычно просто средние - т.к. цепляет их чаще, низки почти не
      //попадают
      if (millis() - LHM_FREQUENCY.runDelay > LHM_FREQUENCY.settings.speed) {
        // Serial.println("runningFrequency");
        // Serial.print("isFlash[2]=");
        // Serial.println(LHM_FREQUENCY.isFlash[2]);
        // Serial.print("isFlash[1]=");
        // Serial.println(LHM_FREQUENCY.isFlash[1]);
        // Serial.print("isFlash[0]=");
        // Serial.println(LHM_FREQUENCY.isFlash[0]);
        for (uint16_t i = 0; i < GLOBAL.halfLedsNum - 1; i++) {
          // Serial.print(i);
          // Serial.print(" ");

          STRIP_LEDS[i] = STRIP_LEDS[i + 1];
          STRIP_LEDS[GLOBAL.settings.numLeds - i - 1] = STRIP_LEDS[i];
        }
        LHM_FREQUENCY.runDelay = millis();
        if (LHM_FREQUENCY.settings.runningFrequencyMode == 3) {
          if (LHM_FREQUENCY.isFlash[2])
            STRIP_LEDS[GLOBAL.halfLedsNum] = CHSV(
                LHM_FREQUENCY.settings.colors[2], 255, LHM_FREQUENCY.bright[2]);
          else if (LHM_FREQUENCY.isFlash[1])
            STRIP_LEDS[GLOBAL.halfLedsNum] = CHSV(
                LHM_FREQUENCY.settings.colors[1], 255, LHM_FREQUENCY.bright[1]);
          else if (LHM_FREQUENCY.isFlash[0])
            STRIP_LEDS[GLOBAL.halfLedsNum] = CHSV(
                LHM_FREQUENCY.settings.colors[0], 255, LHM_FREQUENCY.bright[0]);
          else
            STRIP_LEDS[GLOBAL.halfLedsNum] =
                CHSV(GLOBAL.settings.disabledHue, 255,
                     GLOBAL.settings.disabledBrightness);
        } else {
          if (LHM_FREQUENCY
                  .isFlash[LHM_FREQUENCY.settings.runningFrequencyMode]) {
            STRIP_LEDS[GLOBAL.halfLedsNum] =
                CHSV(LHM_FREQUENCY.settings
                         .colors[LHM_FREQUENCY.settings.runningFrequencyMode],
                     255,
                     LHM_FREQUENCY
                         .bright[LHM_FREQUENCY.settings.runningFrequencyMode]);
          } else {
            STRIP_LEDS[GLOBAL.halfLedsNum] =
                CHSV(GLOBAL.settings.disabledHue, 255,
                     GLOBAL.settings.disabledBrightness);
          }
        }
        STRIP_LEDS[GLOBAL.halfLedsNum - 1] = STRIP_LEDS[GLOBAL.halfLedsNum];
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
  fill_solid(STRIP_LEDS, GLOBAL.settings.numLeds,
             CHSV(GLOBAL.settings.disabledHue, 255,
                  GLOBAL.settings.disabledBrightness));
}

void fillLeds(CHSV color) {
  fill_solid(STRIP_LEDS, GLOBAL.settings.numLeds, color);
}

void checkBluetooth() {
  if (BLUETOOTH_SERIAL.available() > 0) {
    int incomingByte = BLUETOOTH_SERIAL.read();
    Serial.print("StartByte=");
    Serial.println(incomingByte);
    BLUETOOTH_SERIAL.write((uint8_t)0);
    PREVIOUS_PACKAGE_TIME = millis();
    int target;
    do {
      target = readTarget();
      Serial.print("target main=");
      Serial.println((int)target);
      if (target < 0) {
        return;
      }
    } while (target == 0);
    uint8_t configSize = getSettingsSize(target);
    uint8_t* data = (uint8_t*)malloc(configSize + 1);
    uint8_t result;
    do {
      result = readData(data, configSize);
      Serial.print("result=");
      Serial.println((int)result);
      if (result < 0) {
        free(data);
        return;
      }
    } while (result == 0);
    setSettings(target, data, configSize);
    free(data);
  }
}

int readData(uint8_t* data, uint8_t configSize) {
  uint8_t index = 0;
  PREVIOUS_PACKAGE_TIME = millis();
  while (index < configSize + 1) {
    if (BLUETOOTH_SERIAL.available() > 0) {
      data[index] = BLUETOOTH_SERIAL.read();
      Serial.print("data[");
      Serial.print((int)index);
      Serial.print("]=");
      Serial.println((int)data[index]);
      index++;
    }
    if (millis() - PREVIOUS_PACKAGE_TIME > MAX_PACKAGE_AWAIT_TIME) {
      return -1;
    }
  }
  if (calculateHash(data, configSize) == data[configSize]) {
    BLUETOOTH_SERIAL.write((uint8_t)0);
    return 1;
  } else {
    BLUETOOTH_SERIAL.write((uint8_t)1);
    return 0;
  }
}

int readTarget() {
  uint8_t target;
  while (true) {
    if (BLUETOOTH_SERIAL.available() > 0) {
      target = BLUETOOTH_SERIAL.read();
      Serial.print("target=");
      Serial.println((int)target);
      break;
    }
    if (millis() - PREVIOUS_PACKAGE_TIME > MAX_PACKAGE_AWAIT_TIME) {
      return -1;
    }
  }
  uint8_t target1;
  while (true) {
    if (BLUETOOTH_SERIAL.available() > 0) {
      target1 = BLUETOOTH_SERIAL.read();
      Serial.print("TARGET1=");
      Serial.println((int)target1);
      if (target == target1) {
        BLUETOOTH_SERIAL.write((uint8_t)0);
        return target;
      } else {
        BLUETOOTH_SERIAL.write((uint8_t)1);
        return 0;
      }
    }
    if (millis() - PREVIOUS_PACKAGE_TIME > MAX_PACKAGE_AWAIT_TIME) {
      return -1;
    }
  }
}

uint8_t calculateHash(uint8_t* data, uint8_t size) {
  uint32_t hash = 0;
  for (uint8_t i = 0; i < size; i++) {
    hash += data[i];
  }
  return hash % 256;
}

uint8_t getSettingsSize(int target) {
  switch (target) {
    case 1:
      return sizeof(IS_STRIP_ON);
    case 2:
      return sizeof(MODE);
    case 3:
      return 1;
    case 100:
      return sizeof(Global::Settings);
    case 101:
      return sizeof(VuAnalyzer::Settings);
    case 102:
      return sizeof(LowMediumHighFrequency::Settings);
    case 103:
      return sizeof(FullRangeFrequency::Settings);
    case 104:
      return sizeof(Strob::Settings);
    case 105:
      return sizeof(Backlight::Settings);
    default:
      return 0;
  }
}

void setSettings(int target, uint8_t* data, uint8_t size) {
  switch (target) {
    case 1:
      if (IS_STRIP_ON != data[0]) {
        IS_STRIP_ON = data[0];
        if (IS_STRIP_ON == false) {
          fillLeds(CHSV(0, 0, 0));
          FastLED.show();
        }
      }
      return;
    case 2:
      MODE = data[0];
      return;
    case 3:
      return setThreshold();
    case 100:
      return setGlobalSettings((Global::Settings*)data);
    case 101:
      return setVuSettings((VuAnalyzer::Settings*)data);
    case 102:
      return setLmhFrequencySettings((LowMediumHighFrequency::Settings*)data);
    case 103:
      return setFullRangeFrequencySettings((FullRangeFrequency::Settings*)data);
    case 104:
      return setStrobSettings((Strob::Settings*)data);
    case 105:
      return setBacklightSettings((Backlight::Settings*)data);
    default:
      return;
  }
}

void setGlobalSettings(Global::Settings* settings) {
  Serial.println("Apply global");
  auto oldSettings = &GLOBAL.settings;
  if (oldSettings->isMicro != settings->isMicro) {
    oldSettings->isMicro = settings->isMicro;
    if (oldSettings->isMicro) {
      GLOBAL.soundRightPin = Global::MICRO_RIGHT_PIN;
      GLOBAL.soundFrequencyPin = Global::MICRO_FREQUENCY_PIN;
    } else {
      GLOBAL.soundRightPin = Global::WIRE_RIGHT_PIN;
      GLOBAL.soundFrequencyPin = Global::WIRE_FREQUENCY_PIN;
    }
    setReferenceVoltage();
    setThreshold();
  }
  if (oldSettings->numLeds != settings->numLeds) {
    oldSettings->numLeds = settings->numLeds;
    fillLeds(CHSV(0, 0, 0));
    FastLED.show();
    initLeds();
  }
  oldSettings->isStereo = settings->isStereo;
  oldSettings->enabledBrightness = settings->enabledBrightness;
  oldSettings->disabledBrightness = settings->disabledBrightness;
  oldSettings->disabledHue = settings->disabledHue;
}

void setVuSettings(VuAnalyzer::Settings* settings) {
  Serial.println("Apply Vu");
  VU.settings.rainbowStep = settings->rainbowStep;
  VU.settings.smooth = settings->smooth;
  VU.settings.isRainbowOn = settings->isRainbowOn;
}

void setLmhFrequencySettings(LowMediumHighFrequency::Settings* settings) {
  Serial.println("Apply LMH Frequency");
  auto* p = &LHM_FREQUENCY.settings;
  for (uint8_t i; i < sizeof(LowMediumHighFrequency::Settings); i++) {
    p[i] = settings[i];
  }
}

void setFullRangeFrequencySettings(FullRangeFrequency::Settings* settings) {
  Serial.println("Apply Full Frequency");
  FULL_FREQUENCY.settings.lowFrequencyHue = settings->lowFrequencyHue;
  FULL_FREQUENCY.settings.smooth = settings->smooth;
  FULL_FREQUENCY.settings.hueStep = settings->hueStep;
}

void setStrobSettings(Strob::Settings* settings) {
  Serial.println("Apply Strob");
  auto* p = &STROB.settings;
  for (uint8_t i; i < sizeof(Strob::Settings); i++) {
    p[i] = settings[i];
  }
}
void setBacklightSettings(Backlight::Settings* settings) {
  Serial.println("Apply Backlight");
  auto* p = &BACKLIGHT.settings;
  for (uint8_t i; i < sizeof(Backlight::Settings); i++) {
    p[i] = settings[i];
  }
}
