// Darwin
#include <bootstrap.h>
#include <mach/mach_init.h>
#include <mach/mach_port.h>
#include <mach/message.h>
#include <mach/port.h>
#include <mach/task.h>

// std
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

typedef struct {
  mach_msg_header_t header;
  char bodyStr[32];
  int bodyInt;
} Message;

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

  Message message = {0};
  message.header.msgh_remote_port = port;

  // copy send right on the remote port
  message.header.msgh_bits = MACH_MSGH_BITS_SET(
      /* remote */ MACH_MSG_TYPE_COPY_SEND,
      /* local */ 0,
      /* voucher */ 0,
      /* other */ 0);
  message.header.msgh_id = 4;
  message.header.msgh_size = sizeof(message);

  strcpy(message.bodyStr, "Hello Mach!");
  message.bodyInt = 0xff;

  mach_msg_return_t ret = mach_msg(
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

  strcpy(message.bodyStr, "Hello Mach! #2");

  ret = mach_msg(
      /* msg */ (mach_msg_header_t *)&message,
      /* option */ MACH_SEND_MSG,
      /* send size */ sizeof(message),
      /* recv size */ 0,
      /* recv_name */ MACH_PORT_NULL,
      /* timeout */ MACH_MSG_TIMEOUT_NONE,
      /* notify port */ MACH_PORT_NULL);
  if (ret != MACH_MSG_SUCCESS) {
    printf("#2. Failed mach_msg: %d\n", ret);
    return EXIT_FAILURE;
  }

  // Make client sleep for a while to inspect ports using `lsmp`.
  sleep(60);

  return 0;
}
