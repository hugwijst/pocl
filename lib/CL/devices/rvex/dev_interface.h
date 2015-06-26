#ifndef POCL_RVEX_DEV_INTERFACE_H
#define POCL_RVEX_DEV_INTERFACE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct rvex_device {
  size_t fpga_id;
  size_t core_id;
};

void get_devices(size_t *n, struct rvex_device **devs);
void free_devices(size_t n, struct rvex_device **devs);


#ifdef __cplusplus
}
#endif

#endif
