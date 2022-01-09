#pragma once

// Darwin
#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <mach/vm_map.h>

// std
#include <stdio.h>
#include <unistd.h>

static const char *vm_share_mode_to_str(unsigned char share_mode) {
  switch (share_mode) {
  case SM_COW:
    return "SM_COW";
  case SM_PRIVATE:
    return "SM_PRIVATE";
  case SM_EMPTY:
    return "SM_EMPTY";
  case SM_SHARED:
    return "SM_SHARED";
  case SM_TRUESHARED:
    return "SM_TRUESHARED";
  case SM_PRIVATE_ALIASED:
    return "SM_PRIVATE_ALIASED";
  case SM_SHARED_ALIASED:
    return "SM_SHARED_ALIASED";
  case SM_LARGE_PAGE:
    return "SM_LARGE_PAGE";
  default:
    return "unknown";
  }
}

static const char *vm_prot_to_str(vm_prot_t prot) {
  if (prot == VM_PROT_ALL) {
    return "VM_PROT_ALL";
  }

  if (prot == VM_PROT_DEFAULT) {
    return "VM_PROT_DEFAULT";
  }

  if (prot == VM_PROT_EXECUTE_ONLY) {
    return "VM_PROT_EXECUTE_ONLY";
  }

  char buffer[1024] = {0};
  sprintf(buffer, "%#x", prot);

  // note, this will leak memory
  return strdup(buffer);
}

static const char *mach_copy_option_to_str(mach_msg_copy_options_t option) {
  switch (option) {
  case MACH_MSG_PHYSICAL_COPY:
    return "MACH_MSG_PHYSICAL_COPY";
  case MACH_MSG_VIRTUAL_COPY:
    return "MACH_MSG_VIRTUAL_COPY";
  default:
    return "unknown";
  }
}

static kern_return_t vm_region_dump_basic_info(void *_addr) {
  mach_vm_address_t addr = (mach_vm_address_t)_addr;
  mach_vm_size_t size = vm_page_size;

  vm_region_basic_info_data_64_t info = {0};
  vm_region_flavor_t flavour = VM_REGION_BASIC_INFO_64;
  mach_msg_type_number_t infoCnt = VM_REGION_BASIC_INFO_COUNT_64;
  mach_port_name_t object;

  kern_return_t res = mach_vm_region(
      mach_task_self(),
      &addr,
      &size,
      flavour,
      (vm_region_info_t)&info,
      &infoCnt,
      &object);
  if (res != KERN_SUCCESS) {
    return res;
  }

  printf(
      "region info: %p - %p (%#llx)\n"
      "  prot: %s\n"
      "  maxprot: %s\n"
      "  inheritance: %u\n"
      "  shared: %d\n"
      "  reserved: %d\n"
      "  offset: %llu\n"
      "  behavior: %d\n",
      (void *)addr,
      (void *)(addr + size),
      size,
      vm_prot_to_str(info.protection),
      vm_prot_to_str(info.max_protection),
      info.inheritance,
      info.shared,
      info.reserved,
      info.offset,
      info.behavior);

  return KERN_SUCCESS;
}

static kern_return_t vm_region_dump_extended_info(void *_addr) {
  mach_vm_address_t addr = (mach_vm_address_t)_addr;
  mach_vm_size_t size = vm_page_size;

  vm_region_extended_info_data_t info = {0};
  vm_region_flavor_t flavour = VM_REGION_EXTENDED_INFO;
  mach_msg_type_number_t infoCnt = VM_REGION_EXTENDED_INFO_COUNT;
  mach_port_name_t object;

  kern_return_t ret = mach_vm_region(
      mach_task_self(),
      &addr,
      &size,
      flavour,
      (vm_region_info_t)&info,
      &infoCnt,
      &object);
  if (ret != KERN_SUCCESS) {
    return ret;
  }

  printf(
      "region info: %p - %p (%#llx)\n"
      "  prot: %s\n"
      "  pages_resident: %u\n"
      "  pages_shared_now_private: %u\n"
      "  pages_swapped_out: %u\n"
      "  pages_dirtied: %u\n"
      "  ref_count: %u\n"
      "  shadow_depth: %u\n"
      "  external_pager: %u\n"
      "  share_mode: %s\n"
      "  pages_reusable: %u\n",
      (void *)addr,
      (void *)(addr + size),
      size,
      vm_prot_to_str(info.protection),
      info.pages_resident,
      info.pages_shared_now_private,
      info.pages_swapped_out,
      info.pages_dirtied,
      info.ref_count,
      (unsigned int)info.shadow_depth,
      (unsigned int)info.external_pager,
      vm_share_mode_to_str(info.share_mode),
      info.pages_reusable);

  return KERN_SUCCESS;
}

static kern_return_t vm_region_get_submap_info(
    mach_vm_address_t *addr,
    mach_vm_size_t *size,
    natural_t *nestingDepth,
    vm_region_submap_info_data_64_t *info) {
  mach_msg_type_number_t infoCnt = VM_REGION_SUBMAP_INFO_COUNT_64;

  return mach_vm_region_recurse(
      mach_task_self(),
      addr,
      size,
      nestingDepth,
      (vm_region_recurse_info_t)info,
      &infoCnt);
}

static kern_return_t vm_region_dump_submap_info(void *_addr) {
  mach_vm_address_t addr = (mach_vm_address_t)_addr;
  mach_vm_size_t size = 0;
  vm_region_submap_info_data_64_t info = {0};

  // Use high nesting depth to get the most nested submap.
  natural_t nestingDepth = 99;

  kern_return_t ret =
      vm_region_get_submap_info(&addr, &size, &nestingDepth, &info);
  if (ret != KERN_SUCCESS) {
    return ret;
  }

  printf(
      "region info: %p - %p (%#llx)\n"
      "  prot: %s\n"
      "  maxprot: %s\n"
      "  nestingDepth: %d\n"
      "  inheritance: %u\n"
      "  pages_resident: %u\n"
      "  pages_shared_now_private: %u\n"
      "  pages_swapped_out: %u\n"
      "  pages_dirtied: %u\n"
      "  ref_count: %u\n"
      "  shadow_depth: %u\n"
      "  external_pager: %u\n"
      "  share_mode: %s\n"
      "  is_submap: %d\n"
      "  behavior: %d\n"
      "  pages_reusable: %u\n",
      (void *)addr,
      (void *)(addr + size),
      size,
      vm_prot_to_str(info.protection),
      vm_prot_to_str(info.max_protection),
      nestingDepth,
      info.inheritance,
      info.pages_resident,
      info.pages_shared_now_private,
      info.pages_swapped_out,
      info.pages_dirtied,
      info.ref_count,
      (unsigned int)info.shadow_depth,
      (unsigned int)info.external_pager,
      vm_share_mode_to_str(info.share_mode),
      (int)info.is_submap,
      info.behavior,
      info.pages_reusable);

  return KERN_SUCCESS;
}
