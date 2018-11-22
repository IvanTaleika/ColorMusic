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

// --------------------------- НАСТРОЙКИ ---------------------------
#define LOG_OUT 1   //FOR FHT.h lib
#include <FHT.h>         // преобразование Хартли
#include <EEPROMex.h>
#include <SoftwareSerial.h>
#define FASTLED_ALLOW_INTERRUPTS 1
#include "FastLED.h"

// пины
#define SOUND_R A2         // аналоговый пин вход аудио, правый канал
#define SOUND_L A1         // аналоговый пин вход аудио, левый канал
#define SOUND_R_FREQ A3    // аналоговый пин вход аудио для режима с частотами (через кондер)
#define MLED_PIN 13           // пин светодиода режимов
#define MLED_ON HIGH
#define LED_PIN 4         // пин DI светодиодной ленты
#define POT_GND A0         // пин земля для потенциометра

SoftwareSerial bluetoothSerial(4, 3); // RX | TX
#define START_BYTE '$'
#define END_BYTE '^'
// лента
#define NUM_LEDS 102        // количество светодиодов  //В приложении

struct globalSettings {
  bool isOn = true;
  bool isMicro = false;
  bool isMono = true;         // 1 - только один канал (ПРАВЫЙ!!!!! SOUND_R!!!!!), 0 - два канала
  byte enabledBrightness = 200;
  byte disabledBrightness = 30;
  byte currentMode = 0;
  float expCoeffincient = 1; // степень усиления сигнала (для более "резкой" работы)
  byte disabledColor = HUE_PURPLE;
  //NOT setted
  uint16_t numLeds = 102;
} GLOBAL;


struct backlightSettings {
  byte mode = 0;
  byte defaultColor = 150;
  //mode 1
  byte defaultSaturation = 200;

  //mode 2
  byte colorChangeDelay = 100;
  unsigned long colorChangeTime;
  byte currentColor;

  //mode 3
  byte rainbowColorChangeStep = 3;
  byte rainbowStep = 5;
} BACKLIGHT;


// режим стробоскопа
struct strobeSettings {
  unsigned long previousFlashTime;
  byte color = HUE_YELLOW;
  byte saturation = 0;
  bool isNewCycle = false;
  byte bright = 0;
  byte brightStep = 100;
  byte duty = 20;
  uint16_t flashDelay = 100;     // период вспышек, миллисекунды
} STROBE;

struct vuAnalyzerSettings {
  uint16_t signalThreshold = 15;
  byte rainbowStep = 5;
  float smooth = 0.3;
  bool isRainbowOn = false;
  int rainbowColor; //TODO byte?
} VU;


// отрисовка
#define MODE 1              // режим при запуске
#define MAIN_LOOP 5         // период основного цикла отрисовки (по умолчанию 5)
unsigned long main_timer;
// сигнал
#define POTENT 0            // 1 - используем потенциометр, 0 - используется внутренний источник опорного напряжения 1.1 В

// нижний порог шумов
uint16_t SPEKTR_LOW_PASS = 40;   // нижний порог шумов режим спектра, ручная настройка
#define AUTO_LOW_PASS 0     // разрешить настройку нижнего порога шумов при запуске (по умолч. 0)
#define EEPROM_LOW_PASS 0   // порог шумов хранится в энергонезависимой памяти (по умолч. 1)
#define LOW_PASS_ADD 13     // "добавочная" величина к нижнему порогу, для надёжности (режим VU)
#define LOW_PASS_FREQ_ADD 3 // "добавочная" величина к нижнему порогу, для надёжности (режим частот)

// шкала громкости
#define MAX_COEF 1.8        // коэффициент громкости (максимальное равно срднему * этот коэф) (по умолчанию 1.8)

// режим цветомузыки
float SMOOTH_FREQ = 0.8;          // коэффициент плавности анимации частот (по умолчанию 0.8)
float MAX_COEF_FREQ = 1.2;        // коэффициент порога для "вспышки" цветомузыки (по умолчанию 1.5)
#define SMOOTH_STEP 20            // шаг уменьшения яркости в режиме цветомузыки (чем больше, тем быстрее гаснет)
#define LOW_COLOR HUE_RED         // цвет низких частот
#define MID_COLOR HUE_GREEN       // цвет средних
#define HIGH_COLOR HUE_YELLOW     // цвет высоких


// режим бегущих частот
byte RUNNING_SPEED = 10;

// режим анализатора спектра
byte HUE_START = 0;
byte HUE_STEP = 5;
#define LIGHT_SMOOTH 2

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

// ------------------------------ ДЛЯ РАЗРАБОТЧИКОВ --------------------------------

#define STRIPE NUM_LEDS / 5
float freq_to_stripe = NUM_LEDS / 40; // /2 так как симметрия, и /20 так как 20 частот

#define FHT_N 64         // ширина спектра х2
#define LOG_OUT 1



CRGB leds[NUM_LEDS];

// градиент-палитра от зелёного к красному
DEFINE_GRADIENT_PALETTE(soundlevel_gp) {
  0,    0,    255,  0,  // green
  100,  255,  255,  0,  // yellow
  150,  255,  100,  0,  // orange
  200,  255,  50,   0,  // red
  255,  255,  0,    0   // red
};
CRGBPalette32 myPal = soundlevel_gp;

byte Rlenght, Llenght;
float RsoundLevel, RsoundLevel_f;
float LsoundLevel, LsoundLevel_f;

float averageLevel = 50;
int maxLevel = 100;
byte MAX_CH = NUM_LEDS / 2;

unsigned long   running_timer,  rainbow_timer;
float averK = 0.006;

float index = (float)255 / MAX_CH;   // коэффициент перевода для палитры
int RcurrentLevel, LcurrentLevel;
int colorMusic[3];
float colorMusic_f[3], colorMusic_aver[3];
boolean colorMusicFlash[3], strobeUp_flag, strobeDwn_flag;
int thisBright[3];

int8_t freq_strobe_mode;
int freq_max;
float freq_max_f;
int freq_f[32];
boolean running_flag[3], eeprom_flag;

#define cbi(sfr, bit) (_SFR_BYTE(sfr) &= ~_BV(bit))
#define sbi(sfr, bit) (_SFR_BYTE(sfr) |= _BV(bit))
// ------------------------------ ДЛЯ РАЗРАБОТЧИКОВ --------------------------------

void setup() {
  Serial.begin(9600);
  FastLED.addLeds<WS2811, LED_PIN, GRB>(leds, NUM_LEDS).setCorrection( TypicalLEDStrip );
  FastLED.setBrightness(GLOBAL.enabledBrightness);

  pinMode(MLED_PIN, OUTPUT);        //Режим пина для светодиода режима на выход
  digitalWrite(MLED_PIN, !MLED_ON); //Выключение светодиода режима

  pinMode(POT_GND, OUTPUT);
  digitalWrite(POT_GND, LOW);


  //TODO for wire

  if (POTENT) {
    analogReference(EXTERNAL);
  }
  else {
    analogReference(INTERNAL);
  }

  sbi(ADCSRA, ADPS2);
  cbi(ADCSRA, ADPS1);
  sbi(ADCSRA, ADPS0);


  if (AUTO_LOW_PASS && !EEPROM_LOW_PASS) {         // если разрешена автонастройка нижнего порога шумов
    autoLowPass();
  }
}
void loop() {
  checkBluetooth();
  if (GLOBAL.isOn && millis() - main_timer > MAIN_LOOP) {
    processSound();
    if (GLOBAL.currentMode != 7) {       // 7 режиму не нужна очистка!!!
      FastLED.clear();          // очистить массив пикселей
    }
    main_timer = millis();    // сбросить таймер
  }
}

void processSound() {
  // сбрасываем значения
  RsoundLevel = 0;
  LsoundLevel = 0;
  switch (GLOBAL.currentMode) {
    case 0: processLevel();
    case 1: processFrequency();
    case 2: processStrobe();
    case 3: backlightAnimation();
  }
}

void processLevel() {
  for (byte i = 0; i < 100; i ++) {                                 // делаем 100 измерений
    RcurrentLevel = analogRead(SOUND_R);                            // с правого
    if (RsoundLevel < RcurrentLevel) {
      RsoundLevel = RcurrentLevel;   // ищем максимальное
    }
    if (!GLOBAL.isMono || GLOBAL.isMicro) {
      LcurrentLevel = analogRead(SOUND_L); // c левого канала
      if (LsoundLevel < LcurrentLevel) {
        LsoundLevel = LcurrentLevel;   // ищем максимальное
      }
    }
  }
  RsoundLevel = calcSoundLevel(RsoundLevel);
  RsoundLevel_f = RsoundLevel * VU.smooth + RsoundLevel_f * (1 - VU.smooth); // фильтр скользящее среднее
  // For stereo
  if (!GLOBAL.isMono || GLOBAL.isMicro) {
    LsoundLevel = calcSoundLevel(LsoundLevel);
    LsoundLevel_f = LsoundLevel * VU.smooth + LsoundLevel_f * (1 - VU.smooth);
  } else {
    LsoundLevel_f = RsoundLevel_f; // если моно, то левый = правому
  }
  // если значение выше порога - начинаем самое интересное
  if (RsoundLevel_f > 15 && LsoundLevel_f > 15) {
    // расчёт общей средней громкости с обоих каналов, фильтрация.
    // Фильтр очень медленный, сделано специально для автогромкости
    averageLevel = (float)(RsoundLevel_f + LsoundLevel_f) / 2 * averK + averageLevel * (1 - averK);
    // принимаем максимальную громкость шкалы как среднюю, умноженную на некоторый коэффициент MAX_COEF
    maxLevel = (float)averageLevel * MAX_COEF;
    // преобразуем сигнал в длину ленты (где MAX_CH это половина количества светодиодов)
    Rlenght = map(RsoundLevel_f, 0, maxLevel, 0, MAX_CH);
    Llenght = map(LsoundLevel_f, 0, maxLevel, 0, MAX_CH);
    // ограничиваем до макс. числа светодиодов
    Rlenght = constrain(Rlenght, 0, MAX_CH);
    Llenght = constrain(Llenght, 0, MAX_CH);
    vuAnimation();       // отрисовать
  } else if (GLOBAL.disabledBrightness > 5) {
    silenceAnimation();
  }
}

void processFrequency() {
  analyzeAudio();
  colorMusic[0] = 0;
  colorMusic[1] = 0;
  colorMusic[2] = 0;
  for (int i = 0 ; i < 32 ; i++) {
    if (fht_log_out[i] < SPEKTR_LOW_PASS) {
      fht_log_out[i] = 0;
    }
  }
  // низкие частоты, выборка со 2 по 5 тон (0 и 1 зашумленные!)
  for (byte i = 2; i < 6; i++) {
    if (fht_log_out[i] > colorMusic[0]) {
      colorMusic[0] = fht_log_out[i];
    }
  }
  // средние частоты, выборка с 6 по 10 тон
  for (byte i = 6; i < 11; i++) {
    if (fht_log_out[i] > colorMusic[1]) {
      colorMusic[1] = fht_log_out[i];
    }
  }
  // высокие частоты, выборка с 11 по 31 тон
  for (byte i = 11; i < 32; i++) {
    if (fht_log_out[i] > colorMusic[2]) {
      colorMusic[2] = fht_log_out[i];
    }
  }
  freq_max = 0;
  for (byte i = 0; i < 30; i++) {
    if (fht_log_out[i + 2] > freq_max) {
      freq_max = fht_log_out[i + 2];
    }
    if (freq_max < 5) {
      freq_max = 5;
    }

    if (freq_f[i] < fht_log_out[i + 2]) {
      freq_f[i] = fht_log_out[i + 2];
    }
    if (freq_f[i] > 0) {
      freq_f[i] -= LIGHT_SMOOTH;
    }
    else {
      freq_f[i] = 0;
    }
  }
  freq_max_f = freq_max * averK + freq_max_f * (1 - averK);
  //TODO rewrite
  for (byte i = 0; i < 3; i++) {
    //Звук с одинаковыми частотами в итоге вырубит отображение вообще
    colorMusic_aver[i] = colorMusic[i] * averK + colorMusic_aver[i] * (1 - averK);  // общая фильтрация
    colorMusic_f[i] = colorMusic[i] * SMOOTH_FREQ + colorMusic_f[i] * (1 - SMOOTH_FREQ);      // локальная
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
  frequencyAnimation();
}



void vuAnimation() {
  byte count;
  count = 0;
  if (VU.isRainbowOn) {
    if (millis() - rainbow_timer > 30) {
      rainbow_timer = millis();
      VU.rainbowColor = VU.rainbowColor + VU.rainbowStep;
    }
    count = 0;
    // RainbowColors_p -  default FastLED pallet
    for (int i = (MAX_CH - 1); i > ((MAX_CH - 1) - Rlenght); i--) {
      leds[i] = ColorFromPalette(RainbowColors_p, (count * index) / 2 - VU.rainbowColor);  // заливка по палитре радуга
      count++;
    }
    count = 0;
    for (int i = (MAX_CH); i < (MAX_CH + Llenght); i++ ) {
      leds[i] = ColorFromPalette(RainbowColors_p, (count * index) / 2 - VU.rainbowColor); // заливка по палитре радуга
      count++;
    }
  } else {
    for (int i = (MAX_CH - 1); i > ((MAX_CH - 1) - Rlenght); i--) {
      leds[i] = ColorFromPalette(myPal, (count * index));   // заливка по палитре " от зелёного к красному"
      count++;
    }
    count = 0;
    for (int i = (MAX_CH); i < (MAX_CH + Llenght); i++ ) {
      leds[i] = ColorFromPalette(myPal, (count * index));   // заливка по палитре " от зелёного к красному"
      count++;
    }
  }
}

void silenceAnimation() {
  // заливаем "подложку", если яркость достаточная
  for (int i = 0; i < NUM_LEDS; i++) {
    leds[i] = CHSV(GLOBAL.disabledColor, 255, GLOBAL.disabledBrightness);
  }
}


void backlightAnimation() {
  switch (BACKLIGHT.mode) {
    case 0:
      for (int i = 0; i < NUM_LEDS; i++) {
        leds[i] = CHSV(BACKLIGHT.defaultColor, BACKLIGHT.defaultSaturation, 255);
      }
      break;
    case 1:
      if (millis() - BACKLIGHT.colorChangeTime > BACKLIGHT.colorChangeDelay) {
        BACKLIGHT.colorChangeTime = millis();
        BACKLIGHT.currentColor++;
      }
      for (int i = 0; i < NUM_LEDS; i++) {
        leds[i] = CHSV(BACKLIGHT.currentColor, BACKLIGHT.defaultSaturation, 255);
      }
      break;
    case 2:
      if (millis() - BACKLIGHT.colorChangeTime > BACKLIGHT.colorChangeDelay) {
        BACKLIGHT.colorChangeTime = millis();
        BACKLIGHT.currentColor += BACKLIGHT.rainbowColorChangeStep;
      }
      byte rainbowStep = BACKLIGHT.currentColor;
      for (int i = 0; i < NUM_LEDS; i++) {
        leds[i] = CHSV((int)floor(rainbowStep), 255, 255);
        rainbowStep += BACKLIGHT.rainbowStep;
      }
      break;
  }
}

void processStrobe() {
  if ((long)millis() - STROBE.previousFlashTime > STROBE.flashDelay) {
    STROBE.previousFlashTime = millis();
    STROBE.isNewCycle = true;
  }
  if (STROBE.isNewCycle && (long)millis() - STROBE.previousFlashTime > STROBE.flashDelay * STROBE.duty / 100) {
    if (STROBE.bright > STROBE.brightStep) {
      STROBE.bright -= STROBE.brightStep;
    } else {
      STROBE.bright = 0;
      STROBE.isNewCycle = false;
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
    for (int i = 0; i < NUM_LEDS; i++) {
      leds[i] = CHSV(STROBE.color, STROBE.saturation, STROBE.bright);
    }
  }
  else {
    for (int i = 0; i < NUM_LEDS; i++) {
      leds[i] = CHSV(GLOBAL.disabledColor, 255, GLOBAL.disabledBrightness);
    }
  }
}

void frequencyAnimation() {
  switch (GLOBAL.currentMode) {
    case 2:
      for (int i = 0; i < NUM_LEDS; i++) {
        if (i < STRIPE)          {
          leds[i] = CHSV(HIGH_COLOR, 255, thisBright[2]);
        }
        else if (i < STRIPE * 2) {
          leds[i] = CHSV(MID_COLOR, 255, thisBright[1]);
        }
        else if (i < STRIPE * 3) {
          leds[i] = CHSV(LOW_COLOR, 255, thisBright[0]);
        }
        else if (i < STRIPE * 4) {
          leds[i] = CHSV(MID_COLOR, 255, thisBright[1]);
        }
        else if (i < STRIPE * 5) {
          leds[i] = CHSV(HIGH_COLOR, 255, thisBright[2]);
        }
      }
      break;
    case 3:
      for (int i = 0; i < NUM_LEDS; i++) {
        if (i < NUM_LEDS / 3) {
          leds[i] = CHSV(HIGH_COLOR, 255, thisBright[2]);
        }
        else  if (i < NUM_LEDS * 2 / 3) {
          leds[i] = CHSV(MID_COLOR, 255, thisBright[1]);
        }
        else if (i < NUM_LEDS)  {
          leds[i] = CHSV(LOW_COLOR, 255, thisBright[0]);
        }
      }
      break;
    case 4:
      switch (freq_strobe_mode) {
        case 0:
          if (colorMusicFlash[2]) HIGHS();
          else if (colorMusicFlash[1]) MIDS();
          else if (colorMusicFlash[0]) LOWS();
          else SILENCE();
          break;
        case 1:
          if (colorMusicFlash[2]) HIGHS();
          else SILENCE();
          break;
        case 2:
          if (colorMusicFlash[1]) MIDS();
          else SILENCE();
          break;
        case 3:
          if (colorMusicFlash[0]) LOWS();
          else SILENCE();
          break;
      }
      break;
    case 7:
      //Показывает обычно просто средние - т.к. цепляет их чаще, низки почти не попадают
      switch (freq_strobe_mode) {
        case 0:
          if (running_flag[2]) leds[NUM_LEDS / 2] = CHSV(HIGH_COLOR, 255, thisBright[2]);
          else if (running_flag[1]) leds[NUM_LEDS / 2] = CHSV(MID_COLOR, 255, thisBright[1]);
          else if (running_flag[0]) leds[NUM_LEDS / 2] = CHSV(LOW_COLOR, 255, thisBright[0]);
          else leds[NUM_LEDS / 2] = CHSV(GLOBAL.disabledColor, 255, GLOBAL.disabledBrightness);
          break;
        case 1:
          if (running_flag[2]) leds[NUM_LEDS / 2] = CHSV(HIGH_COLOR, 255, thisBright[2]);
          else leds[NUM_LEDS / 2] = CHSV(GLOBAL.disabledColor, 255, GLOBAL.disabledBrightness);
          break;
        case 2:
          if (running_flag[1]) leds[NUM_LEDS / 2] = CHSV(MID_COLOR, 255, thisBright[1]);
          else leds[NUM_LEDS / 2] = CHSV(GLOBAL.disabledColor, 255, GLOBAL.disabledBrightness);
          break;
        case 3:
          if (running_flag[0]) leds[NUM_LEDS / 2] = CHSV(LOW_COLOR, 255, thisBright[0]);
          else leds[NUM_LEDS / 2] = CHSV(GLOBAL.disabledColor, 255, GLOBAL.disabledBrightness);
          break;
      }
      leds[(NUM_LEDS / 2) - 1] = leds[NUM_LEDS / 2];
      if (millis() - running_timer > RUNNING_SPEED) {
        running_timer = millis();
        for (byte i = 0; i < NUM_LEDS / 2 - 1; i++) {
          leds[i] = leds[i + 1];
          leds[NUM_LEDS - i - 1] = leds[i];
        }
      }
      break;
    case 8:
      byte HUEindex = HUE_START;
      for (byte i = 0; i < NUM_LEDS / 2; i++) {
        byte this_bright = map(freq_f[(int)floor((NUM_LEDS / 2 - i) / freq_to_stripe)], 0, freq_max_f, 0, 255);
        this_bright = constrain(this_bright, 0, 255);
        leds[i] = CHSV(HUEindex, 255, this_bright);
        leds[NUM_LEDS - i - 1] = leds[i];
        HUEindex += HUE_STEP;
        if (HUEindex > 255) HUEindex = 0;
      }
      break;
  }
}

void colorEmptyLeds() {
  CHSV this_dark = CHSV(GLOBAL.disabledColor, 255, GLOBAL.disabledBrightness);
  for (int i = ((MAX_CH - 1) - Rlenght); i > 0; i--) {
    leds[i] = this_dark;
  }
  for (int i = MAX_CH + Llenght; i < NUM_LEDS; i++) {
    leds[i] = this_dark;
  }
}

float calcSoundLevel(float level) {
  level = map(level, VU.signalThreshold, 1023, 0, 500); // фильтруем по нижнему порогу шумов
  level = constrain(level, 0, 500); // ограничиваем диапазон
  level = pow(level, GLOBAL.expCoeffincient); // возводим в степень (для большей чёткости работы)
  return level;
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

void processMessage() {
  isMessageReceived = false;
}

void HIGHS() {
  for (int i = 0; i < NUM_LEDS; i++) leds[i] = CHSV(HIGH_COLOR, 255, thisBright[2]);
}
void MIDS() {
  for (int i = 0; i < NUM_LEDS; i++) leds[i] = CHSV(MID_COLOR, 255, thisBright[1]);
}
void LOWS() {
  for (int i = 0; i < NUM_LEDS; i++) leds[i] = CHSV(LOW_COLOR, 255, thisBright[0]);
}
void SILENCE() {
  for (int i = 0; i < NUM_LEDS; i++) leds[i] = CHSV(GLOBAL.disabledColor, 255, GLOBAL.disabledBrightness);
}

// вспомогательная функция, изменяет величину value на шаг incr в пределах minimum.. maximum
int smartIncr(int value, int incr_step, int mininmum, int maximum) {
  int val_buf = value + incr_step;
  val_buf = constrain(val_buf, mininmum, maximum);
  return val_buf;
}

float smartIncrFloat(float value, float incr_step, float mininmum, float maximum) {
  float val_buf = value + incr_step;
  val_buf = constrain(val_buf, mininmum, maximum);
  return val_buf;
}


void autoLowPass() {
  // для режима VU
  delay(1000);                                // ждём инициализации АЦП
  int thisMax = 0;                          // максимум
  int thisLevel;
  for (byte i = 0; i < 200; i++) {
    thisLevel = analogRead(SOUND_R);        // делаем 200 измерений

    if (thisLevel > thisMax) {               // ищем максимумы
      thisMax = thisLevel;                  // запоминаем
    }
    delay(4);                               // ждём 4мс
  }
  VU.signalThreshold = thisMax + LOW_PASS_ADD;        // нижний порог как максимум тишины + некая величина

  // для режима спектра
  thisMax = 0;
  for (byte i = 0; i < 100; i++) {          // делаем 100 измерений
    analyzeAudio();                         // разбить в спектр
    for (byte j = 2; j < 32; j++) {         // первые 2 канала - хлам
      thisLevel = fht_log_out[j];
      if (thisLevel > thisMax) {              // ищем максимумы
        thisMax = thisLevel;                // запоминаем
      }
    }
    delay(4);                               // ждём 4мс
  }
  SPEKTR_LOW_PASS = thisMax + LOW_PASS_FREQ_ADD;  // нижний порог как максимум тишины
  if (EEPROM_LOW_PASS && !AUTO_LOW_PASS) {
    EEPROM.updateInt(70, VU.signalThreshold);
    EEPROM.updateInt(72, SPEKTR_LOW_PASS);
  }
}

void analyzeAudio() {
  for (int i = 0 ; i < FHT_N ; i++) {
    int sample = analogRead(SOUND_R_FREQ);
    fht_input[i] = sample; // put real data into bins
  }
  fht_window();  // window the data for better frequency response
  fht_reorder(); // reorder the data before doing the fht
  fht_run();     // process the data in the fht
  fht_mag_log(); // take the output of the fht
}

void fullLowPass() {
  digitalWrite(MLED_PIN, MLED_ON);   // включить светодиод
  FastLED.setBrightness(0); // погасить ленту
  FastLED.clear();          // очистить массив пикселей
  FastLED.show();           // отправить значения на ленту
  delay(500);               // подождать чутка
  autoLowPass();            // измерить шумы
  delay(500);               // подождать
  FastLED.setBrightness(GLOBAL.enabledBrightness);  // вернуть яркость
  digitalWrite(MLED_PIN, !MLED_ON);    // выключить светодиод
}
