#include "colorMusic.h"
void setup();
void loop();
void processSound();
void initLeds();
void setThreshold();
void setVuThreshold();
void setFrequencyThreshold();
void setReferenceVoltage();
void processLevel();
void vuAnimation(uint16_t rLength, uint16_t lLength);
void colorEmptyLeds(uint16_t rDisabled, uint16_t lDisabled);
void backlightAnimation();
void processStrobe();
void strobeAnimation();
void analyzeAudio();
void lmhFrequencyTransform();
void lmhFrequencyAnimation();
void fullFrequencyTransform();
void fullFrequencyAnimation();
float calcSoundLevel(float level);
void silence();
void fillLeds(CHSV color);
void checkBluetooth();


uint8_t getSettingsSize(int target);
void setSettings(int target, uint8_t* data, uint8_t size);
uint8_t calculateHash(uint8_t* data, uint8_t size);
int readData(uint8_t* data, uint8_t configSize);
int readTarget();

void setGlobalSettings(Global::Settings* settings);
void setVuSettings(VuAnalyzer::Settings* settings);
void setLmhFrequencySettings(LowMediumHighFrequency::Settings* settings);
void setFullRangeFrequencySettings(FullRangeFrequency::Settings* settings);
void setStrobSettings(Strob::Settings* settings);
void setBacklightSettings(Backlight::Settings* settings);
