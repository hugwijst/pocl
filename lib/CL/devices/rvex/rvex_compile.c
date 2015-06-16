#include "rvex_compile.h"

#include <string.h>

#ifndef _MSC_VER
#  include <unistd.h>
#else
#  include "vccompat.hpp"
#endif

#include "pocl_llvm.h"
#include "pocl_runtime_config.h"
#include "pocl_util.h"

#define COMMAND_LENGTH 2048

/**
 * Generate code from the final bitcode using the LLVM
 * tools.
 *
 * Uses an existing (cached) one, if available.
 *
 * @param tmpdir The directory of the work-group function bitcode.
 * @param return the generated binary filename.
 *
 * Original in devices/common.c
 */
const char*
rvex_llvm_codegen (const char* tmpdir, cl_kernel kernel, cl_device_id device) {

  const char* pocl_verbose_ptr =
    pocl_get_string_option("POCL_VERBOSE", (char*)NULL);
  int pocl_verbose = pocl_verbose_ptr && *pocl_verbose_ptr;

  char command[COMMAND_LENGTH];
  char bytecode[POCL_FILENAME_LENGTH];
  char objfile[POCL_FILENAME_LENGTH];
  char asmfile[POCL_FILENAME_LENGTH];

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

  error = snprintf
    (asmfile, POCL_FILENAME_LENGTH,
     "%s/%s.so.s", tmpdir, kernel->function_name);
  assert (error >= 0);

  if (access (module, F_OK) != 0)
    {
      error = snprintf (bytecode, POCL_FILENAME_LENGTH,
                        "%s/%s", tmpdir, POCL_PARALLEL_BC_FILENAME);
      assert (error >= 0);

      error = pocl_llvm_codegen( kernel, device, bytecode, asmfile, 0);
      assert (error == 0);

      // assemble the file
      error = snprintf (command, COMMAND_LENGTH,
            "rvex-elf32-as --issue 4 --config 337B --borrow 1.0.3,0.2,1 -o %s %s",
            objfile, asmfile);
      assert (error >= 0);

      if (pocl_verbose) {
        fprintf(stderr, "[pocl] executing [%s]\n", command);
        fflush(stderr);
      }
      error = system (command);
      assert (error == 0);

      // clang is used as the linker driver in LINK_CMD
      error = snprintf (command, COMMAND_LENGTH,
            "ld-elf2flt.sh -elf2flt -v -o %s %s",
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
          pocl_remove_file(asmfile);
          pocl_remove_file(bytecode);
        }
    }
  return module;
}

/* vim: set ts=4 expandtab: */
