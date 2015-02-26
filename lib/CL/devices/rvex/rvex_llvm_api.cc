#include "rvex_llmv_api.h"

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
llvm_codegen_as (const char* tmpdir, cl_kernel kernel, cl_device_id device) {

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
      
      error = pocl_llvm_codegen( kernel, device, bytecode, objfile);
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


/* Run LLVM codegen on input file (parallel-optimized).
 *
 * Output native object file. */
int
pocl_llvm_codegen_as(cl_kernel kernel,
                  cl_device_id device,
                  const char *infilename,
                  const char *outfilename)
{
    SMDiagnostic Err;
#if defined LLVM_3_2 || defined LLVM_3_3
    std::string error;
    tool_output_file outfile(outfilename, error, 0);
#elif defined LLVM_3_4 || defined LLVM_3_5
    std::string error;
    tool_output_file outfile(outfilename, error, F_Binary);
#else
    std::error_code error;
    tool_output_file outfile(outfilename, error, F_Binary);
#endif
    llvm::Triple triple(device->llvm_target_triplet);
    llvm::TargetMachine *target = GetTargetMachine(device);
    llvm::Module *input = ParseIRFile(infilename, Err, *GlobalContext());

    llvm::PassManager PM;
    llvm::TargetLibraryInfo *TLI = new TargetLibraryInfo(triple);
    PM.add(TLI);
    if (target != NULL) {
#if defined LLVM_3_2
      PM.add(new TargetTransformInfo(target->getScalarTargetTransformInfo(),
                                     target->getVectorTargetTransformInfo()));
#else
      target->addAnalysisPasses(PM);
#endif
    }

    // TODO: get DataLayout from the 'device'
#if defined LLVM_3_2 || defined LLVM_3_3 || defined LLVM_3_4
    const DataLayout *TD = NULL;
    if (target != NULL)
      TD = target->getDataLayout();
    if (TD != NULL)
        PM.add(new DataLayout(*TD));
    else
        PM.add(new DataLayout(input));
#endif
    // TODO: better error check
    formatted_raw_ostream FOS(outfile.os());
    if(target->addPassesToEmitFile(PM, FOS, llvm::TargetMachine::CGFT_AssemblyFile))
        return 1;

    PM.run(*input);
    outfile.keep();

    return 0;
}
/* vim: set ts=4 expandtab: */
