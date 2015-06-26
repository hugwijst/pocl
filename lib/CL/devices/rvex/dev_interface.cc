#include "dev_interface.h"

#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Path.h"

namespace fs = llvm::sys::fs;
namespace path = llvm::sys::path;

void get_devices(size_t *n, struct rvex_device **devs) {
  *n = 0;
  size_t nfpga = 0, ncore = 0;

  if (!fs::is_directory("/sys/class/rvex"))
    return;

  std::error_code ec;
  fs::directory_iterator fpga_iter("/sys/class/rvex", ec);
  fs::directory_iterator end;

  do {
    fs::directory_iterator core_iter(fpga_iter->path(), ec);

    do {
      const std::string &core_path = fpga_iter->path();

      if (!fs::is_directory(core_path)
          || !path::filename(core_path).startswith("core")) {
        continue;
      }

      if(devs != NULL) {
        struct rvex_device *dev = new rvex_device;

        dev->fpga_id = nfpga;
        dev->core_id = ncore;

        devs[*n] = dev;
      }

      (*n)++;
    } while(core_iter.increment(ec) != end);


  } while(fpga_iter.increment(ec) != end);
}

void free_devices(size_t n, struct rvex_device **devs) {
  for(size_t i = 0; i < n; ++i) {
    delete devs[i];
  }
}
