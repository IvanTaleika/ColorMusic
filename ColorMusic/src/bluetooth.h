#include <SoftwareSerial.h>
#define START_BYTE 'a'
#define END_BYTE 'z'

struct Bluetooth {
  enum Operation{
    GET_STATE,
    RESET_STATE
  };
  //NUmeric code of the last error
  uint8_t state = 0;
  bool isReceiving;
  bool isMessageReceived;
  String message;
};