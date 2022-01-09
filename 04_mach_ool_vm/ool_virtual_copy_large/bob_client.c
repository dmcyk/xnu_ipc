#include "../vm_utils.h"
#include "message.h"

// Darwin
#include <bootstrap.h>
#include <mach/mach_init.h>
#include <mach/mach_port.h>
#include <mach/message.h>
#include <mach/port.h>
#include <mach/task.h>

#include <mach/mach_vm.h>
#include <mach/vm_map.h>
#include <mach/vm_region.h>

// std
#include <stdio.h>
#include <stdlib.h>

#define MS_IN_S 1000

mach_msg_return_t
receive_ool_message(mach_port_name_t recvPort, OOLReceiveMessage *rcvMessage) {
  mach_msg_return_t ret = mach_msg(
      /* msg */ (mach_msg_header_t *)rcvMessage,
      /* option */ MACH_RCV_MSG,
      /* send size */ 0,
      /* recv size */ sizeof(*rcvMessage),
      /* recv_name */ recvPort,
      /* timeout */ MACH_MSG_TIMEOUT_NONE,
      /* notify port */ MACH_PORT_NULL);
  if (ret != MACH_MSG_SUCCESS) {
    return ret;
  }

  if (rcvMessage->message.header.msgh_id != MSG_ID_COPY_MEM &&
      rcvMessage->message.header.msgh_id != MSG_ID_INSPECT_MEM) {
    return RCV_ERROR_INVALID_MESSAGE_ID;
  }

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
  mach_port_name_t task = mach_task_self();

  mach_port_t bootstrapPort;
  if (task_get_special_port(task, TASK_BOOTSTRAP_PORT, &bootstrapPort) !=
      KERN_SUCCESS) {
    return EXIT_FAILURE;
  }

  mach_port_t port;
  if (bootstrap_look_up(bootstrapPort, "xyz.dmcyk.alice.as-a-service", &port) !=
      KERN_SUCCESS) {
    return EXIT_FAILURE;
  }

  mach_port_t replyPort;
  if (mach_port_allocate(task, MACH_PORT_RIGHT_RECEIVE, &replyPort) !=
      KERN_SUCCESS) {
    return EXIT_FAILURE;
  }

  if (mach_port_insert_right(
          task, replyPort, replyPort, MACH_MSG_TYPE_MAKE_SEND) !=
      KERN_SUCCESS) {
    return EXIT_FAILURE;
  }

  Message message = {0};
  message.header.msgh_remote_port = port;
  message.header.msgh_local_port = replyPort;

  // Setup message rights.
  message.header.msgh_bits = MACH_MSGH_BITS_SET(
      /* remote */ MACH_MSG_TYPE_COPY_SEND,
      /* local */ MACH_MSG_TYPE_MAKE_SEND,
      /* voucher */ 0,
      /* other */ 0);
  message.header.msgh_id = MSG_ID_INSPECT_MEM;
  message.header.msgh_size = sizeof(message);

  kern_return_t ret = mach_msg(
      /* msg */ (mach_msg_header_t *)&message,
      /* option */ MACH_SEND_MSG,
      /* send size */ sizeof(message),
      /* recv size */ 0,
      /* recv_name */ MACH_PORT_NULL,
      /* timeout */ MACH_MSG_TIMEOUT_NONE,
      /* notify port */ MACH_PORT_NULL);
  if (ret != MACH_MSG_SUCCESS) {
    printf("Failed mach_msg: %d\n", ret);
    return EXIT_FAILURE;
  }

  while (true) {
    OOLReceiveMessage rcvMessage = {0};
    ret = receive_ool_message(replyPort, &rcvMessage);
    if (ret != MACH_MSG_SUCCESS) {
      printf("Failed to receive an OOL message: %#x\n", ret);
      return 1;
    }

    printf(
        "%s\n"
        "  buffer addr: %p\n"
        "  total buffer size: %x\n"
        "  copy option: %s\n",
        (const char *)rcvMessage.message.descriptor.address,
        rcvMessage.message.descriptor.address,
        rcvMessage.message.descriptor.size,
        mach_copy_option_to_str(rcvMessage.message.descriptor.copy));

    if (rcvMessage.message.header.msgh_id == MSG_ID_INSPECT_MEM) {
      vm_region_dump_submap_info(rcvMessage.message.descriptor.address);
      printf("\n");

      if (rcvMessage.message.fill_all) {
        void *start = rcvMessage.message.descriptor.address;
        void *end =
            (void *)((vm_address_t)start + rcvMessage.message.descriptor.size);
        fill_pages(start, end, "foo");
      }
    }
  }

  return 0;
}
