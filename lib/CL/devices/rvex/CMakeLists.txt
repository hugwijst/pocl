#=============================================================================
#   CMake build system files
#
#   Copyright (c) 2014 pocl developers
#
#   Permission is hereby granted, free of charge, to any person obtaining a copy
#   of this software and associated documentation files (the "Software"), to deal
#   in the Software without restriction, including without limitation the rights
#   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
#   copies of the Software, and to permit persons to whom the Software is
#   furnished to do so, subject to the following conditions:
#
#   The above copyright notice and this permission notice shall be included in
#   all copies or substantial portions of the Software.
#
#   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
#   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
#   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
#   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
#   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
#   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
#   THE SOFTWARE.
#
#=============================================================================

# set llvm C++ flags on dev_interface.cc
set_source_files_properties("dev_interface.cc" PROPERTIES COMPILE_FLAGS "-std=c++11")


if(MSVC)
  set_source_files_properties( rvex.h rvex.c rvex_compile.h rvex_compile.c dev_interface.h dev_interface.cc PROPERTIES LANGUAGE CXX )
endif(MSVC)
add_library("pocl-devices-rvex" OBJECT rvex.h rvex.c rvex_compile.h rvex_compile.c dev_interface.h dev_interface.cc)

install(FILES _start.s DESTINATION "${POCL_INSTALL_PRIVATE_DATADIR}")
