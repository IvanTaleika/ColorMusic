#include <stdint.h>
#define START_BYTE 'a'
#define END_BYTE 'z'
#define MAX_MESSAGE_SIZE 20

struct Bluetooth {
  enum Operation { GET_STATE, RESET_STATE };
  // Numeric code of the last error
  uint8_t state = 0;
  char message[MAX_MESSAGE_SIZE];
  uint8_t messageSize = 0;
};