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

#ifndef fatbinary_INCLUDED
#define fatbinary_INCLUDED

/* 
 * This is the fat binary header structure. 
 * Because all layout information is contained in all the structures, 
 * it is both forward and backward compatible. 
 * A new driver can interpret an old binary 
 * as it will not address fields that are present in the current version. 
 * An old driver can, for minor version differences, 
 * still interpret a new binary, 
 * as the new features in the binary will be ignored by the driver.
 *
 * This is the top level type for the binary format. 
 * It points to a fatBinaryHeader structure. 
 * It is followed by a number of code binaries.
 * The structures must be 8-byte aligned, 
 * and are the same on both 32bit and 64bit platforms.
 *
 * The details of the format for the binaries that follow the header
 * are in a separate internal header.
 */

typedef struct fatBinaryHeader *computeFatBinaryFormat_t;

/* ensure 8-byte alignment */
#if defined(__GNUC__)
#define __align__(n) __attribute__((aligned(n)))
#elif defined(_WIN32)
#define __align__(n) __declspec(align(n))
#else
#error !! UNSUPPORTED COMPILER !!
#endif

/* Magic numbers */
#define FATBIN_MAGIC 0xBA55ED50
#define OLD_STYLE_FATBIN_MAGIC 0x1EE55A01

#define FATBIN_VERSION 0x0001

/*
 * This is the fat binary header structure. 
 * The 'magic' field holds the magic number. 
 * A magic of OLD_STYLE_FATBIN_MAGIC indicates an old style fat binary. 
 * Because old style binaries are in little endian, we can just read 
 * the magic in a 32 bit container for both 32 and 64 bit platforms. 
 * The 'version' fields holds the fatbin version.
 * It should be the goal to never bump this version. 
 * The headerSize holds the size of the header (must be multiple of 8).
 * The 'fatSize' fields holds the size of the entire fat binary, 
 * excluding this header. It must be a multiple of 8.
 */
struct __align__(8) fatBinaryHeader
{
  unsigned int           magic;
  unsigned short         version;
  unsigned short         headerSize;
  unsigned long long int fatSize;
};

/* Code kinds supported by the driver */
typedef enum {
  FATBIN_KIND_PTX      = 0x0001,
  FATBIN_KIND_ELF      = 0x0002,
  FATBIN_KIND_OLDCUBIN = 0x0004,
} fatBinaryCodeKind;

#endif /* fatbinary_INCLUDED */
