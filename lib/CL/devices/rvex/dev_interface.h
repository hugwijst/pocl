#ifndef POCL_RVEX_DEV_INTERFACE_H
#define POCL_RVEX_DEV_INTERFACE_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

struct rvex_core;

void get_devices(size_t *n, struct rvex_core **devs);
struct rvex_core *get_device(size_t n);
void free_devices(size_t n, struct rvex_core **devs);
void free_device(struct rvex_core *dev);

char* rvex_dev_get_name(struct rvex_core *dev);

const char *rvex_dev_get_memfile(struct rvex_core *dev);

uint32_t rvex_dev_get_pc(struct rvex_core *dev);
void rvex_dev_set_pc(struct rvex_core *dev, uint32_t val);

bool rvex_dev_get_reset(struct rvex_core *dev);
void rvex_dev_set_reset(struct rvex_core *dev, bool val);

uint32_t rvex_dev_get_reset_vector(struct rvex_core *dev);
void rvex_dev_set_reset_vector(struct rvex_core *dev, uint32_t val);

bool rvex_dev_get_run(struct rvex_core *dev);
void rvex_dev_set_run(struct rvex_core *dev, bool val);

bool rvex_dev_get_done(struct rvex_core *dev);

#ifdef __cplusplus
}
#endif

#endif
