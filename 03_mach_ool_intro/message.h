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

typedef struct {
  mach_msg_header_t header;
  mach_msg_size_t msgh_descriptor_count;
  mach_msg_ool_descriptor_t descriptor;
} OOLMessage;

typedef struct {
  OOLMessage message;

  // Suitable for use with the default trailer type - no custom trailer
  // information requested using `MACH_RCV_TRAILER_TYPE`, or just the explicit
  // `MACH_RCV_TRAILER_NULL` type.
  mach_msg_trailer_t trailer;
} OOLReceiveMessage;

#define MSG_ID_DEFAULT 8
#define MSG_ID_COPY_MEM 10

#define RCV_ERROR_INVALID_MESSAGE_ID 0xffffff01
