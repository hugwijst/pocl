#include "dev_interface.h"

#include "pocl_runtime_config.h"

#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Path.h"

#include <sstream>
#include <fstream>

namespace fs = llvm::sys::fs;
namespace path = llvm::sys::path;

struct rvex_device {
  size_t fpga_id;
  size_t core_id;

  std::string core_path;
  std::string cdev_path;
};

fs::directory_iterator get_fpga_iter() {
  if (!fs::is_directory("/sys/class/rvex"))
    return fs::directory_iterator();

  std::error_code ec;
  return fs::directory_iterator("/sys/class/rvex", ec);
}

void get_devices(size_t *n, struct rvex_device **devs) {
  *n = 0;
  size_t nfpga = 0, ncore = 0;
  fs::directory_iterator end;

  fs::directory_iterator fpga_iter = get_fpga_iter();

  while(fpga_iter != end) {
    std::error_code ec;

    fs::directory_iterator core_iter(fpga_iter->path(), ec);

    while(core_iter != end) {
      const std::string &core_path = fpga_iter->path();

      if (!fs::is_directory(core_path)
          || !path::filename(core_path).startswith("core")) {
        continue;
      }

      if(devs != nullptr) {
        struct rvex_device *dev = new rvex_device;

        dev->fpga_id = nfpga;
        dev->core_id = ncore;
        dev->core_path = core_path;
        std::ostringstream oss;
        oss << "/dev/rvex" << *n;
        dev->cdev_path = oss.str();

        devs[*n] = dev;
      }

      ncore++;
      (*n)++;
      core_iter.increment(ec);
    }

    nfpga++;
    fpga_iter.increment(ec);
  }

  if (pocl_get_bool_option("POCL_FAKE_RVEX", 0)) {
    if(devs != nullptr) {
      struct rvex_device *dev = new rvex_device;

      dev->fpga_id = nfpga;
      dev->core_id = ncore;
      dev->core_path = "FAKE";
      std::ostringstream oss;
      oss << "/dev/rvex" << *n;
      dev->cdev_path = oss.str();

      devs[*n] = dev;
    }

    (*n)++;
  }
}

struct rvex_device * get_device(const size_t id) {
  size_t nfpga = 0, ncore = 0, n = 0;
  fs::directory_iterator end;
  fs::directory_iterator fpga_iter = get_fpga_iter();

  while(fpga_iter != end) {
    std::error_code ec;

    fs::directory_iterator core_iter(fpga_iter->path(), ec);

    while(core_iter != end) {
      const std::string &core_path = fpga_iter->path();

      if (!fs::is_directory(core_path)
          || !path::filename(core_path).startswith("core")) {
        continue;
      }

      if(n == id) {
        struct rvex_device *dev = new rvex_device;

        dev->fpga_id = nfpga;
        dev->core_id = ncore;
        dev->core_path = core_path;
        std::ostringstream oss;
        oss << "/dev/rvex" << n;
        dev->cdev_path = oss.str();

        return dev;
      }

      ncore++;
      n++;
      core_iter.increment(ec);
    }

    nfpga++;
    fpga_iter.increment(ec);
  }

  if (pocl_get_bool_option("POCL_FAKE_RVEX", 0)) {
    if(n == id) {
      struct rvex_device *dev = new rvex_device;

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

  return nullptr;
}

void free_devices(size_t n, struct rvex_device **devs) {
  for(size_t i = 0; i < n; ++i) {
    delete devs[i];
  }
}

void free_device(struct rvex_device *dev) {
    delete dev;
}

/**
 * Get the name of the device.
 *
 * The returned string should be freed by the caller.
 */
char *rvex_dev_get_name(struct rvex_device *dev) {
  const char *fmt_str = "fpga%d - core%d";

  int size = snprintf(NULL, 0, fmt_str, dev->fpga_id, dev->core_id);

  char *res = (char*)malloc(size+1);
  if (res == nullptr) {
    return res;
  }

  snprintf(res, size+1, fmt_str, dev->fpga_id, dev->core_id);
  return res;
}

#define RVEX_DEV_GET(_reg, _type)                                             \
  _type rvex_dev_get_##_reg (struct rvex_device *dev) {                       \
    _type res;                                                                \
    std::ifstream f(dev->core_path + "/context0/"#_reg);                      \
                                                                              \
    f >> res;                                                                 \
    return res;                                                               \
  }

#define RVEX_DEV_SET(_reg, _type)                                             \
  void rvex_dev_set_##_reg (struct rvex_device *dev, _type val) {             \
    std::ofstream f(dev->core_path + "/context0/"#_reg);                      \
    f << val;                                                                 \
  }

RVEX_DEV_GET(pc, uint32_t);
RVEX_DEV_SET(pc, uint32_t);

RVEX_DEV_GET(reset, bool);
RVEX_DEV_SET(reset, bool);

RVEX_DEV_GET(run, bool);
RVEX_DEV_SET(run, bool);

RVEX_DEV_GET(done, bool);
