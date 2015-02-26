#pragma once
#include "pocl_cl.h"

#ifdef __cplusplus
extern "C" {
#endif

int
rvex_llvm_codegen(cl_kernel kernel,
                  cl_device_id device,
                  const char *infilename,
                  const char *outfilename)

#ifdef __cplusplus
}
#endif
