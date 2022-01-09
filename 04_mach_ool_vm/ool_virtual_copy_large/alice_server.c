#include "../vm_utils.h"
#include "message.h"

// Darwin
#include <bootstrap.h>
#include <mach/mach.h>
#include <mach/message.h>

// std
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

mach_msg_return_t send_ool_reply(
    mach_port_name_t port,
    void *addr,
    mach_msg_size_t size,
    boolean_t fillAll,
    mach_msg_copy_options_t copy,
    boolean_t deallocate) {
  OOLMessage message = {0};
  message.header.msgh_bits = MACH_MSGH_BITS_SET(
      /* remote */ MACH_MSG_TYPE_COPY_SEND,
      /* local */ 0,
      /* voucher */ 0,
      /* other */ MACH_MSGH_BITS_COMPLEX);
  message.header.msgh_remote_port = port;
  message.header.msgh_id = MSG_ID_INSPECT_MEM;
  message.header.msgh_size = sizeof(message);
  message.msgh_descriptor_count = 1;

  message.descriptor.address = addr;
  message.descriptor.size = size;
  message.descriptor.copy = copy;
  message.descriptor.deallocate = deallocate;
  message.descriptor.type = MACH_MSG_OOL_DESCRIPTOR;

  message.fill_all = fillAll;

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
  return mach_msg(
      /* msg */ (mach_msg_header_t *)buffer,
      /* option */ MACH_RCV_MSG,
      /* send size */ 0,
      /* recv size */ sizeof(*buffer),
      /* recv_name */ recvPort,
      /* timeout */ MACH_MSG_TIMEOUT_NONE,
      /* notify port */ MACH_PORT_NULL);
}

void fill_pages(void *start, void *end, const char *data) {
  while (start < end) {
    strcpy((char *)start, data);

    start = (void *)((vm_address_t)start + vm_page_size);
    uint32_t *pageEnd = (uint32_t *)start;
    *(pageEnd - 1) = arc4random();
  }
}

kern_return_t
send_memory_inspection_messages(const ReceiveMessage *receiveMessage) {
  typedef struct {
    const char *name;
    vm_size_t size;
    mach_msg_copy_options_t copy;
    boolean_t deallocate;
    boolean_t fill_all;
    boolean_t fill_all_client;
  } MemoryOptions;

  MemoryOptions testCases[] = {
      {.name = "VIRTUAL;2GB;NO_FREE",
       .size = /* 2GB */ 2147483648,
       .copy = MACH_MSG_VIRTUAL_COPY,
       .deallocate = false,
       .fill_all = true,
       .fill_all_client = true},
      {.name = "VIRTUAL;2GB;NO_FREE",
       .size = /* 2GB */ 2147483648,
       .copy = MACH_MSG_VIRTUAL_COPY,
       .deallocate = false,
       .fill_all = true,
       .fill_all_client = false},
  };

  for (int i = 0; i < sizeof(testCases) / sizeof(testCases[0]); ++i) {
    MemoryOptions testCase = testCases[i];

    void *oolBuffer = NULL;
    mach_vm_size_t oolBufferSize = testCase.size;
    kern_return_t ret = mach_vm_allocate(
        mach_task_self(),
        (mach_vm_address_t *)&oolBuffer,
        oolBufferSize,
        VM_PROT_READ | VM_PROT_WRITE);
    if (ret != KERN_SUCCESS) {
      printf("Failed to allocate memory buffer: %x\n", ret);
      return KERN_FAILURE;
    }

    void *oolBufferEnd = (void *)((vm_address_t)oolBuffer + testCase.size);

    // Copy test case name.
    strcpy((char *)oolBuffer, testCase.name);

    if (testCase.fill_all) {
      fill_pages(oolBuffer, oolBufferEnd, testCase.name);
    }

    ret = send_ool_reply(
        receiveMessage->message.header.msgh_remote_port,
        oolBuffer,
        oolBufferSize,
        testCase.fill_all_client,
        testCase.copy,
        testCase.deallocate);
    printf("sent test case message: #%d, res - %d\n", i + 1, ret);

    // Stop if deallocated, the address is no longer valid here.
    if (testCase.deallocate) {
      continue;
    }

    vm_region_dump_submap_info(oolBuffer);

    if (testCase.fill_all) {
      fill_pages(oolBuffer, oolBufferEnd, "some other data\n");
    }
  }

  return KERN_SUCCESS;
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

    // Continue if there's no reply port.
    if (receiveMessage.message.header.msgh_remote_port == MACH_PORT_NULL) {
      continue;
    }

    switch (receiveMessage.message.header.msgh_id) {
    case MSG_ID_INSPECT_MEM:
      // Send several messages to inspect the behaviour of virtual memory for
      // different OOL options.
      ret = send_memory_inspection_messages(&receiveMessage);
      break;

    default:
      printf("Got message with unhandled id\n");
      break;
    }

    if (ret != MACH_MSG_SUCCESS) {
      printf("Failed to respond: %#x\n", ret);
    }
  }

  return 0;
}
