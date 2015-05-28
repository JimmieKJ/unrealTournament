 /* Copyright 2009-2014 NVIDIA Corporation.  All rights reserved. 
  * 
  * NOTICE TO LICENSEE: 
  * 
  * The source code and/or documentation ("Licensed Deliverables") are 
  * subject to NVIDIA intellectual property rights under U.S. and 
  * international Copyright laws. 
  * 
  * The Licensed Deliverables contained herein are PROPRIETARY and 
  * CONFIDENTIAL to NVIDIA and are being provided under the terms and 
  * conditions of a form of NVIDIA software license agreement by and 
  * between NVIDIA and Licensee ("License Agreement") or electronically 
  * accepted by Licensee.  Notwithstanding any terms or conditions to 
  * the contrary in the License Agreement, reproduction or disclosure 
  * of the Licensed Deliverables to any third party without the express 
  * written consent of NVIDIA is prohibited. 
  * 
  * NOTWITHSTANDING ANY TERMS OR CONDITIONS TO THE CONTRARY IN THE 
  * LICENSE AGREEMENT, NVIDIA MAKES NO REPRESENTATION ABOUT THE 
  * SUITABILITY OF THESE LICENSED DELIVERABLES FOR ANY PURPOSE.  THEY ARE 
  * PROVIDED "AS IS" WITHOUT EXPRESS OR IMPLIED WARRANTY OF ANY KIND. 
  * NVIDIA DISCLAIMS ALL WARRANTIES WITH REGARD TO THESE LICENSED 
  * DELIVERABLES, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY, 
  * NONINFRINGEMENT, AND FITNESS FOR A PARTICULAR PURPOSE. 
  * NOTWITHSTANDING ANY TERMS OR CONDITIONS TO THE CONTRARY IN THE 
  * LICENSE AGREEMENT, IN NO EVENT SHALL NVIDIA BE LIABLE FOR ANY 
  * SPECIAL, INDIRECT, INCIDENTAL, OR CONSEQUENTIAL DAMAGES, OR ANY 
  * DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, 
  * WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS 
  * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE 
  * OF THESE LICENSED DELIVERABLES. 
  * 
  * U.S. Government End Users.  These Licensed Deliverables are a 
  * "commercial item" as that term is defined at 48 C.F.R. 2.101 (OCT 
  * 1995), consisting of "commercial computer software" and "commercial 
  * computer software documentation" as such terms are used in 48 
  * C.F.R. 12.212 (SEPT 1995) and are provided to the U.S. Government 
  * only as a commercial end item.  Consistent with 48 C.F.R.12.212 and 
  * 48 C.F.R. 227.7202-1 through 227.7202-4 (JUNE 1995), all 
  * U.S. Government End Users acquire the Licensed Deliverables with 
  * only those rights set forth herein. 
  * 
  * Any use of the Licensed Deliverables in individual and commercial 
  * software must include, in the user documentation and internal 
  * comments to the code, the above Disclaimer and U.S. Government End 
  * Users Notice. 
  */ 
#ifndef NV_NPPCORE_H
#define NV_NPPCORE_H

#include <cuda_runtime_api.h>

/**
 * \file nppcore.h
 * Basic NPP functionality. 
 *  This file contains functions to query the NPP version as well as 
 *  info about the CUDA compute capabilities on a given computer.
 */
 
#include "nppdefs.h"

#ifdef __cplusplus
extern "C" {
#endif
 
/** \defgroup core_npp NPP Core
 * Basic functions for library management, in particular library version
 * and device property query functions.
 * @{
 */

/**
 * Get the NPP library version.
 *
 * \return A struct containing separate values for major and minor revision 
 *      and build number.
 */
const NppLibraryVersion * 
nppGetLibVersion(void);

/**
 * What CUDA compute model is supported by the active CUDA device?
 * 
 * Before trying to call any NPP functions, the user should make a call
 * this function to ensure that the current machine has a CUDA capable device.
 *
 * \return An enum value representing if a CUDA capable device was found and what
 *      level of compute capabilities it supports.
 */
NppGpuComputeCapability 
nppGetGpuComputeCapability(void);

/**
 * Get the number of Streaming Multiprocessors (SM) on the active CUDA device.
 *
 * \return Number of SMs of the default CUDA device.
 */
int 
nppGetGpuNumSMs(void);

/**
 * Get the maximum number of threads per block on the active CUDA device.
 *
 * \return Maximum number of threads per block on the active CUDA device.
 */
int 
nppGetMaxThreadsPerBlock(void);

/**
 * Get the maximum number of threads per SM for the active GPU
 *
 * \return Maximum number of threads per SM for the active GPU
 */
int 
nppGetMaxThreadsPerSM(void);

/** 
 * Get the name of the active CUDA device.
 *
 * \return Name string of the active graphics-card/compute device in a system.
 */
const char * 
nppGetGpuName(void);

/**
 * Get the NPP CUDA stream.
 * NPP enables concurrent device tasks via a global stream state varible.
 * The NPP stream by default is set to stream 0, i.e. non-concurrent mode.
 * A user can set the NPP stream to any valid CUDA stream. All CUDA commands
 * issued by NPP (e.g. kernels launched by the NPP library) are then
 * issed to that NPP stream.
 */
cudaStream_t
nppGetStream(void);

/**
 * Set the NPP CUDA stream.
 * \see nppGetStream()
 */
void
nppSetStream(cudaStream_t hStream);


/** @} Module LabelCoreNPP */


#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* NV_NPPCORE_H */
