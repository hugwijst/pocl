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
#include "install-paths.h"

#define COMMAND_LENGTH 2048

/**
 * Generate code from the final bitcode using the LLVM
 * tools.
 *
 * Uses an existing (cached) one, if available.
 *
 * @param tmpdir The directory of the work-group function bitcode.
 * @param return the generated relocatable ELF file. The file needs to be
 * linked again with a correct memory map.
 *
 * Original in devices/common.c
 */
const char*
rvex_llvm_codegen (const char* tmpdir, cl_kernel kernel, cl_device_id device) {

  const char* pocl_verbose_ptr =
    pocl_get_string_option("POCL_VERBOSE", (char*)NULL);
  int pocl_verbose = pocl_verbose_ptr && *pocl_verbose_ptr;

  static const char* const assembly_cmd = "rvex-elf32-as --issue 8 --config 3333337B --borrow 1.0.3.2.5.4.7.6 --defsym WG_FUNCTION=%s_workgroup -o %s %s";

  char command[COMMAND_LENGTH];
  char bytecode[POCL_FILENAME_LENGTH];
  char objfile[POCL_FILENAME_LENGTH];
  char asmfile[POCL_FILENAME_LENGTH];
  char start_asmfile[POCL_FILENAME_LENGTH];
  char start_objfile[POCL_FILENAME_LENGTH];

  char* module = (char*) malloc(min(POCL_FILENAME_LENGTH,
	   strlen(tmpdir) + strlen(kernel->function_name) + 5)); // strlen of / .so 4+1

  int error;

  error = snprintf
    (module, POCL_FILENAME_LENGTH,
     "%s/%s.o", tmpdir, kernel->function_name);
  assert (error >= 0);

  error = snprintf
    (objfile, POCL_FILENAME_LENGTH,
     "%s/%s_wg.o", tmpdir, kernel->function_name);
  assert (error >= 0);

  error = snprintf
    (asmfile, POCL_FILENAME_LENGTH,
     "%s/%s_wg.s", tmpdir, kernel->function_name);
  assert (error >= 0);

  if (pocl_get_bool_option("POCL_BUILDING", 0)) {
    error = snprintf(start_asmfile, POCL_FILENAME_LENGTH, "%s/lib/CL/devices/rvex/_start.s",
        SRCDIR);
  } else {
    error = snprintf(start_asmfile, POCL_FILENAME_LENGTH, "%s/_start.s",
        PKGDATADIR);
  }

  error = snprintf
    (start_objfile, POCL_FILENAME_LENGTH, "%s/_start.o", tmpdir);
  assert (error >= 0);

  if (access (module, F_OK) != 0)
    {
      error = snprintf (bytecode, POCL_FILENAME_LENGTH,
                        "%s/%s", tmpdir, POCL_PARALLEL_BC_FILENAME);
      assert (error >= 0);

      if (pocl_verbose) {
        fprintf(stderr, "[pocl] generating assembly: %s -> %s\n", bytecode, asmfile);
        fflush(stderr);
      }
      error = pocl_llvm_codegen( kernel, device, bytecode, asmfile, 0);
      assert (error == 0);

      // assemble the file
      if (pocl_verbose) {
        fprintf(stderr, "[pocl] assembling files: %s, %s\n", objfile, asmfile);
        fflush(stderr);
      }
      error = snprintf (command, COMMAND_LENGTH,
            assembly_cmd, kernel->function_name, objfile, asmfile);
      assert (error >= 0);

      if (pocl_verbose) {
        fprintf(stderr, "[pocl] executing [%s]\n", command);
        fflush(stderr);
      }
      error = system (command);
      assert (error == 0);

      // assemble _start.s
      if (pocl_verbose) {
        fprintf(stderr, "[pocl] assembling files: %s, %s\n", start_objfile, start_asmfile);
        fflush(stderr);
      }
      error = snprintf (command, COMMAND_LENGTH,
            assembly_cmd, kernel->function_name, start_objfile, start_asmfile);
      assert (error >= 0);

      error = system (command);
      assert (error == 0);

      // link the object files into one object file
      error = snprintf (command, COMMAND_LENGTH,
            "rvex-elf32-ld -r -o %s %s %s",
            module, start_objfile, objfile);
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
