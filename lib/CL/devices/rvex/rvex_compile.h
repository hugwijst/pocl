#pragma once
#include "pocl_cl.h"

const char* rvex_llvm_codegen (const char* tmpdir,
                          cl_kernel kernel,
                          cl_device_id device);
