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
void readBluetooth();
void processMessage();

uint8_t globalUpdate(String& message);
uint8_t vuUpdate(String& message);
uint8_t frequencyUpdate(String& message);
uint8_t strobeUpdate(String& message);
uint8_t backlightUpdate(String& message);
void bluetoothOperation(String& message);
CHSV parseHsvColor(String message);