// mach
#include <bootstrap.h>
#include <dispatch/dispatch.h>
#include <dispatch/source.h>
#include <mach/mach.h>
#include <mach/message.h>

// std
#include <stdio.h>
#include <stdlib.h>

typedef struct {
  mach_msg_header_t header;
  char bodyStr[32];
  int bodyInt;

  // Suitable for use with the default trailer type - no custom trailer
  // information requested using `MACH_RCV_TRAILER_TYPE`, or just the explicit
  // `MACH_RCV_TRAILER_NULL` type.
  mach_msg_trailer_t trailer;
} Message;

mach_msg_return_t receive_msg(mach_port_name_t recvPort) {
  // Message buffer.
  Message message = {0};

  mach_msg_return_t ret = mach_msg(
      /* msg */ (mach_msg_header_t *)&message,
      /* option */ MACH_RCV_MSG,
      /* send size */ 0,
      /* recv size */ sizeof(message),
      /* recv_name */ recvPort,
      /* timeout */ MACH_MSG_TIMEOUT_NONE,
      /* notify port */ MACH_PORT_NULL);
  if (ret != MACH_MSG_SUCCESS) {
    return ret;
  }

  printf("got message!\n");
  printf("  id      : %d\n", message.header.msgh_id);
  printf("  bodyS   : %s\n", message.bodyStr);
  printf("  bodyI   : %d\n", message.bodyInt);

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
    mach_msg_return_t ret = receive_msg(recvPort);
    if (ret != MACH_MSG_SUCCESS) {
      printf("Failed to receive a message: %d\n", ret);
    }
  }

  // dispatch_source_t recvSource = dispatch_source_create(
  //    DISPATCH_SOURCE_TYPE_MACH_RECV,
  //    recvPort,
  //    /* mask */ 0,
  //    dispatch_get_main_queue());

  // dispatch_source_set_event_handler(recvSource, ^() {
  //  kern_return_t ret = retrieve_msg(recvPort);
  //  if (ret != KERN_SUCCESS) {
  //    printf("Failed mach_msg: 0x%x\n", ret);
  //  }
  //});

  // dispatch_resume(recvSource);

  // dispatch_main();

  return 0;
}
