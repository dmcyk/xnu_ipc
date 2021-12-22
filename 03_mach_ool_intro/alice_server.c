#include "message.h"

// Darwin
#include <bootstrap.h>
#include <mach/mach.h>
#include <mach/message.h>

// std
#include <stdio.h>
#include <stdlib.h>

mach_msg_return_t send_reply(mach_port_name_t port, const Message *inMessage) {
  Message response = {0};

  response.header.msgh_bits =
      inMessage->header.msgh_bits & // received message already contains
      MACH_MSGH_BITS_REMOTE_MASK;   // necessary remote bits to provide a
                                    // response, and it can differ
                                    // depending on SEND/SEND_ONCE right.

  response.header.msgh_remote_port = port;
  response.header.msgh_id = inMessage->header.msgh_id;
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

mach_msg_return_t send_ool_reply(
    mach_port_name_t port,
    const Message *inMessage,
    void *addr,
    mach_msg_size_t size) {
  OOLMessage message = {0};
  message.header.msgh_bits = MACH_MSGH_BITS_SET(
      /* remote */ MACH_MSG_TYPE_COPY_SEND,
      /* local */ 0,
      /* voucher */ 0,
      /* other */ MACH_MSGH_BITS_COMPLEX);
  message.header.msgh_remote_port = port;
  message.header.msgh_id = inMessage->header.msgh_id;
  message.header.msgh_size = sizeof(message);
  message.msgh_descriptor_count = 1;

  message.descriptor.address = addr;
  message.descriptor.size = size;
  message.descriptor.copy = MACH_MSG_VIRTUAL_COPY;
  message.descriptor.deallocate = false;
  message.descriptor.type = MACH_MSG_OOL_DESCRIPTOR;

  return mach_msg(
      /* msg */ (mach_msg_header_t *)&message,
      /* option */ MACH_SEND_MSG,
      /* send size */ sizeof(message),
      /* recv size */ 0,
      /* recv_name */ MACH_PORT_NULL,
      /* timeout */ MACH_MSG_TIMEOUT_NONE,
      /* notify port */ MACH_PORT_NULL);
}

mach_msg_return_t
receive_msg(mach_port_name_t recvPort, ReceiveMessage *buffer) {
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

  Message *message = &buffer->message;

  printf("got message!\n");
  printf("  id      : %d\n", message->header.msgh_id);
  printf("  bodyS   : %s\n", message->bodyStr);
  printf("  bodyI   : %d\n", message->bodyInt);

  return MACH_MSG_SUCCESS;
}

void fill_pages(void *start, void *end, const char *data) {
  while (start < end) {
    strcpy((char *)start, data);

    start = (void *)((vm_address_t)start + vm_page_size);
    uint32_t *pageEnd = (uint32_t *)start;
    *(pageEnd - 1) = arc4random();
  }
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

  void *oolBuffer = NULL;
  vm_size_t oolBufferSize = vm_page_size;
  if (vm_allocate(
          mach_task_self(),
          (vm_address_t *)&oolBuffer,
          oolBufferSize,
          VM_PROT_READ | VM_PROT_WRITE) != KERN_SUCCESS) {
    printf("Failed to allocate memory buffer\n");
    return 1;
  }

  strcpy((char *)oolBuffer, "Hello, OOL data!");

  while (true) {
    // Message buffer.
    ReceiveMessage receiveMessage = {0};

    mach_msg_return_t ret = receive_msg(recvPort, &receiveMessage);
    if (ret != MACH_MSG_SUCCESS) {
      printf("Failed to receive a message: %#x\n", ret);
      continue;
    }

    // Continue if there's no reply port.
    if (receiveMessage.message.header.msgh_remote_port == MACH_PORT_NULL) {
      continue;
    }

    switch (receiveMessage.message.header.msgh_id) {
    case MSG_ID_COPY_MEM:
      // As per request, respond with OOL memory.
      ret = send_ool_reply(
          receiveMessage.message.header.msgh_remote_port,
          &receiveMessage.message,
          oolBuffer,
          oolBufferSize);
      break;

    default:
      // Send a generic, inline response.
      ret = send_reply(
          receiveMessage.message.header.msgh_remote_port,
          &receiveMessage.message);
      break;
    }

    if (ret != MACH_MSG_SUCCESS) {
      printf("Failed to respond: %#x\n", ret);
    }
  }

  return 0;
}
