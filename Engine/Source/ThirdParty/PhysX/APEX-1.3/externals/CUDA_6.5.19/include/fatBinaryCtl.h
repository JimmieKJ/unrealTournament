
/*
 *  Copyright 2010 NVIDIA Corporation.  All rights reserved.
 *
 *  NOTICE TO USER: The source code, and related code and software
 *  ("Code"), is copyrighted under U.S. and international laws.
 *
 *  NVIDIA Corporation owns the copyright and any patents issued or
 *  pending for the Code.
 *
 *  NVIDIA CORPORATION MAKES NO REPRESENTATION ABOUT THE SUITABILITY
 *  OF THIS CODE FOR ANY PURPOSE.  IT IS PROVIDED "AS-IS" WITHOUT EXPRESS
 *  OR IMPLIED WARRANTY OF ANY KIND.  NVIDIA CORPORATION DISCLAIMS ALL
 *  WARRANTIES WITH REGARD TO THE CODE, INCLUDING NON-INFRINGEMENT, AND
 *  ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 *  PURPOSE.  IN NO EVENT SHALL NVIDIA CORPORATION BE LIABLE FOR ANY
 *  DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES
 *  WHATSOEVER ARISING OUT OF OR IN ANY WAY RELATED TO THE USE OR
 *  PERFORMANCE OF THE CODE, INCLUDING, BUT NOT LIMITED TO, INFRINGEMENT,
 *  LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 *  NEGLIGENCE OR OTHER TORTIOUS ACTION, AND WHETHER OR NOT THE
 *  POSSIBILITY OF SUCH DAMAGES WERE KNOWN OR MADE KNOWN TO NVIDIA
 *  CORPORATION.
 *
 */

#ifndef fatbinaryctl_INCLUDED
#define fatbinaryctl_INCLUDED

#ifndef __CUDA_INTERNAL_COMPILATION__
#include <stddef.h> /* for size_t */
#endif
#include "fatbinary.h"

/* 
 * These are routines for controlling the fat binary.
 * A void* object is used, with ioctl-style calls to set and get info from it.
 */

typedef enum {
  FBCTL_ERROR_NONE = 0,
  FBCTL_ERROR_NULL,           /* null pointer */
  FBCTL_ERROR_UNRECOGNIZED,   /* unrecognized kind */
  FBCTL_ERROR_NO_CANDIDATE,   /* no candidate found */
  FBCTL_ERROR_COMPILE_FAILED, /* no candidate found */
  FBCTL_ERROR_INTERNAL        /* unexpected internal error */
} fatBinaryCtlError_t;
extern char* fatBinaryCtl_Errmsg (fatBinaryCtlError_t e);

extern fatBinaryCtlError_t fatBinaryCtl_Create (void* *data);

extern void fatBinaryCtl_Delete (void* data);

/* use this control-call to set and get values */
extern fatBinaryCtlError_t fatBinaryCtl (void* data, int request, ...);

/* defined requests */
#define FBCTL_SET_BINARY        1  /* void* (e.g. fatbin, elf or ptx object) */
#define FBCTL_SET_TARGETSM      2  /* int (use values from nvelf.h) */
#define FBCTL_SET_FLAGS         3  /* longlong */
#define FBCTL_SET_CMDOPTIONS    4  /* char* */
#define FBCTL_SET_POLICY        5  /* fatBinary_CompilationPolicy */
/* get calls return value in arg, thus are all by reference */
#define FBCTL_GET_CANDIDATE     10 /* void** binary, 
                                    * fatBinaryCodeKind* kind, 
                                    * size_t* size */
#define FBCTL_GET_IDENTIFIER    11 /* char* * */
#define FBCTL_HAS_DEBUG         12 /* Bool * */

typedef enum {
  fatBinary_PreferBestCode,  /* default */
  fatBinary_AvoidPTX,        /* use sass if possible for compile-time savings */
  fatBinary_ForcePTX,        /* use ptx (mainly for testing) */
  fatBinary_JITIfNotMatch    /* use ptx if arch doesn't match */
} fatBinary_CompilationPolicy;

/* 
 * Using the input values, pick the best candidate;
 * use subsequent Ctl requests to get info about that candidate.
 */
extern fatBinaryCtlError_t fatBinaryCtl_PickCandidate (void* data);

/* 
 * Using the previously chosen candidate, compile the code to elf,
 * returning elf image and size.
 * Note that because elf is allocated inside fatBinaryCtl, 
 * it will be freed when _Delete routine is called.
 */
extern fatBinaryCtlError_t fatBinaryCtl_Compile (void* data, 
                                                 void* *elf, size_t *esize);

/*
 * These defines are for the fatbin.c runtime wrapper
 */
#define FATBINC_MAGIC   0x466243B1
#define FATBINC_VERSION 1
#define FATBINC_LINK_VERSION 2

typedef struct {
  int magic;
  int version;
  const unsigned long long* data;
  void *filename_or_fatbins;  /* version 1: offline filename,
                               * version 2: array of prelinked fatbins */
} __fatBinC_Wrapper_t;

/*
 * The section that contains the fatbin control structure
 */
#ifdef STD_OS_Darwin
/* mach-o sections limited to 15 chars, and want __ prefix else strip complains, * so use a different name */
#define FATBIN_CONTROL_SECTION_NAME     "__fatbin"
#define FATBIN_DATA_SECTION_NAME        "__nv_fatbin"
/* only need segment name for mach-o */
#define FATBIN_SEGMENT_NAME             "__NV_CUDA"
#else
#define FATBIN_CONTROL_SECTION_NAME     ".nvFatBinSegment"
/*
 * The section that contains the fatbin data itself
 * (put in separate section so easy to find)
 */
#define FATBIN_DATA_SECTION_NAME        ".nv_fatbin"
#endif
/* section for pre-linked relocatable fatbin data */
#define FATBIN_PRELINK_DATA_SECTION_NAME "__nv_relfatbin"

#endif /* fatbinaryctl_INCLUDED */
