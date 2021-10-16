#pragma once

#include <mach/message.h>

typedef struct {
  mach_msg_header_t header;
  char bodyStr[32];
  int bodyInt;
} Message;

typedef struct {
  Message message;

  // Suitable for use with the default trailer type - no custom trailer
  // information requested using `MACH_RCV_TRAILER_TYPE`, or just the explicit
  // `MACH_RCV_TRAILER_NULL` type.
  mach_msg_trailer_t trailer;
} ReceiveMessage;
