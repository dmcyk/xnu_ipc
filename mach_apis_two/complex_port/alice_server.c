#include "message.h"

// Darwin
#include <bootstrap.h>
#include <mach/mach.h>
#include <mach/message.h>

// std
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

mach_msg_return_t send_reply(const PortMessage *inMessage) {
  Message response = {0};

  response.header.msgh_bits = MACH_MSGH_BITS_SET(
      /* remote */ inMessage->descriptor.disposition,
      /* local */ 0,
      /* voucher */ 0,
      /* other */ 0);
  response.header.msgh_remote_port = inMessage->descriptor.name;
  response.header.msgh_id = MSG_ID_DEFAULT;
  response.header.msgh_size = sizeof(response);

  strcpy(response.bodyStr, "Response :) ");

  return mach_msg(
      /* msg */ (mach_msg_header_t *)&response,
      /* option */ MACH_SEND_MSG,
      /* send size */ sizeof(response),
      /* recv size */ 0,
      /* recv_name */ MACH_PORT_NULL,
      /* timeout */ MACH_MSG_TIMEOUT_NONE,
      /* notify port */ MACH_PORT_NULL);
}

mach_msg_return_t receive_msg(
    mach_port_name_t recvPort,
    ReceiveAnyMessage *buffer) {
  mach_msg_return_t ret = mach_msg(
      /* msg */ (mach_msg_header_t *)buffer,
      /* option */ MACH_RCV_MSG,
      /* send size */ 0,
      /* recv size */ sizeof(*buffer),
      /* recv_name */ recvPort,
      /* timeout */ MACH_MSG_TIMEOUT_NONE,
      /* notify port */ MACH_PORT_NULL);
  if (ret != MACH_MSG_SUCCESS) {
    return ret;
  }

  if (buffer->header.msgh_id == MSG_ID_DEFAULT) {
    Message *message = &buffer->message.message;

    printf("got default message!\n");
    printf("  id      : %d\n", message->header.msgh_id);
    printf("  bodyS   : %s\n", message->bodyStr);
    printf("  bodyI   : %d\n", message->bodyInt);
  } else if (buffer->header.msgh_id == MSG_ID_PORT) {
    printf(
        "got port message with name: %#x, disposition: %#x!\n",
        buffer->portMessage.message.descriptor.name,
        buffer->portMessage.message.descriptor.disposition);
  } else {
    return RCV_ERROR_INVALID_MESSAGE_ID;
  }

  return MACH_MSG_SUCCESS;
}

int main() {
  mach_port_t task = mach_task_self();

  mach_port_name_t recvPort;
  if (mach_port_allocate(task, MACH_PORT_RIGHT_RECEIVE, &recvPort) !=
      KERN_SUCCESS) {
    return EXIT_FAILURE;
  }

  if (mach_port_insert_right(
          task, recvPort, recvPort, MACH_MSG_TYPE_MAKE_SEND) != KERN_SUCCESS) {
    return EXIT_FAILURE;
  }

  mach_port_t bootstrapPort;
  if (task_get_special_port(task, TASK_BOOTSTRAP_PORT, &bootstrapPort) !=
      KERN_SUCCESS) {
    return EXIT_FAILURE;
  }

  if (bootstrap_register(
          bootstrapPort, "xyz.dmcyk.alice.as-a-service", recvPort) !=
      KERN_SUCCESS) {
    return EXIT_FAILURE;
  }

  while (true) {
    // Message buffer.
    ReceiveAnyMessage receiveMessage = {0};

    mach_msg_return_t ret = receive_msg(recvPort, &receiveMessage);
    if (ret != MACH_MSG_SUCCESS) {
      printf("Failed to receive a message: %#x\n", ret);
      continue;
    }

    // Continue if it's not the port descriptor message.
    if (receiveMessage.header.msgh_id != MSG_ID_PORT) {
      continue;
    }

    ret = send_reply(&receiveMessage.portMessage.message);

    if (ret != MACH_MSG_SUCCESS) {
      printf("Failed to respond: %#x\n", ret);
    }
  }

  return 0;
}
