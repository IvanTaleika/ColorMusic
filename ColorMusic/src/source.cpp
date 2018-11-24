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
unsigned long LOOP_TIMER = 0;
CRGB* STRIP_LEDS = nullptr;
Global GLOBAL;
VuAnalyzer VU;
FrequencyAnalyzer FREQUENCY;
Strobe STROBE;
Backlight BACKLIGHT;
Bluetooth BLUETOOTH;

void setup() {
  Serial.begin(9600);
  BLUETOOTH_SERIAL.begin(9600);
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
  FREQUENCY.fullTransform.ledsForFreq = (float)GLOBAL.halfLedsNum / 30;
  Serial.print("GLOBAL.halfLedsNum=");
  Serial.println(GLOBAL.halfLedsNum);
  Serial.print("VU.colorToLed=");
  Serial.println(VU.colorToLed);
  Serial.print("FREQUENCY.fullTransform.ledsForFreq=");
  Serial.println(FREQUENCY.fullTransform.ledsForFreq);
}

void setThreshold() {
  delay(1000);  // ждём инициализации АЦП
  setVuThreshold();
  setFrequencyThreshold();
}

void setVuThreshold() {
  Serial.println("VU signalThreshold");

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
  VU.signalThreshold = maxLevel + VU.additionalThreshold;
}

void setFrequencyThreshold() {
  Serial.println("setFrequencyThreshold");

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
    if (GLOBAL.isStereo && !GLOBAL.isMicro) {
      lLevel = analogRead(SOUND_L);
      if (lMax < lLevel) {
        lMax = lLevel;
      }
    }
  }
  // фильтр скользящее среднее
  VU.rAverage = (float)rMax * VU.smooth + VU.rAverage * (1 - VU.smooth);

  if (GLOBAL.isStereo && !GLOBAL.isMicro) {
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

void checkBluetooth() {
  if (BLUETOOTH_SERIAL.available()) {
    readBluetooth();
  }
  if (BLUETOOTH.isMessageReceived) {
    processMessage();
  }
}

void readBluetooth() {
  char incomingByte = BLUETOOTH_SERIAL.read();
  Serial.print("readBluetooth=");
  Serial.println(incomingByte);
  if (BLUETOOTH.isReceiving) {
    if (incomingByte == END_BYTE) {
      BLUETOOTH.isReceiving = false;
      BLUETOOTH.isMessageReceived = true;
    } else {
      BLUETOOTH.message += incomingByte;
    }
  }
  if (incomingByte == START_BYTE) {
    BLUETOOTH.isReceiving = true;
    BLUETOOTH.message = "";
  }
}

void processMessage() {
  Serial.print("message=");
  Serial.println(BLUETOOTH.message);
  Serial.print("state=");
  Serial.println(BLUETOOTH.state);
  uint8_t destination = BLUETOOTH.message.substring(0, 1).toInt();

  uint8_t result = 0;
  switch (destination) {
    case 0:
      result = globalUpdate(BLUETOOTH.message);
      break;
    case 1:
      result = vuUpdate(BLUETOOTH.message);
      break;
    case 2:
      result = frequencyUpdate(BLUETOOTH.message);
      break;
    case 3:
      result = strobeUpdate(BLUETOOTH.message);
      break;
    case 4:
      result = backlightUpdate(BLUETOOTH.message);
      break;
    case 9:
      bluetoothOperation(BLUETOOTH.message);
      break;
    default:
      result = 1;
      break;
  }
  if (result) {
    BLUETOOTH.state = result;
  }
  BLUETOOTH.isMessageReceived = false;
}

uint8_t globalUpdate(String& message) {
  auto setting = static_cast<Global::Setting>(message.substring(1, 3).toInt());
  switch (setting) {
    case Global::ON_OFF:
      GLOBAL.isOn = message.substring(3).toInt();
      Serial.print("GLOBAL.isOn=");
      Serial.println(GLOBAL.isOn);
      return 0;
    case Global::MICRO:
      GLOBAL.isMicro = message.substring(3).toInt();
      setThreshold();
      Serial.print("GLOBAL.isMicro=");
      Serial.println(GLOBAL.isMicro);
      return 0;
    case Global::STEREO:
      GLOBAL.isStereo = message.substring(3).toInt();
      Serial.print("GLOBAL.isStereo=");
      Serial.println(GLOBAL.isStereo);
      return 0;
    case Global::MODE:
      GLOBAL.currentMode = message.substring(3).toInt();
      Serial.print("GLOBAL.currentMode=");
      Serial.println(GLOBAL.currentMode);
      return 0;
    case Global::N_LEDS:
      GLOBAL.numLeds = message.substring(3).toInt();
      initLeds();
      Serial.print("GLOBAL.numLeds=");
      Serial.println(GLOBAL.numLeds);
      return 0;
    case Global::ENABLED_BRIGHTNESS:
      GLOBAL.enabledBrightness = message.substring(3).toInt();
      Serial.print("GLOBAL.enabledBrightness=");
      Serial.println(GLOBAL.enabledBrightness);
      return 0;
    case Global::DISABLED_BRIGHTNESS:
      GLOBAL.disabledBrightness = message.substring(3).toInt();
      Serial.print("GLOBAL.disabledBrightness=");
      Serial.println(GLOBAL.disabledBrightness);
      return 0;
    default:
      return 2;
  }
}

uint8_t vuUpdate(String& message) { return 3; }
uint8_t frequencyUpdate(String& message) { return 4; }
uint8_t strobeUpdate(String& message) { return 5; }
uint8_t backlightUpdate(String& message) { return 6; }
void bluetoothOperation(String& message) {
  auto operation =
      static_cast<Bluetooth::Operation>(message.substring(1, 3).toInt());
  switch (operation) {
    case Bluetooth::GET_STATE:
      BLUETOOTH_SERIAL.write(BLUETOOTH.state);
      Serial.print("BLUETOOTH.state=");
      Serial.println(BLUETOOTH.state);
      return;
    case Bluetooth::RESET_STATE:
      Serial.print("RESET BLUETOOTH.state=");
      Serial.println(BLUETOOTH.state);
      BLUETOOTH.state = 0;
      return;
      
  }
}