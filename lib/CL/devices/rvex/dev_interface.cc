#include "dev_interface.h"

#include "pocl_runtime_config.h"

#include <libudev.h>

#include <iomanip>
#include <sstream>
#include <fstream>

#include <cstring>

struct rvex_core {
  struct udev *udev;
  struct udev_device *dev;

  rvex_core(struct udev_device *core_dev) {
    udev = udev_device_get_udev(core_dev);
    dev = core_dev;

    udev_ref(udev);
    udev_device_ref(dev);
  }

  ~rvex_core() {
    udev_device_unref(dev);
    udev_unref(udev);
  }

  std::string name() {
    return udev_device_get_sysname(dev);
  }

  std::string fpga_name() {
    struct udev_device *parent = udev_device_get_parent(dev);
    if(parent == nullptr) {
      return nullptr;
    }
    return udev_device_get_sysname(parent);
  }

  std::string syspath() {
    return udev_device_get_syspath(dev);
  }

  const char *mem_devnode() {
    struct udev_device *parent = udev_device_get_parent(dev);
    if(parent == nullptr) {
      return nullptr;
    }
    return udev_device_get_devnode(parent);
  }
};


void get_devices(size_t *n, struct rvex_core **devs) {
  struct udev *udev;
  struct udev_enumerate *enumerate;
  struct udev_list_entry *devices, *dev_list_entry;
  struct udev_device *dev;

  *n = 0;

  /* create new udev context */
  udev = udev_new();
  if (udev == nullptr) {
    return;
  }

  /* scan for rvex core devices */
  enumerate = udev_enumerate_new(udev);
  udev_enumerate_add_match_subsystem(enumerate, "rvex-core");
  udev_enumerate_scan_devices(enumerate);
  devices = udev_enumerate_get_list_entry(enumerate);

  udev_list_entry_foreach(dev_list_entry, devices) {
    if(devs != nullptr) {
      const char *path;

      path = udev_list_entry_get_name(dev_list_entry);
      dev = udev_device_new_from_syspath(udev, path);

      devs[*n] = new rvex_core(dev);

      udev_device_unref(dev);
    }

    (*n)++;
  }

  udev_enumerate_unref(enumerate);

  udev_unref(udev);
}

struct rvex_core * get_device(const size_t id) {
  struct udev *udev;
  struct udev_enumerate *enumerate;
  struct udev_list_entry *devices, *dev_list_entry;
  struct udev_device *dev;

  size_t n = 0;

  /* create new udev context */
  udev = udev_new();
  if (udev == nullptr) {
    return nullptr;
  }

  /* scan for rvex core devices */
  enumerate = udev_enumerate_new(udev);
  udev_enumerate_add_match_subsystem(enumerate, "rvex-core");
  udev_enumerate_scan_devices(enumerate);
  devices = udev_enumerate_get_list_entry(enumerate);

  udev_list_entry_foreach(dev_list_entry, devices) {
    if(n == id) {
      const char *path;

      path = udev_list_entry_get_name(dev_list_entry);
      dev = udev_device_new_from_syspath(udev, path);

      struct rvex_core *core = new rvex_core(dev);

      udev_device_unref(dev);
      udev_enumerate_unref(enumerate);
      udev_unref(udev);

      return core;
    }

    n++;
  }

  udev_enumerate_unref(enumerate);

  udev_unref(udev);
#if 0
  if (pocl_get_bool_option("POCL_FAKE_RVEX", 0)) {
    if(n == id) {
      struct rvex_core *dev = new rvex_core;

      dev->fpga_id = nfpga;
      dev->core_id = ncore;
      dev->core_path = "FAKE";
      std::ostringstream oss;
      oss << "/dev/rvex" << n;
      dev->cdev_path = oss.str();

      return dev;
    }

    n++;
  }
#endif

  return nullptr;
}

void free_devices(size_t n, struct rvex_core **devs) {
  for(size_t i = 0; i < n; ++i) {
    delete devs[i];
  }
}

void free_device(struct rvex_core *dev) {
    delete dev;
}

/**
 * Get the name of the device.
 *
 * The returned string should be freed by the caller.
 */
char *rvex_dev_get_name(struct rvex_core *core) {
  std::string name = core->fpga_name() + " - " + core->name();

  char *res = (char*)malloc(name.size()+1);
  if (res == nullptr) {
    return res;
  }

  std::memcpy(res, name.c_str(), name.size()+1);
  return res;
}

const char *rvex_dev_get_memfile(struct rvex_core *core) {
  return core->mem_devnode();
}

/* TODO: use get/set_sysattr_value */
#define RVEX_DEV_GET(_reg, _type)                                             \
  _type rvex_dev_get_##_reg (struct rvex_core *core) {                        \
    _type res;                                                                \
    std::ifstream f(core->syspath() + "/context0/"#_reg);                     \
                                                                              \
    f >> std::setbase(0) >> res;                                              \
    return res;                                                               \
  }

#define RVEX_DEV_SET(_reg, _type)                                             \
  void rvex_dev_set_##_reg (struct rvex_core *core, _type val) {              \
    std::ofstream f(core->syspath() + "/context0/"#_reg);                     \
    f << val;                                                                 \
  }

RVEX_DEV_GET(pc, uint32_t);
RVEX_DEV_SET(pc, uint32_t);

RVEX_DEV_GET(reset, bool);
RVEX_DEV_SET(reset, bool);

RVEX_DEV_GET(reset_vector, uint32_t);
RVEX_DEV_SET(reset_vector, uint32_t);

RVEX_DEV_GET(run, bool);
RVEX_DEV_SET(run, bool);

RVEX_DEV_GET(done, bool);

/* Get committed bundle count */
RVEX_DEV_GET(cbun,   uint32_t);
/* Get active cycles (includes stalled cycles) */
RVEX_DEV_GET(ccyc,   uint32_t);
/* Get committed nop count */
RVEX_DEV_GET(cnop,   uint32_t);
/* Get stalled cycles */
RVEX_DEV_GET(cstall, uint32_t);
/* Get committed syllable count */
RVEX_DEV_GET(csyl,   uint32_t);
