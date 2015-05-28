################################################################################
#
# Copyright 1993-2012 NVIDIA Corporation.  All rights reserved.
#
# NOTICE TO USER:   
#
# This source code is subject to NVIDIA ownership rights under U.S. and 
# international Copyright laws.  
#
# NVIDIA MAKES NO REPRESENTATION ABOUT THE SUITABILITY OF THIS SOURCE 
# CODE FOR ANY PURPOSE.  IT IS PROVIDED "AS IS" WITHOUT EXPRESS OR 
# IMPLIED WARRANTY OF ANY KIND.  NVIDIA DISCLAIMS ALL WARRANTIES WITH 
# REGARD TO THIS SOURCE CODE, INCLUDING ALL IMPLIED WARRANTIES OF 
# MERCHANTABILITY, NONINFRINGEMENT, AND FITNESS FOR A PARTICULAR PURPOSE.   
# IN NO EVENT SHALL NVIDIA BE LIABLE FOR ANY SPECIAL, INDIRECT, INCIDENTAL, 
# OR CONSEQUENTIAL DAMAGES, OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS 
# OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE 
# OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE 
# OR PERFORMANCE OF THIS SOURCE CODE.  
#
# U.S. Government End Users.  This source code is a "commercial item" as 
# that term is defined at 48 C.F.R. 2.101 (OCT 1995), consisting  of 
# "commercial computer software" and "commercial computer software 
# documentation" as such terms are used in 48 C.F.R. 12.212 (SEPT 1995) 
# and is provided to the U.S. Government only as a commercial end item.  
# Consistent with 48 C.F.R.12.212 and 48 C.F.R. 227.7202-1 through 
# 227.7202-4 (JUNE 1995), all U.S. Government End Users acquire the 
# source code with only those rights set forth herein.
#
################################################################################


set(LLVM_LINK_COMPONENTS Core Support)

add_llvm_executable(cuda-c-linking cuda-c-linking.cpp)

target_link_libraries(cuda-c-linking ${NVVM_LIB} ${CUDA_LIB})

if (APPLE OR UNIX)
  set_target_properties(cuda-c-linking PROPERTIES
    LINK_FLAGS "-Wl,-rpath,${LIBNVVM_RPATH}")
endif()


##############################
### Math Lib
##############################

### The math library is built as a collection of several variants.  We compile
### one version each for {compute_20, compute_30, compute_35} X {32-bit/64-bit}.
### The results are bundled into a system library using the 'nvcc -lib' feature.

set(mathlib_sources math-funcs.cu)

#>> nvcc -m32 -gencode=compute_20,code=sm_20 ... -dc math-funcs.cu -o math-funcs32.o
add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/math-funcs32.o"
                   COMMAND ${NVCC} -m32 -gencode arch=compute_20,code=sm_20 -gencode arch=compute_30,code=sm_30 -gencode arch=compute_35,code=sm_35 -dc "${CMAKE_CURRENT_SOURCE_DIR}/math-funcs.cu" -o "${CMAKE_CURRENT_BINARY_DIR}/math-funcs32.o"
                   DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/math-funcs.cu"
                   COMMENT "Building math-funcs32.o")

#>> nvcc -m32 -lib math-funcs32.o -o libmathfuncs32.a
add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/libmathfuncs32.a"
                   COMMAND ${NVCC} -m32 -lib "${CMAKE_CURRENT_BINARY_DIR}/math-funcs32.o" -o "${CMAKE_CURRENT_BINARY_DIR}/libmathfuncs32.a"
                   DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/math-funcs32.o"
                   COMMENT "Building libmathfuncs32.a")

#>> nvcc -m64 -gencode=compute_20,code=sm_20 ... -dc math-funcs.cu -o math-funcs64.o
add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/math-funcs64.o"
                   COMMAND ${NVCC} -m64 -gencode arch=compute_20,code=sm_20 -gencode arch=compute_30,code=sm_30 -gencode arch=compute_35,code=sm_35 -dc "${CMAKE_CURRENT_SOURCE_DIR}/math-funcs.cu" -o "${CMAKE_CURRENT_BINARY_DIR}/math-funcs64.o"
                   DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/math-funcs.cu"
                   COMMENT "Building math-funcs64.o")

#>> nvcc -m64 -lib math-funcs64.o -o libmathfuncs64.a
add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/libmathfuncs64.a"
                   COMMAND ${NVCC} -m64 -lib "${CMAKE_CURRENT_BINARY_DIR}/math-funcs64.o" -o "${CMAKE_CURRENT_BINARY_DIR}/libmathfuncs64.a"
                   DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/math-funcs64.o"
                   COMMENT "Building libmathfuncs64.a")

add_custom_target(mathlib DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/libmathfuncs32.a" "${CMAKE_CURRENT_BINARY_DIR}/libmathfuncs64.a")

add_dependencies(cuda-c-linking mathlib)

install(TARGETS cuda-c-linking DESTINATION bin)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/libmathfuncs32.a"
              "${CMAKE_CURRENT_BINARY_DIR}/libmathfuncs64.a" DESTINATION bin)


