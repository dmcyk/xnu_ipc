#include "message.h"

// Darwin
#include <bootstrap.h>
#include <dispatch/dispatch.h>
#include <dispatch/source.h>
#include <mach/mach.h>
#include <mach/message.h>

// std
#include <stdio.h>
#include <stdlib.h>

mach_msg_return_t send_response(
    mach_port_name_t port,
    const Message *inMessage) {
  Message response = {0};
  response.header.msgh_bits = MACH_MSGH_BITS_SET(
      /* remote */ inMessage->header.msgh_bits &
          MACH_MSGH_BITS_REMOTE_MASK,  // received message already contains
                                       // necessary remote bits to provide a
                                       // response, because it can differ
                                       // depending on SEND/SEND_ONCE.
      /* local */ 0,
      /* voucher */ 0,
      /* other */ 0);
  response.header.msgh_remote_port = port;
  response.header.msgh_id = inMessage->header.msgh_id << 1;
  response.header.msgh_size = sizeof(response);

  response.bodyInt = inMessage->bodyInt << 1;
  strcpy(response.bodyStr, "Response - ");
  strncpy(
      response.bodyStr + strlen(response.bodyStr),
      inMessage->bodyStr,
      sizeof(response.bodyStr) - strlen(response.bodyStr) - 1);

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
    ReceiveMessage *buffer) {
  mach_msg_return_t ret = mach_msg(
      /* msg */ (mach_msg_header_t *)buffer,
      /* option */ MACH_RCV_MSG,
      /* send size */ 0,
      /* recv size */ sizeof(ReceiveMessage),
      /* recv_name */ recvPort,
      /* timeout */ MACH_MSG_TIMEOUT_NONE,
      /* notify port */ MACH_PORT_NULL);
  if (ret != MACH_MSG_SUCCESS) {
    return ret;
  }

  Message *message = &buffer->message;

  printf("got message!\n");
  printf("  bits    : %#x\n", message->header.msgh_bits);
  printf("  id      : %d\n", message->header.msgh_id);
  printf("  bodyS   : %s\n", message->bodyStr);
  printf("  bodyI   : %d\n", message->bodyInt);

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
    ReceiveMessage receiveMessage = {0};

    mach_msg_return_t ret = receive_msg(recvPort, &receiveMessage);
    if (ret != MACH_MSG_SUCCESS) {
      printf("Failed to receive a message: %#x\n", ret);
      continue;
    }

    // Continue if no reply port was specified.
    if (receiveMessage.message.header.msgh_remote_port == MACH_PORT_NULL) {
      continue;
    }

    // Send response if reply port was given.
    ret = send_response(
        receiveMessage.message.header.msgh_remote_port,
        &receiveMessage.message);
    if (ret != MACH_MSG_SUCCESS) {
      printf("Failed to respond: %#x\n", ret);
    }
  }

  return 0;
}
