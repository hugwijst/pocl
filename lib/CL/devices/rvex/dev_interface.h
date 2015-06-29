#ifndef POCL_RVEX_DEV_INTERFACE_H
#define POCL_RVEX_DEV_INTERFACE_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

struct rvex_device;

void get_devices(size_t *n, struct rvex_device **devs);
struct rvex_device *get_device(size_t n);
void free_devices(size_t n, struct rvex_device **devs);
void free_device(struct rvex_device *dev);

char* rvex_dev_get_name(struct rvex_device *dev);

uint32_t rvex_dev_get_pc(struct rvex_device *dev);
void rvex_dev_set_pc(struct rvex_device *dev, uint32_t val);

bool rvex_dev_get_reset(struct rvex_device *dev);
void rvex_dev_set_reset(struct rvex_device *dev, bool val);

bool rvex_dev_get_run(struct rvex_device *dev);
void rvex_dev_set_run(struct rvex_device *dev, bool val);

bool rvex_dev_get_done(struct rvex_device *dev);

#ifdef __cplusplus
}
#endif

#endif
