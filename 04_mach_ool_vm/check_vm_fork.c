#include "vm_utils.h"

// std
#include <fcntl.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

#define VM_REGION_DUMP vm_region_dump_submap_info

int main() {
  vm_address_t _oolBuffer = 0;
  void *oolBuffer = 0;
  vm_size_t oolBufferSize = vm_page_size;

  if (vm_map(
          mach_task_self(),
          &_oolBuffer,
          oolBufferSize,
          /* mask */ 0,
          VM_FLAGS_ANYWHERE,
          MACH_PORT_NULL,
          0,
          /* copy */ false,
          VM_PROT_DEFAULT,
          VM_PROT_ALL,
          VM_INHERIT_COPY) != KERN_SUCCESS) {
    return 1;
  }

  oolBuffer = (void *)_oolBuffer;

  VM_REGION_DUMP(oolBuffer);
  strcpy((char *)oolBuffer, "foo");

  printf("\nregion after write\n");
  VM_REGION_DUMP(oolBuffer);

  printf("\nstart fork\n\n");

  if (fork() == 0) {
    printf("child pid: %d\n", getpid());

    VM_REGION_DUMP(oolBuffer);

    strcpy((char *)oolBuffer, "child");

    printf("\nregion after write\n");
    VM_REGION_DUMP(oolBuffer);
    printf("\n");
  } else {
    sleep(1);

    printf("parent pid: %d\n", getpid());
    printf("parent print buffer: %s\n", (char *)oolBuffer);

    VM_REGION_DUMP(oolBuffer);

    strcpy((char *)oolBuffer, "child");

    printf("\nregion after write\n");
    VM_REGION_DUMP(oolBuffer);
  }

  // Sleep at the end so either of the processes doesn't get killed before all
  // the region cases are dumped.
  sleep(2);

  return 0;
}
