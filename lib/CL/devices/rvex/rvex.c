/* rvex.c - a pocl device driver layer implementation for the rVEX softcore

   Copyright (c) 2011-2013 Universidad Rey Juan Carlos and
                 2011-2014 Pekka Jääskeläinen / Tampere University of Technology
                 2015 Hugo van der Wijst / Delft University of Technology
   
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

#include "rvex.h"

#include "bufalloc.h"
#include "common.h"
#include "cpuinfo.h"
#include "dev_interface.h"
#include "devices.h"
#include "install-paths.h"
#include "pocl_runtime_config.h"
#include "pocl_util.h"
#include "rvex_compile.h"
#include "utlist.h"

#include "topology/pocl_topology.h"

#include <endian.h>
#include <fcntl.h>
#include <unistd.h>

#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#ifndef _MSC_VER
#  include <sys/time.h>
#  include <unistd.h>
#else
#  include "vccompat.hpp"
#endif

#define max(a,b) (((a) > (b)) ? (a) : (b))

#define COMMAND_LENGTH 2048
#define WORKGROUP_STRING_LENGTH 128

struct data {
  /* Device handle */
  struct rvex_core *rvdev;
  /* Currently loaded kernel. */
  cl_kernel current_kernel;
  /* Memory allocation bookkeeping information */
  struct memory_region memory;
};

static const cl_image_format supported_image_formats[] = {
    { CL_R, CL_SNORM_INT8 },
    { CL_R, CL_SNORM_INT16 },
    { CL_R, CL_UNORM_INT8 },
    { CL_R, CL_UNORM_INT16 },
    { CL_R, CL_UNORM_SHORT_565 }, 
    { CL_R, CL_UNORM_SHORT_555 },
    { CL_R, CL_UNORM_INT_101010 }, 
    { CL_R, CL_SIGNED_INT8 },
    { CL_R, CL_SIGNED_INT16 }, 
    { CL_R, CL_SIGNED_INT32 },
    { CL_R, CL_UNSIGNED_INT8 }, 
    { CL_R, CL_UNSIGNED_INT16 },
    { CL_R, CL_UNSIGNED_INT32 }, 
    { CL_R, CL_HALF_FLOAT },
    { CL_R, CL_FLOAT },
    { CL_Rx, CL_SNORM_INT8 },
    { CL_Rx, CL_SNORM_INT16 },
    { CL_Rx, CL_UNORM_INT8 },
    { CL_Rx, CL_UNORM_INT16 },
    { CL_Rx, CL_UNORM_SHORT_565 }, 
    { CL_Rx, CL_UNORM_SHORT_555 },
    { CL_Rx, CL_UNORM_INT_101010 }, 
    { CL_Rx, CL_SIGNED_INT8 },
    { CL_Rx, CL_SIGNED_INT16 }, 
    { CL_Rx, CL_SIGNED_INT32 },
    { CL_Rx, CL_UNSIGNED_INT8 }, 
    { CL_Rx, CL_UNSIGNED_INT16 },
    { CL_Rx, CL_UNSIGNED_INT32 }, 
    { CL_Rx, CL_HALF_FLOAT },
    { CL_Rx, CL_FLOAT },
    { CL_A, CL_SNORM_INT8 },
    { CL_A, CL_SNORM_INT16 },
    { CL_A, CL_UNORM_INT8 },
    { CL_A, CL_UNORM_INT16 },
    { CL_A, CL_UNORM_SHORT_565 }, 
    { CL_A, CL_UNORM_SHORT_555 },
    { CL_A, CL_UNORM_INT_101010 }, 
    { CL_A, CL_SIGNED_INT8 },
    { CL_A, CL_SIGNED_INT16 }, 
    { CL_A, CL_SIGNED_INT32 },
    { CL_A, CL_UNSIGNED_INT8 }, 
    { CL_A, CL_UNSIGNED_INT16 },
    { CL_A, CL_UNSIGNED_INT32 }, 
    { CL_A, CL_HALF_FLOAT },
    { CL_A, CL_FLOAT },
    { CL_RG, CL_SNORM_INT8 },
    { CL_RG, CL_SNORM_INT16 },
    { CL_RG, CL_UNORM_INT8 },
    { CL_RG, CL_UNORM_INT16 },
    { CL_RG, CL_UNORM_SHORT_565 }, 
    { CL_RG, CL_UNORM_SHORT_555 },
    { CL_RG, CL_UNORM_INT_101010 }, 
    { CL_RG, CL_SIGNED_INT8 },
    { CL_RG, CL_SIGNED_INT16 }, 
    { CL_RG, CL_SIGNED_INT32 },
    { CL_RG, CL_UNSIGNED_INT8 }, 
    { CL_RG, CL_UNSIGNED_INT16 },
    { CL_RG, CL_UNSIGNED_INT32 }, 
    { CL_RG, CL_HALF_FLOAT },
    { CL_RG, CL_FLOAT },
    { CL_RGx, CL_SNORM_INT8 },
    { CL_RGx, CL_SNORM_INT16 },
    { CL_RGx, CL_UNORM_INT8 },
    { CL_RGx, CL_UNORM_INT16 },
    { CL_RGx, CL_UNORM_SHORT_565 }, 
    { CL_RGx, CL_UNORM_SHORT_555 },
    { CL_RGx, CL_UNORM_INT_101010 }, 
    { CL_RGx, CL_SIGNED_INT8 },
    { CL_RGx, CL_SIGNED_INT16 }, 
    { CL_RGx, CL_SIGNED_INT32 },
    { CL_RGx, CL_UNSIGNED_INT8 }, 
    { CL_RGx, CL_UNSIGNED_INT16 },
    { CL_RGx, CL_UNSIGNED_INT32 }, 
    { CL_RGx, CL_HALF_FLOAT },
    { CL_RGx, CL_FLOAT },
    { CL_RA, CL_SNORM_INT8 },
    { CL_RA, CL_SNORM_INT16 },
    { CL_RA, CL_UNORM_INT8 },
    { CL_RA, CL_UNORM_INT16 },
    { CL_RA, CL_UNORM_SHORT_565 }, 
    { CL_RA, CL_UNORM_SHORT_555 },
    { CL_RA, CL_UNORM_INT_101010 }, 
    { CL_RA, CL_SIGNED_INT8 },
    { CL_RA, CL_SIGNED_INT16 }, 
    { CL_RA, CL_SIGNED_INT32 },
    { CL_RA, CL_UNSIGNED_INT8 }, 
    { CL_RA, CL_UNSIGNED_INT16 },
    { CL_RA, CL_UNSIGNED_INT32 }, 
    { CL_RA, CL_HALF_FLOAT },
    { CL_RA, CL_FLOAT },
    { CL_RGBA, CL_SNORM_INT8 },
    { CL_RGBA, CL_SNORM_INT16 },
    { CL_RGBA, CL_UNORM_INT8 },
    { CL_RGBA, CL_UNORM_INT16 },
    { CL_RGBA, CL_UNORM_SHORT_565 }, 
    { CL_RGBA, CL_UNORM_SHORT_555 },
    { CL_RGBA, CL_UNORM_INT_101010 }, 
    { CL_RGBA, CL_SIGNED_INT8 },
    { CL_RGBA, CL_SIGNED_INT16 }, 
    { CL_RGBA, CL_SIGNED_INT32 },
    { CL_RGBA, CL_UNSIGNED_INT8 }, 
    { CL_RGBA, CL_UNSIGNED_INT16 },
    { CL_RGBA, CL_UNSIGNED_INT32 }, 
    { CL_RGBA, CL_HALF_FLOAT },
    { CL_RGBA, CL_FLOAT },
    { CL_INTENSITY, CL_UNORM_INT8 }, 
    { CL_INTENSITY, CL_UNORM_INT16 }, 
    { CL_INTENSITY, CL_SNORM_INT8 }, 
    { CL_INTENSITY, CL_SNORM_INT16 }, 
    { CL_INTENSITY, CL_HALF_FLOAT }, 
    { CL_INTENSITY, CL_FLOAT },
    { CL_LUMINANCE, CL_UNORM_INT8 }, 
    { CL_LUMINANCE, CL_UNORM_INT16 }, 
    { CL_LUMINANCE, CL_SNORM_INT8 }, 
    { CL_LUMINANCE, CL_SNORM_INT16 }, 
    { CL_LUMINANCE, CL_HALF_FLOAT }, 
    { CL_LUMINANCE, CL_FLOAT },
    { CL_RGB, CL_UNORM_SHORT_565 }, 
    { CL_RGB, CL_UNORM_SHORT_555 },
    { CL_RGB, CL_UNORM_INT_101010 }, 
    { CL_RGBx, CL_UNORM_SHORT_565 }, 
    { CL_RGBx, CL_UNORM_SHORT_555 },
    { CL_RGBx, CL_UNORM_INT_101010 }, 
    { CL_ARGB, CL_SNORM_INT8 },
    { CL_ARGB, CL_UNORM_INT8 },
    { CL_ARGB, CL_SIGNED_INT8 },
    { CL_ARGB, CL_UNSIGNED_INT8 }, 
    { CL_BGRA, CL_SNORM_INT8 },
    { CL_BGRA, CL_UNORM_INT8 },
    { CL_BGRA, CL_SIGNED_INT8 },
    { CL_BGRA, CL_UNSIGNED_INT8 }
 };


void
pocl_rvex_init_device_ops(struct pocl_device_ops *ops)
{
  ops->device_name = "rvex";

  ops->init_device_infos = pocl_rvex_init_device_infos;
  ops->init_build = pocl_rvex_init_build;
  ops->probe = pocl_rvex_probe;
  ops->uninit = pocl_rvex_uninit;
  ops->init = pocl_rvex_init;
  ops->alloc_mem_obj = pocl_rvex_alloc_mem_obj;
  ops->free = pocl_rvex_free;
  ops->read = pocl_rvex_read;
  ops->read_rect = pocl_rvex_read_rect;
  ops->write = pocl_rvex_write;
  ops->write_rect = pocl_rvex_write_rect;
  ops->copy = pocl_rvex_copy;
  ops->copy_rect = pocl_rvex_copy_rect;
  ops->fill_rect = pocl_rvex_fill_rect;
  ops->map_mem = pocl_rvex_map_mem;
  ops->compile_submitted_kernels = pocl_rvex_compile_submitted_kernels;
  ops->run = pocl_rvex_run;
  ops->run_native = pocl_rvex_run_native;
  ops->get_timer_value = pocl_rvex_get_timer_value;
  ops->get_supported_image_formats = pocl_rvex_get_supported_image_formats;
}

void
pocl_rvex_init_device_infos(struct _cl_device_id* dev, size_t id)
{
  struct rvex_core *rvdev = get_device(id);
  char *name = rvex_dev_get_name(rvdev);
  free_device(rvdev);

  const char* dev_name = "rVEX: ";
  size_t long_name_size = strlen(name) + strlen(dev_name) + 1;
  /* We are leaking long_name, but there is nothing we can do about that */
  char* long_name = malloc(long_name_size);
  snprintf(long_name, long_name_size, "%s%s", dev_name, name);
  free(name);


  dev->type = CL_DEVICE_TYPE_ACCELERATOR;
  dev->vendor_id = 0;
  dev->max_compute_units = 4;
  dev->max_work_item_dimensions = 3;
  dev->max_work_item_sizes[0] = SIZE_MAX;
  dev->max_work_item_sizes[1] = SIZE_MAX;
  dev->max_work_item_sizes[2] = SIZE_MAX;
  /*
    The hard restriction will be the context data which is
    stored in stack that can be as small as 8K in Linux.
    Thus, there should be enough work-items alive to fill up
    the SIMD lanes times the vector units, but not more than
    that to avoid stack overflow and cache trashing.
  */
  dev->max_work_group_size = 1024*4;
  dev->preferred_wg_size_multiple = 8;
  dev->preferred_vector_width_char = POCL_DEVICES_PREFERRED_VECTOR_WIDTH_CHAR;
  dev->preferred_vector_width_short = POCL_DEVICES_PREFERRED_VECTOR_WIDTH_SHORT;
  dev->preferred_vector_width_int = POCL_DEVICES_PREFERRED_VECTOR_WIDTH_INT;
  dev->preferred_vector_width_long = POCL_DEVICES_PREFERRED_VECTOR_WIDTH_LONG;
  dev->preferred_vector_width_float = POCL_DEVICES_PREFERRED_VECTOR_WIDTH_FLOAT;
  dev->preferred_vector_width_double = POCL_DEVICES_PREFERRED_VECTOR_WIDTH_DOUBLE;
  dev->preferred_vector_width_half = POCL_DEVICES_PREFERRED_VECTOR_WIDTH_HALF;
  /* TODO: figure out what the difference between preferred and native widths are */
  dev->native_vector_width_char = POCL_DEVICES_PREFERRED_VECTOR_WIDTH_CHAR;
  dev->native_vector_width_short = POCL_DEVICES_PREFERRED_VECTOR_WIDTH_SHORT;
  dev->native_vector_width_int = POCL_DEVICES_PREFERRED_VECTOR_WIDTH_INT;
  dev->native_vector_width_long = POCL_DEVICES_PREFERRED_VECTOR_WIDTH_LONG;
  dev->native_vector_width_float = POCL_DEVICES_PREFERRED_VECTOR_WIDTH_FLOAT;
  dev->native_vector_width_double = POCL_DEVICES_PREFERRED_VECTOR_WIDTH_DOUBLE;
  dev->native_vector_width_half = POCL_DEVICES_PREFERRED_VECTOR_WIDTH_HALF;
  dev->max_clock_frequency = 0;
  dev->address_bits = 32; /* rVEX has 32 bit pointers */

  /* Use the minimum values until we get a more sensible
     upper limit from somewhere. */
  dev->max_read_image_args = dev->max_write_image_args = 128;
  dev->image2d_max_width = dev->image2d_max_height = 8192;
  dev->image3d_max_width = dev->image3d_max_height = dev->image3d_max_depth = 2048;
  dev->max_samplers = 16;
  dev->max_constant_args = 8;

  /* We can allocate up to the total available memory of 512MB */
  dev->image_support = CL_TRUE;
  dev->image_max_buffer_size = 0;
  dev->image_max_array_size = 0;
  dev->max_parameter_size = 1024;
  dev->min_data_type_align_size = dev->mem_base_addr_align = MAX_EXTENDED_ALIGNMENT;
  dev->half_fp_config = 0;
  dev->single_fp_config = CL_FP_ROUND_TO_NEAREST | CL_FP_INF_NAN;
  dev->double_fp_config = CL_FP_ROUND_TO_NEAREST | CL_FP_INF_NAN;
  dev->global_mem_cache_type = CL_NONE;
  dev->global_mem_cacheline_size = 0;
  dev->global_mem_cache_size = 0;
  dev->global_mem_size = 512*1024*1024;
  dev->max_mem_alloc_size = dev->global_mem_size/4;
  dev->max_constant_buffer_size = dev->global_mem_size;
  dev->local_mem_type = CL_GLOBAL;
  dev->local_mem_size = dev->global_mem_size;
  dev->error_correction_support = CL_FALSE;
  dev->host_unified_memory = CL_FALSE;
  dev->profiling_timer_resolution = 0;
  dev->endian_little = CL_FALSE;
  dev->available = CL_TRUE;
  dev->compiler_available = CL_TRUE;
  dev->execution_capabilities = CL_EXEC_KERNEL | CL_EXEC_NATIVE_KERNEL;
  dev->queue_properties = CL_QUEUE_PROFILING_ENABLE;
  dev->platform = 0;
  dev->device_partition_properties[0] = 0;
  dev->printf_buffer_size = 0;
  dev->vendor = "TU_Delft";
  dev->profile = "FULL_PROFILE";
  dev->long_name = long_name;
  dev->extensions = "";
  dev->llvm_target_triplet = "rvex";
  dev->llvm_cpu = "rvex-vliw";
  dev->use_pic = CL_FALSE;
  dev->has_64bit_long = 0;
}

char *pocl_rvex_init_build(void *data, const char *tmpdir)
{
  static const char *conf_str = "-backend-option -config=%s%s/rvex_W8";
  static const char *base_dir;
  static const char *path;
  int len;

  if (pocl_get_bool_option("POCL_BUILDING", 0)) {
    base_dir = SRCDIR;
    path = "/lib/kernel/rvex";
  } else {
    base_dir = PKGDATADIR;
    path = "";
  }

  len = snprintf(NULL, 0, conf_str, base_dir, path);

  char *ret = malloc(len+1);
  snprintf(ret, len+1, conf_str, base_dir, path);

  return ret;
}

unsigned int
pocl_rvex_probe(struct pocl_device_ops *ops)
{
  size_t count;
  int env_count = pocl_device_get_env_count(ops->device_name);

  if (env_count == 0) {
    // Devices specified using env_count and rvex not specified
    // Don't search for devices
    return 0;
  }
  get_devices(&count, NULL);

  return count;
}

void
pocl_rvex_init (cl_device_id device, size_t id, const char* parameters)
{
  struct data *d;
  static int global_mem_id;
  static int first_rvex_init = 1;
  
  if (first_rvex_init)
    {
      first_rvex_init = 0;
      global_mem_id = device->dev_id;
    }
  device->global_mem_id = global_mem_id;

  d = (struct data *) calloc (1, sizeof (struct data));
  
  d->rvdev = get_device(id);
  d->current_kernel = NULL;
  // The ML605 has 512 MBytes of RAM, starting at address 0
  init_mem_region(&d->memory, 0, 512*1024*1024);
  device->data = d;

  /* The rvex driver represents only one "compute unit" as
     it doesn't exploit multiple hardware threads. Multiple
     rvex devices can be still used for task level parallelism 
     using multiple OpenCL devices. */
  device->max_compute_units = 1;
}

cl_int
pocl_rvex_alloc_mem_obj (cl_device_id device, cl_mem mem_obj)
{
  void *b = NULL;
  struct data* data = (struct data*)device->data;
  cl_int flags = mem_obj->flags;
  pocl_mem_identifier *dev_ptrs = &mem_obj->device_ptrs[device->global_mem_id];

  assert(! (flags & CL_MEM_USE_HOST_PTR && flags & CL_MEM_COPY_HOST_PTR)
      && "USE_HOST_PTR and COPY_HOST_PTR are both set");

  assert(! (mem_obj->mem_host_ptr == NULL
      && (flags & CL_MEM_USE_HOST_PTR || flags & CL_MEM_COPY_HOST_PTR))
      && "mem_host_ptr should not be NULL");

  /* if memory for this global memory is not yet allocated -> do it */
  if (dev_ptrs->mem_ptr == NULL) {
    chunk_info_t *chunk = alloc_buffer(&data->memory, mem_obj->size);
    if(chunk == NULL) {
      return CL_MEM_OBJECT_ALLOCATION_FAILURE;
    }

    dev_ptrs->mem_ptr = chunk;
    dev_ptrs->global_mem_id = device->global_mem_id;

    if (flags & CL_MEM_USE_HOST_PTR || flags & CL_MEM_COPY_HOST_PTR) {
      pocl_rvex_write(data, mem_obj->mem_host_ptr, chunk,
          0, mem_obj->size);
    }
  }
  /* copy already allocated global mem info to devices own slot */
  mem_obj->device_ptrs[device->dev_id] = 
    mem_obj->device_ptrs[device->global_mem_id];
    
  return CL_SUCCESS;
}

void
pocl_rvex_free (void *data, cl_mem_flags flags, void *ptr)
{
  /* CL_MEM_USE_HOST_PTR objects have their data copied to them after execution
     of the kernel */
  free_chunk((chunk_info_t *)ptr);
}

void
pocl_rvex_read (void *data, void *host_ptr, const void *device_ptr,
    size_t offset, size_t cb)
{
  struct data *d = (struct data*) data;
  chunk_info_t *chunk = (chunk_info_t*)device_ptr;
  size_t bytes_left = cb;
  struct timespec tw1, tw2;

  if (pocl_get_bool_option("POCL_VERBOSE", 0)) {
    fprintf(stderr, "[pocl] reading %zu bytes from 0x%zx+0x%zx to %p\n", cb,
        chunk->start_address, offset, host_ptr);
    fflush(stderr);

    clock_gettime(CLOCK_MONOTONIC, &tw1);
  }

  int f = open(rvex_dev_get_memfile(d->rvdev), O_RDONLY);
  if (f < 0) {
    fprintf(stderr, "Couldn't open %s for reading!\n", rvex_dev_get_memfile(d->rvdev));
    return;
  }

  off_t newoff = lseek(f, chunk->start_address + offset, SEEK_SET);
  if (newoff == (off_t)-1) {
    close(f);
    return;
  }

  while (bytes_left > 0) {
    ssize_t read_bytes = read(f, host_ptr, bytes_left);

    if (read_bytes < 0) {
      fprintf(stderr, "Reading from device failed (addr: 0x%zx, size: %zu)!\n",
          chunk->start_address + offset, bytes_left);
      close(f);
      return;
    }

    bytes_left -= read_bytes;
    host_ptr += read_bytes;
    offset += read_bytes;
  }
  close(f);

  if (pocl_get_bool_option("POCL_VERBOSE", 0)) {
    struct tw2;
    double walltime;

    clock_gettime(CLOCK_MONOTONIC, &tw2);
    walltime = 1000.0*tw2.tv_sec + 1e-6*tw2.tv_nsec -
      (1000.0*tw1.tv_sec + 1e-6*tw1.tv_nsec);

    printf("[pocl] Reading %zu bytes took %.2f ms, speed: %.2f MB/s\n", cb,
        walltime, cb / walltime / 1000);
  }
}

void
pocl_rvex_write (void *data, const void *host_ptr, void *device_ptr,
    size_t offset, const size_t cb)
{
  struct data *d = (struct data*) data;
  chunk_info_t *chunk = (chunk_info_t*)device_ptr;
  size_t bytes_left = cb;
  struct timespec tw1, tw2;

  if (pocl_get_bool_option("POCL_VERBOSE", 0)) {
    fprintf(stderr, "[pocl] writing %zu bytes to 0x%zx+0x%zx\n", cb,
        chunk->start_address, offset);
    fflush(stderr);
    uint32_t const *w = host_ptr;

    clock_gettime(CLOCK_MONOTONIC, &tw1);
  }

  int f = open(rvex_dev_get_memfile(d->rvdev), O_WRONLY);
  if (f < 0) {
    fprintf(stderr, "Couldn't open %s for writing!\n", rvex_dev_get_memfile(d->rvdev));
    return;
  }

  off_t newoff = lseek(f, chunk->start_address + offset, SEEK_SET);
  if (newoff == (off_t)-1) {
    close(f);
    return;
  }

  while (bytes_left > 0) {
    ssize_t written_bytes = write(f, host_ptr, bytes_left);

    if (written_bytes < 0) {
      fprintf(stderr, "Writing to device failed (addr: 0x%zx, size: %zu)!\n",
          chunk->start_address + offset, bytes_left);
      close(f);
      return;
    }

    bytes_left -= written_bytes;
    host_ptr += written_bytes;
    offset += written_bytes;
  }
  close(f);

  if (pocl_get_bool_option("POCL_VERBOSE", 0)) {
    struct tw2;
    double walltime;

    clock_gettime(CLOCK_MONOTONIC, &tw2);
    walltime = 1000.0*tw2.tv_sec + 1e-6*tw2.tv_nsec -
      (1000.0*tw1.tv_sec + 1e-6*tw1.tv_nsec);

    printf("[pocl] Writing %zu bytes took %.2f ms, speed: %.2f MB/s\n", cb,
        walltime, cb / walltime / 1000);
  }
}

/* The rVEX is a 32-bit big-endian architecture. Define our own pocl-context
   so it also works on 64-bit architectures. */
struct rvex_pocl_context {
  uint32_t work_dim;
  uint32_t num_groups[3];
  uint32_t group_id[3];
  uint32_t global_offset[3];
};

static inline size_t align_size(size_t s, size_t psize) {
  return (s + psize - 1) / psize * psize;
}

void
pocl_rvex_run 
(void *data, 
 _cl_command_node* cmd)
{
  struct data *d;
  size_t x, y, z;
  unsigned i;
  cl_kernel kernel = cmd->command.run.kernel;
  struct pocl_context *pc = &cmd->command.run.pc;
  struct rvex_pocl_context rvex_pc;
  /* rVEX has 4 byte pointers */
  const size_t psize = 4;

  assert (data != NULL);
  d = (struct data *) data;

  d->current_kernel = kernel;

  /* Determine the amount of memory needed for all argument data */
  size_t alloc_size = 0;
  /* The initial argument pointers */
  size_t arg_list_size = psize * (kernel->num_args + kernel->num_locals);
  alloc_size += arg_list_size;
  /* Data needed for kernel arguments */
  for (i = 0; i < kernel->num_args; ++i) {
    struct pocl_argument *al = &(cmd->command.run.arguments[i]);
    if (kernel->arg_info[i].is_local) {
      alloc_size += align_size(al->size, psize);
    } else {
      switch (kernel->arg_info[i].type) {
        case POCL_ARG_TYPE_POINTER:
          /* The buffer should either be local and handled above, or should
             already be transfered. */
          break;
        case POCL_ARG_TYPE_IMAGE:
          alloc_size += align_size(sizeof(dev_image_t), psize);
          break;
        case POCL_ARG_TYPE_SAMPLER:
          alloc_size += align_size(sizeof(dev_sampler_t), psize);
          break;
        case POCL_ARG_TYPE_NONE:
          alloc_size += align_size(al->size, psize);
          break;
      }
    }
  }
  /* Data needed for non-argument locals */
  for (i = 0; i < kernel->num_locals; ++i) {
    struct pocl_argument *al = &(cmd->command.run.arguments[i]);

    alloc_size += align_size(al->size, psize);
  }

  /* Allocate total argument data memory */
  uint8_t *arguments = malloc(alloc_size);
  chunk_info_t *args_alloc = alloc_buffer(&d->memory, alloc_size);

  /* allocate pocl context (second argument to wg) */
  chunk_info_t *ctxt_alloc = alloc_buffer(&d->memory, sizeof(rvex_pc));

  /* The arguments array is located at the beginning of the arg_data array */
  uint32_t *arg_list = (uint32_t*)&arguments[0];
  uint8_t *arg_data = &arguments[arg_list_size];
  uint32_t p_arg_data_dev = args_alloc->start_address + arg_list_size;

  /* Process the kernel arguments. Convert the opaque buffer
     pointers to real device pointers, allocate dynamic local 
     memory buffers, etc. */
  for (i = 0; i < kernel->num_args; ++i) {
    struct pocl_argument *al = &(cmd->command.run.arguments[i]);

    if (kernel->arg_info[i].is_local) {
      arg_list[i] = htobe32(p_arg_data_dev);
      p_arg_data_dev += align_size(al->size, psize);

    } else if (kernel->arg_info[i].type == POCL_ARG_TYPE_POINTER) {
      /* It's legal to pass a NULL pointer to clSetKernelArguments. In
         that case we must pass the same NULL forward to the kernel.
         Otherwise, the user must have created a buffer with per device
         pointers stored in the cl_mem. */
      if (al->value == NULL)
        arg_list[i] = 0;
      else {
        cl_mem_t *mem = *(cl_mem_t **) al->value;
        /* rVEX addresses are 32-bit */
        chunk_info_t *chunk = (chunk_info_t*) mem->device_ptrs[cmd->device->dev_id].mem_ptr;
        arg_list[i] = htobe32((uint32_t)chunk->start_address);
      }

    } else if (kernel->arg_info[i].type == POCL_ARG_TYPE_IMAGE) {
      const size_t SIZE = sizeof(dev_image_t);
      dev_image_t di;
      fill_dev_image_t (&di, al, cmd->device);

      memcpy(arg_data, &di, SIZE);
      arg_list[i] = p_arg_data_dev;

      arg_data += align_size(SIZE, psize);
      p_arg_data_dev += align_size(SIZE, psize);

    } else if (kernel->arg_info[i].type == POCL_ARG_TYPE_SAMPLER) {
      const size_t SIZE = sizeof(dev_sampler_t);
      dev_sampler_t ds;
      fill_dev_sampler_t (&ds, al);

      memcpy(arg_data, &ds, SIZE);
      arg_list[i] = p_arg_data_dev;

      arg_data += align_size(SIZE, psize);
      p_arg_data_dev += align_size(SIZE, psize);

    } else {
      memcpy(arg_data, al->value, al->size);
      arg_list[i] = p_arg_data_dev;

      arg_data += align_size(al->size, psize);
      p_arg_data_dev += align_size(al->size, psize);
    }
  }
  for (i = kernel->num_args; i < kernel->num_args + kernel->num_locals; ++i) {
    struct pocl_argument *al = &(cmd->command.run.arguments[i]);

    arg_list[i] = p_arg_data_dev;
    p_arg_data_dev += align_size(al->size, psize);
  }

  /* Write argument data to rvex */
  pocl_rvex_write(data, arguments, args_alloc, 0, alloc_size);

  /* Reserve space for program and write program */
  /* TODO: determine memory size from elf file */
  chunk_info_t *prog_alloc = alloc_buffer(&d->memory, 1*1024*1024);
  {
    char *binbuffer;

    const char *binfile = rvex_link (cmd->command.run.tmp_dir,
        (const char*)cmd->command.run.wg, prog_alloc->start_address,
        cmd->command.run.kernel);
    size_t fsize = pocl_read_binary_file(binfile, (void**)&binbuffer);
    assert(fsize > 0);

    /* Write pointers to first and second argument of workgroup to data */
    *(uint32_t*)&binbuffer[0] = htobe32(args_alloc->start_address);
    *(uint32_t*)&binbuffer[4] = htobe32(ctxt_alloc->start_address);

    pocl_rvex_write(data, binbuffer, prog_alloc, 0, fsize);

    free(binbuffer);

    if (!pocl_get_bool_option("POCL_LEAVE_KERNEL_COMPILER_TEMP_FILES", 0)) {
      pocl_remove_file(binfile);
    }
    free((void*)binfile);
  }

  for (z = 0; z < pc->num_groups[2]; ++z)
    {
      for (y = 0; y < pc->num_groups[1]; ++y)
        {
          for (x = 0; x < pc->num_groups[0]; ++x)
            {
              pc->group_id[0] = x;
              pc->group_id[1] = y;
              pc->group_id[2] = z;

              rvex_pc.work_dim =  htobe32(pc->work_dim);
              for (int i = 0; i < 3; i++) {
                  rvex_pc.num_groups   [i] = htobe32(pc->num_groups   [i]);
                  rvex_pc.group_id     [i] = htobe32(pc->group_id     [i]);
                  rvex_pc.global_offset[i] = htobe32(pc->global_offset[i]);
              }

              pocl_rvex_write(data, &rvex_pc, ctxt_alloc, 0,
                  sizeof(rvex_pc));

              rvex_dev_set_run(d->rvdev, false);
              rvex_dev_set_reset_vector(d->rvdev, prog_alloc->start_address + 8);
              rvex_dev_set_reset(d->rvdev, true);
              rvex_dev_set_reset(d->rvdev, false);
              rvex_dev_set_run(d->rvdev, true);

              while(!rvex_dev_get_done(d->rvdev)) {
                nanosleep(&(struct timespec){.tv_nsec=1000000}, NULL);
              }
            }
        }
    }
  /* Copy CL_MEM_USE_HOST_PTR argument data back to the host */
  for (i = 0; i < kernel->num_args; ++i) {
    if (kernel->arg_info[i].type == POCL_ARG_TYPE_POINTER) {
      struct pocl_argument *al = &(cmd->command.run.arguments[i]);
      /* It is legal to pass NULL */
      if (al->value != NULL) {
        cl_mem_t *mem = *(cl_mem_t **) al->value;

        if(mem->flags & CL_MEM_USE_HOST_PTR) {
          chunk_info_t* chunk = (chunk_info_t*)mem->device_ptrs[cmd->device->dev_id].mem_ptr;
          pocl_rvex_read(data, mem->mem_host_ptr, chunk, 0, mem->size);
        }
      }
    }
  }

  /* Free allocated memory */
  free_chunk(prog_alloc);
  free_chunk(ctxt_alloc);
  free_chunk(args_alloc);
  free(arguments);
}

void
pocl_rvex_run_native 
(void *data, 
 _cl_command_node* cmd)
{
#if 0
  cmd->command.native.user_func(cmd->command.native.args);
#endif
}

void
pocl_rvex_copy (void *data, const void *src_ptr, size_t src_offset,
    void *__restrict__ dst_ptr, size_t dst_offset, size_t cb)
{
  /* Not yet implemented */
}

void
pocl_rvex_copy_rect (void *data,
                      const void *__restrict const src_ptr,
                      void *__restrict__ const dst_ptr,
                      const size_t *__restrict__ const src_origin,
                      const size_t *__restrict__ const dst_origin, 
                      const size_t *__restrict__ const region,
                      size_t const src_row_pitch,
                      size_t const src_slice_pitch,
                      size_t const dst_row_pitch,
                      size_t const dst_slice_pitch)
{
  /* Not yet implemented */
  char const *__restrict const adjusted_src_ptr = 
    (char const*)src_ptr +
    src_origin[0] + src_row_pitch * src_origin[1] + src_slice_pitch * src_origin[2];
  char *__restrict__ const adjusted_dst_ptr = 
    (char*)dst_ptr +
    dst_origin[0] + dst_row_pitch * dst_origin[1] + dst_slice_pitch * dst_origin[2];
  
  size_t j, k;

  /* TODO: handle overlaping regions */
  
#if 0
  for (k = 0; k < region[2]; ++k)
    for (j = 0; j < region[1]; ++j)
      memcpy (adjusted_dst_ptr + dst_row_pitch * j + dst_slice_pitch * k,
              adjusted_src_ptr + src_row_pitch * j + src_slice_pitch * k,
              region[0]);
#endif
}

void
pocl_rvex_write_rect (void *data,
                       const void *__restrict__ const host_ptr,
                       void *__restrict__ const device_ptr,
                       const size_t *__restrict__ const buffer_origin,
                       const size_t *__restrict__ const host_origin, 
                       const size_t *__restrict__ const region,
                       size_t const buffer_row_pitch,
                       size_t const buffer_slice_pitch,
                       size_t const host_row_pitch,
                       size_t const host_slice_pitch)
{
  /* Not yet implemented */
  char *__restrict const adjusted_device_ptr = 
    (char*)device_ptr +
    buffer_origin[0] + buffer_row_pitch * buffer_origin[1] + buffer_slice_pitch * buffer_origin[2];
  char const *__restrict__ const adjusted_host_ptr = 
    (char const*)host_ptr +
    host_origin[0] + host_row_pitch * host_origin[1] + host_slice_pitch * host_origin[2];
  
  size_t j, k;

  /* TODO: handle overlaping regions */
  
#if 0
  for (k = 0; k < region[2]; ++k)
    for (j = 0; j < region[1]; ++j)
      memcpy (adjusted_device_ptr + buffer_row_pitch * j + buffer_slice_pitch * k,
              adjusted_host_ptr + host_row_pitch * j + host_slice_pitch * k,
              region[0]);
#endif
}

void
pocl_rvex_read_rect (void *data,
                      void *__restrict__ const host_ptr,
                      void *__restrict__ const device_ptr,
                      const size_t *__restrict__ const buffer_origin,
                      const size_t *__restrict__ const host_origin, 
                      const size_t *__restrict__ const region,
                      size_t const buffer_row_pitch,
                      size_t const buffer_slice_pitch,
                      size_t const host_row_pitch,
                      size_t const host_slice_pitch)
{
  size_t device_const_offset =
      buffer_origin[2] * buffer_slice_pitch +
      buffer_origin[1] * buffer_row_pitch +
      buffer_origin[0];

  char *__restrict__ const adjusted_host_ptr = 
      host_ptr +
      host_origin[2] * host_slice_pitch +
      host_origin[1] * host_row_pitch +
      host_origin[0];

  size_t j, k;
  
  /* TODO: handle overlaping regions */

  for (k = 0; k < region[2]; ++k)
    if (buffer_row_pitch == region[0] && host_row_pitch == region[0]) {
      pocl_rvex_read(data,
          adjusted_host_ptr + host_slice_pitch * k,
          device_ptr,
          device_const_offset + buffer_slice_pitch * k,
          region[0] * region[1]);
    } else {
      for (j = 0; j < region[1]; ++j)
        pocl_rvex_read(data,
            adjusted_host_ptr + host_row_pitch * j + host_slice_pitch * k,
            device_ptr,
            device_const_offset + buffer_row_pitch * j + buffer_slice_pitch * k,
            region[0]);
    }
}

/* origin and region must be in original shape unlike in copy/read/write_rect()
 */
void
pocl_rvex_fill_rect (void *data,
                      void *__restrict__ const device_ptr,
                      const size_t *__restrict__ const buffer_origin,
                      const size_t *__restrict__ const region,
                      size_t const buffer_row_pitch,
                      size_t const buffer_slice_pitch,
                      void *fill_pixel,
                      size_t pixel_size)                    
{
  /* Not yet implemented */
  char *__restrict const adjusted_device_ptr = (char*)device_ptr 
    + buffer_origin[0] * pixel_size 
    + buffer_row_pitch * buffer_origin[1] 
    + buffer_slice_pitch * buffer_origin[2];
    
  size_t i, j, k;

#if 0
  for (k = 0; k < region[2]; ++k)
    for (j = 0; j < region[1]; ++j)
      for (i = 0; i < region[0]; ++i)
        memcpy (adjusted_device_ptr + pixel_size * i 
                + buffer_row_pitch * j 
                + buffer_slice_pitch * k, fill_pixel, pixel_size);
#endif
}

void *
pocl_rvex_map_mem (void *data, void *buf_ptr, 
                      size_t offset, size_t size,
                      void *host_ptr) 
{
  /* Not yet implemented */

#if 0
  /* All global pointers of the pthread/CPU device are in 
     the host address space already, and up to date. */
  if (host_ptr != NULL) return host_ptr;
  return (char*)buf_ptr + offset;
#endif
  return NULL;
}

void
pocl_rvex_uninit (cl_device_id device)
{
  struct data *d = (struct data*)device->data;
  free_device(d->rvdev);
  POCL_MEM_FREE(d);
  device->data = NULL;
}

cl_ulong
pocl_rvex_get_timer_value (void *data) 
{
  /* Just use the timer functionality of the PC. */
#ifndef _MSC_VER
  struct timeval current;
  gettimeofday(&current, NULL);  
  return (current.tv_sec * 1000000 + current.tv_usec)*1000;
#else
  FILETIME ft;
  cl_ulong tmpres = 0;
  GetSystemTimeAsFileTime(&ft);
  tmpres |= ft.dwHighDateTime;
  tmpres <<= 32;
  tmpres |= ft.dwLowDateTime;
  tmpres -= 11644473600000000Ui64;
  tmpres /= 10;
  return tmpres;
#endif
}

cl_int 
pocl_rvex_get_supported_image_formats (cl_mem_flags flags,
                                        const cl_image_format **image_formats,
                                        cl_int *num_img_formats)
{
    if (num_img_formats == NULL || image_formats == NULL)
      return CL_INVALID_VALUE;
  
    *num_img_formats = sizeof(supported_image_formats)/sizeof(cl_image_format);
    *image_formats = supported_image_formats;
    
    return CL_SUCCESS; 
}

typedef struct compiler_cache_item compiler_cache_item;
struct compiler_cache_item
{
  char *tmp_dir;
  char *function_name;
  pocl_workgroup wg;
  compiler_cache_item *next;
};

static compiler_cache_item *compiler_cache;
static pocl_lock_t compiler_cache_lock;

static void check_compiler_cache (_cl_command_node *cmd)
{
  char workgroup_string[WORKGROUP_STRING_LENGTH];
  lt_dlhandle dlhandle;
  compiler_cache_item *ci = NULL;
  
  if (compiler_cache == NULL)
    POCL_INIT_LOCK (compiler_cache_lock);

  POCL_LOCK (compiler_cache_lock);
  LL_FOREACH (compiler_cache, ci)
    {
      if (strcmp (ci->tmp_dir, cmd->command.run.tmp_dir) == 0 &&
          strcmp (ci->function_name, 
                  cmd->command.run.kernel->function_name) == 0)
        {
          POCL_UNLOCK (compiler_cache_lock);
          cmd->command.run.wg = ci->wg;
          return;
        }
    }
  ci = (compiler_cache_item*) malloc (sizeof (compiler_cache_item));
  ci->next = NULL;
  ci->tmp_dir = strdup(cmd->command.run.tmp_dir);
  ci->function_name = strdup (cmd->command.run.kernel->function_name);
  const char* module_fn = rvex_llvm_codegen (cmd->command.run.tmp_dir,
                                        cmd->command.run.kernel,
                                        cmd->device);
  cmd->command.run.wg = (void*)module_fn;
#if 0
  /* TODO: load correct workgroup info in structures */
  dlhandle = lt_dlopen (module_fn);
  if (dlhandle == NULL)
    {
      printf ("pocl error: lt_dlopen(\"%s\") failed with '%s'.\n", 
              module_fn, lt_dlerror());
      printf ("note: missing symbols in the kernel binary might be" 
              "reported as 'file not found' errors.\n");
      abort();
    }
  snprintf (workgroup_string, WORKGROUP_STRING_LENGTH,
            "_%s_workgroup", cmd->command.run.kernel->function_name);
  cmd->command.run.wg = ci->wg = 
    (pocl_workgroup) lt_dlsym (dlhandle, workgroup_string);
#endif

  LL_APPEND (compiler_cache, ci);
  POCL_UNLOCK (compiler_cache_lock);

}

void
pocl_rvex_compile_submitted_kernels (_cl_command_node *cmd)
{
  if (cmd->type == CL_COMMAND_NDRANGE_KERNEL)
    check_compiler_cache (cmd);

}
