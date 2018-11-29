#include "bluetooth.h"
#include "colorMusic.h"
void setup();
void loop();
void initLeds();
void setThreshold();
void setVuThreshold();
void setFrequencyThreshold();
void setReferenceVoltage();
void processSound();
void processLevel();
void vuAnimation(int16_t rLength, int16_t lLength);
void colorEmptyLeds(int16_t rDisabled, int16_t lDisabled);
void backlightAnimation();
void processStrobe();
void strobeAnimation();
void processFrequency();
void analyzeAudio();
void fullFrequencyTransform();
void fullFrequencyAnimation();
void lmhTransform();
void lmhFrequencyAnimation();
float calcSoundLevel(float level);
void silence();
void fillLeds(CHSV color);
void checkBluetooth();
void processMessage();

uint8_t globalUpdate(char* charArray);
uint8_t vuUpdate(char* charArray);
uint8_t frequencyUpdate(char* charArray);
uint8_t strobeUpdate(char* charArray);
uint8_t backlightUpdate(char* charArray);
void bluetoothOperation(char* charArray);
CHSV parseHsvColor(char* charArray, uint8_t from, uint8_t to);
double substrToDouble(char* charArray, uint8_t from, uint8_t to);
int substrToInt(char* charArray, uint8_t from, uint8_t to);