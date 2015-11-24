/* common.c - common code that can be reused between device driver 
              implementations

   Copyright (c) 2011-2013 Universidad Rey Juan Carlos and
                           Pekka Jääskeläinen / Tampere Univ. of Technology
   
   Permission is hereby granted, free of charge, to any person obtaining a copy
   of this software and associated documentation files (the "Software"), to deal
   in the Software without restriction, including without limitation the rights
   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
   copies of the Software, and to permit persons to whom the Software is
   furnished to do so, subject to the following conditions:
   
   The above copyright notice and this permission notice shall be included in
   all copies or substantial portions of the Software.
   
   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
   THE SOFTWARE.
*/
#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _MSC_VER
#  include <unistd.h>
#else
#  include "vccompat.hpp"
#endif

#include "config.h"

#include "pocl_util.h"
#include "devices.h"
#include "pocl_mem_management.h"
#include "pocl_runtime_config.h"
#include "pocl_llvm.h"

#define COMMAND_LENGTH 2048

/**
 * Generate code from the final bitcode using the LLVM
 * tools.
 *
 * Uses an existing (cached) one, if available.
 *
 * @param tmpdir The directory of the work-group function bitcode.
 * @param return the generated binary filename.
 */
const char*
llvm_codegen (const char* tmpdir, cl_kernel kernel, cl_device_id device, char as_obj) {

  const char* pocl_verbose_ptr = 
    pocl_get_string_option("POCL_VERBOSE", (char*)NULL);
  int pocl_verbose = pocl_verbose_ptr && *pocl_verbose_ptr;

  char command[COMMAND_LENGTH];
  char bytecode[POCL_FILENAME_LENGTH];
  char objfile[POCL_FILENAME_LENGTH];

  char* module = (char*) malloc(min(POCL_FILENAME_LENGTH, 
	   strlen(tmpdir) + strlen(kernel->function_name) + 5)); // strlen of / .so 4+1

  int error;

  error = snprintf 
    (module, POCL_FILENAME_LENGTH,
     "%s/%s.so", tmpdir, kernel->function_name);

  assert (error >= 0);

  error = snprintf
    (objfile, POCL_FILENAME_LENGTH,
     "%s/%s.so.o", tmpdir, kernel->function_name);
  assert (error >= 0);


  if (access (module, F_OK) != 0)
    {
      error = snprintf (bytecode, POCL_FILENAME_LENGTH,
                        "%s/%s", tmpdir, POCL_PARALLEL_BC_FILENAME);
      assert (error >= 0);
      
      error = pocl_llvm_codegen( kernel, device, bytecode, objfile, as_obj);
      assert (error == 0);

      // clang is used as the linker driver in LINK_CMD
      error = snprintf (command, COMMAND_LENGTH,
#ifndef POCL_ANDROID
            LINK_CMD " " HOST_CLANG_FLAGS " " HOST_LD_FLAGS " -o %s %s",
#else
            POCL_ANDROID_PREFIX"/bin/ld " HOST_LD_FLAGS " -o %s %s ",
#endif
            module, objfile);
      assert (error >= 0);

      if (pocl_verbose) {
        fprintf(stderr, "[pocl] executing [%s]\n", command);
        fflush(stderr);
      }
      error = system (command);
      assert (error == 0);

      /* Save space in kernel cache */
      if (!pocl_get_bool_option("POCL_LEAVE_KERNEL_COMPILER_TEMP_FILES", 0))
        {
          pocl_remove_file(objfile);
          pocl_remove_file(bytecode);
        }
    }
  return module;
}

/**
 * Populates the device specific image data structure used by kernel
 * from given kernel image argument
 */
void fill_dev_image_t (dev_image_t* di, struct pocl_argument* parg, 
                       cl_device_id device)
{
  cl_mem mem = *(cl_mem *)parg->value;
  di->is_array = mem->type == CL_MEM_OBJECT_IMAGE1D_ARRAY ||
                 mem->type == CL_MEM_OBJECT_IMAGE2D_ARRAY;
  di->data = (mem->device_ptrs[device->dev_id].mem_ptr);
  di->width = mem->image_width;
  di->height = mem->image_height;
  di->depth = mem->image_depth;
  di->image_array_size = mem->image_array_size;
  di->row_pitch = mem->image_row_pitch;
  di->slice_pitch = mem->image_slice_pitch;
  di->order = mem->image_channel_order;
  di->data_type = mem->image_channel_data_type;
  di->num_channels = mem->image_channels;
  di->elem_size = mem->image_elem_size;
}

/**
 * Populates the device specific sampler data structure used by kernel
 * from given kernel sampler argument
 */
void fill_dev_sampler_t (dev_sampler_t *ds, struct pocl_argument *parg)
{
  cl_sampler_t sampler = *(cl_sampler_t *)parg->value;

  *ds = 0;
  *ds |= sampler.normalized_coords == CL_TRUE ? CLK_NORMALIZED_COORDS_TRUE :
      CLK_NORMALIZED_COORDS_FALSE;

  switch (sampler.addressing_mode) {
    case CL_ADDRESS_NONE:
      *ds |= CLK_ADDRESS_NONE; break;
    case CL_ADDRESS_CLAMP_TO_EDGE:
      *ds |= CLK_ADDRESS_CLAMP_TO_EDGE; break;
    case CL_ADDRESS_CLAMP:
      *ds |= CLK_ADDRESS_CLAMP; break;
    case CL_ADDRESS_REPEAT:
      *ds |= CLK_ADDRESS_REPEAT; break;
    case CL_ADDRESS_MIRRORED_REPEAT:
      *ds |= CLK_ADDRESS_MIRRORED_REPEAT; break;
  }

  switch (sampler.filter_mode) {
    case CL_FILTER_NEAREST:
      *ds |= CLK_FILTER_NEAREST; break;
    case CL_FILTER_LINEAR :
      *ds |= CLK_FILTER_LINEAR; break;
  }
}

void*
pocl_memalign_alloc(size_t align_width, size_t size)
{
  void *ptr;
  int status;

#ifndef POCL_ANDROID
  status = posix_memalign(&ptr, align_width, size);
  return ((status == 0)? ptr: (void*)NULL);
#else
  ptr = memalign(align_width, size);
  return ptr;
#endif
}


