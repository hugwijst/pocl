#pragma once
#include "pocl_cl.h"

const char* rvex_llvm_codegen (const char* tmpdir,
                          cl_kernel kernel,
                          cl_device_id device);

const char* rvex_link (const char* tmpdir, const char* objfile,
    size_t start_address, cl_kernel kernel);
