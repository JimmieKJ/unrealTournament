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

cmake_minimum_required(VERSION 2.8)

project(libnvvm-samples)


if (DEFINED ENV{CUDA_HOME})
  set(CUDA_HOME "$ENV{CUDA_HOME}")
  set(CUDA_HOME "$ENV{CUDA_HOME}" CACHE PATH "Path to CUDA Toolkit.")
elseif (NOT DEFINED CUDA_HOME)
  # There was no CUDA_HOME explicitly given, so try to determine it from
  # locating nvcc in PATH.
  find_program(NVCC nvcc)
  if (NOT NVCC)
    message(FATAL_ERROR "CUDA_HOME not set, and could not find nvcc in "
                        "your PATH.  Please either set CUDA_HOME to a "
                        "installation path for the CUDA Toolkit or "
                        "add nvcc to your PATH.")
  endif()
  get_filename_component(CUDA_HOME "${NVCC}" PATH)
  get_filename_component(CUDA_HOME "${CUDA_HOME}/.." ABSOLUTE)
  set(CUDA_HOME "${CUDA_HOME}" CACHE PATH "Path to CUDA Toolkit.")
endif()

message(STATUS "Using CUDA_HOME: ${CUDA_HOME}")

# libNVVM
if(NOT DEFINED ENV{LIBNVVM_HOME})
  set(LIBNVVM_HOME "${CUDA_HOME}/nvvm" CACHE PATH "Path to NVVM.")
else()
  set(LIBNVVM_HOME "$ENV{LIBNVVM_HOME}" CACHE PATH "Path to NVVM.")
endif()
message(STATUS "Using LIBNVVM_HOME: ${LIBNVVM_HOME}")

# LLVM
if(DEFINED ENV{LLVM_HOME})
  set(LLVM_HOME "$ENV{LLVM_HOME}")
  set(LLVM_HOME "${LLVM_HOME}" CACHE PATH "Path to LLVM 3.2.")
else()
  set(LLVM_HOME "" CACHE PATH "Path to LLVM 3.2.")  
endif()
message(STATUS "Using LLVM_HOME: ${LLVM_HOME}")

if (CMAKE_SIZEOF_VOID_P STREQUAL "8")
  if (WIN32)
    set (CUDA_LIB_SEARCH_PATH "${CUDA_HOME}/lib/x64")
    set (NVVM_DLL_NAME nvvm64_20_0.dll)
  else ()
    set (CUDA_LIB_SEARCH_PATH "")
  endif()
else()
  if (WIN32)
    set (CUDA_LIB_SEARCH_PATH "${CUDA_HOME}/lib/Win32")
    set (NVVM_DLL_NAME nvvm32_20_0.dll)
  else()
    set (CUDA_LIB_SEARCH_PATH "")
  endif()
endif()

### Find CUDA libs/tools
find_library(CUDA_LIB cuda ${CUDA_LIB_SEARCH_PATH})
if(NOT CUDA_LIB)
  message(FATAL_ERROR "Unable to locate CUDA library/framework")
endif()
find_file(CUDA_H cuda.h PATHS "${CUDA_HOME}" "${CUDA_HOME}/include" /usr/local/cuda/include)
if(CUDA_H)
  get_filename_component(CUDA_INCLUDE_DIR ${CUDA_H} PATH)
  include_directories(${CUDA_INCLUDE_DIR})
else()
  message(FATAL_ERROR "Unable to locate cuda.h")
endif()
message(STATUS "Using cuda header: ${CUDA_H}")

# On Mac, we may also need the headers installed to /usr/local/cuda
if (APPLE)
  include_directories(/usr/local/cuda/include)
endif ()

find_program(NVCC nvcc PATHS "${CUDA_HOME}" "${CUDA_HOME}/bin")
if (NOT NVCC)
  message(FATAL_ERROR "Cannot find nvcc.  Please set CUDA_HOME to a CUDA Toolkit directory")
endif()
message(STATUS "Using nvcc: ${NVCC}")


### Find libNVVM
# The directory structure for nvvm is a bit complex.
# On Windows:
#   32-bit -- nvvm/lib/Win32
#   64-bit -- nvvm/lib/x64
# On Linux:
#   32-bit -- nvvm/lib
#   64-bit -- nvvm/lib64
# On Mac:
#   Universal -- nvvm/lib
if (CMAKE_SIZEOF_VOID_P STREQUAL "8")
  if (WIN32)
    set (LIB_ARCH_SUFFIX "/x64")
  elseif (APPLE)
    set (LIB_ARCH_SUFFIX "")
  else ()
    set (LIB_ARCH_SUFFIX "64")
  endif()
else()
  if (WIN32)
    set (LIB_ARCH_SUFFIX "/Win32")
  else()
    set (LIB_ARCH_SUFFIX "")
  endif()
endif()
find_library(NVVM_LIB nvvm PATHS "${LIBNVVM_HOME}/lib${LIB_ARCH_SUFFIX}")
find_file(NVVM_H nvvm.h PATHS "${LIBNVVM_HOME}/include")
if(NVVM_LIB AND NVVM_H)
  get_filename_component(NVVM_INCLUDE_DIR ${NVVM_H} PATH)
else()
  message(FATAL_ERROR "Unable to find libNVVM")
endif()
include_directories(${NVVM_INCLUDE_DIR})
message(STATUS "Using libnvvm header path: ${NVVM_INCLUDE_DIR}")
message(STATUS "Using libnvvm library: ${NVVM_LIB}")
set(LIBNVVM_RPATH "${LIBNVVM_HOME}/lib${LIB_ARCH_SUFFIX}")
set(CMAKE_INSTALL_RPATH ${LIBNVVM_RPATH})

# On Windows, locate nvvm.dll so we can install it
if (WIN32)
  find_file(NVVM_DLL ${NVVM_DLL_NAME} PATHS "${LIBNVVM_HOME}/bin")
  if(NOT NVVM_DLL)
    message(FATAL_ERROR "Found nvvm .h/.lib, but not .dll")
  endif()
  install(FILES ${NVVM_DLL} DESTINATION bin)
endif (WIN32)

add_definitions(-DLIBDEVICE_MAJOR_VERSION=1)
add_definitions(-DLIBDEVICE_MINOR_VERSION=0)

### Find LLVM
# LLVM is a bit tricky if we don't assume a CMake-based build
find_file(ADDLLVM_CMAKE AddLLVM.cmake PATHS "${LLVM_HOME}/share/llvm/cmake")
if(ADDLLVM_CMAKE)
  list(APPEND CMAKE_MODULE_PATH "${LLVM_HOME}/share/llvm/cmake")
  include(AddLLVM)
  include("${LLVM_HOME}/share/llvm/cmake/LLVMConfig.cmake")
  include(HandleLLVMOptions)
  include_directories("${LLVM_HOME}/include")
  link_directories("${LLVM_HOME}/lib")
  if (NOT LLVM_VERSION_MAJOR STREQUAL "3" OR NOT LLVM_VERSION_MINOR STREQUAL "2")
    message(STATUS "LLVM found, but version 3.2 is required.")
    message(STATUS "cuda-c-linking sample will not be built!")
  else()
    set(HAVE_LLVM YES)
    message(STATUS "Using LLVM install from ${LLVM_HOME}")
  endif()
else()
  message(STATUS "CMake-based LLVM install could not be found.")
  message(STATUS "cuda-c-linking sample will not be built!")
endif()


### Include common headers
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/common/include")


### Samples
if(HAVE_LLVM)
  add_subdirectory(cuda-c-linking)
endif()
add_subdirectory(ptxgen)
add_subdirectory(simple)
