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
#ifndef NV_NPPI_FILTERING_FUNCTIONS_H
#define NV_NPPI_FILTERING_FUNCTIONS_H
 
/**
 * \file nppi_filtering_functions.h
 * NPP Image Processing Functionality.
 */
 
#include "nppdefs.h"


#ifdef __cplusplus
extern "C" {
#endif

/** @defgroup image_filtering_functions Filtering Functions
 *  @ingroup nppi
 *
 * Linear and non-linear image filtering functions.
 *
 * Filtering functions are classified as \ref neighborhood_operations. It is the user's 
 * responsibility to avoid \ref sampling_beyond_image_boundaries. 
 *
 * @{
 *
 */

/** @defgroup image_1D_linear_filter 1D Linear Filter
 *
 * @{
 *
 */

/** @name FilterColumn
 * Apply convolution filter with user specified 1D column of weights.  
 * Result pixel is equal to the sum of the products between the kernel
 * coefficients (pKernel array) and corresponding neighboring column pixel
 * values in the source image defined by nKernelDim and nAnchorY, divided by
 * nDivisor. 
 * 
 * @{
 *
 */

/**
 * 8-bit unsigned single-channel 1D column convolution.
 * 
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *                Coefficients are expected to be stored in reverse order.
 * \param nMaskSize Length of the linear kernel array.
 * \param nAnchor Y offset of the kernel origin frame of reference relative to the
 *                 source pixel.
 * \param nDivisor The factor by which the convolved summation from the Filter
 *                 operation should be divided.  If equal to the sum of
 *                 coefficients, this will keep the maximum result value within
 *                 full scale.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterColumn_8u_C1R(const Npp8u * pSrc, Npp32s nSrcStep, Npp8u * pDst, Npp32s nDstStep, NppiSize oROI, 
                        const Npp32s * pKernel, Npp32s nMaskSize, Npp32s nAnchor, Npp32s nDivisor);

/**
 * 8-bit unsigned three-channel 1D column convolution.
 * 
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *                Coefficients are expected to be stored in reverse order.
 * \param nMaskSize Length of the linear kernel array.
 * \param nAnchor Y offset of the kernel origin frame of reference relative to the
 *                 source pixel.
 * \param nDivisor The factor by which the convolved summation from the Filter
 *                 operation should be divided.  If equal to the sum of
 *                 coefficients, this will keep the maximum result value within
 *                 full scale.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterColumn_8u_C3R(const Npp8u * pSrc, Npp32s nSrcStep, Npp8u * pDst, Npp32s nDstStep, NppiSize oROI, 
                        const Npp32s * pKernel, Npp32s nMaskSize, Npp32s nAnchor, Npp32s nDivisor);

/**
 * 8-bit unsigned four-channel 1D column convolution.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *                Coefficients are expected to be stored in reverse order.
 * \param nMaskSize Length of the linear kernel array.
 * \param nAnchor Y offset of the kernel origin frame of reference relative to the
 *                 source pixel.
 * \param nDivisor The factor by which the convolved summation from the Filter
 *                 operation should be divided.  If equal to the sum of
 *                 coefficients, this will keep the maximum result value within
 *                 full scale.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterColumn_8u_C4R(const Npp8u * pSrc, Npp32s nSrcStep, Npp8u * pDst, Npp32s nDstStep, NppiSize oROI, 
                        const Npp32s * pKernel, Npp32s nMaskSize, Npp32s nAnchor, Npp32s nDivisor);

/**
 * 8-bit unsigned four-channel 1D column convolution ignoring alpha-channel.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *                Coefficients are expected to be stored in reverse order.
 * \param nMaskSize Length of the linear kernel array.
 * \param nAnchor Y offset of the kernel origin frame of reference relative to the
 *                 source pixel.
 * \param nDivisor The factor by which the convolved summation from the Filter
 *                 operation should be divided.  If equal to the sum of
 *                 coefficients, this will keep the maximum result value within
 *                 full scale.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterColumn_8u_AC4R(const Npp8u * pSrc, Npp32s nSrcStep, Npp8u * pDst, Npp32s nDstStep, NppiSize oROI, 
                         const Npp32s * pKernel, Npp32s nMaskSize, Npp32s nAnchor, Npp32s nDivisor);

/**
 * 16-bit unsigned single-channel 1D column convolution.
 * 
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *                Coefficients are expected to be stored in reverse order.
 * \param nMaskSize Length of the linear kernel array.
 * \param nAnchor Y offset of the kernel origin frame of reference relative to the
 *                 source pixel.
 * \param nDivisor The factor by which the convolved summation from the Filter
 *                 operation should be divided.  If equal to the sum of
 *                 coefficients, this will keep the maximum result value within
 *                 full scale.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterColumn_16u_C1R(const Npp16u * pSrc, Npp32s nSrcStep, Npp16u * pDst, Npp32s nDstStep, NppiSize oROI, 
                         const Npp32s * pKernel, Npp32s nMaskSize, Npp32s nAnchor, Npp32s nDivisor);

/**
 * 16-bit unsigned three-channel 1D column convolution.
 * 
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *                Coefficients are expected to be stored in reverse order.
 * \param nMaskSize Length of the linear kernel array.
 * \param nAnchor Y offset of the kernel origin frame of reference relative to the
 *                 source pixel.
 * \param nDivisor The factor by which the convolved summation from the Filter
 *                 operation should be divided.  If equal to the sum of
 *                 coefficients, this will keep the maximum result value within
 *                 full scale.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterColumn_16u_C3R(const Npp16u * pSrc, Npp32s nSrcStep, Npp16u * pDst, Npp32s nDstStep, NppiSize oROI, 
                         const Npp32s * pKernel, Npp32s nMaskSize, Npp32s nAnchor, Npp32s nDivisor);

/**
 * 16-bit unsigned four-channel 1D column convolution.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *                Coefficients are expected to be stored in reverse order.
 * \param nMaskSize Length of the linear kernel array.
 * \param nAnchor Y offset of the kernel origin frame of reference relative to the
 *                 source pixel.
 * \param nDivisor The factor by which the convolved summation from the Filter
 *                 operation should be divided.  If equal to the sum of
 *                 coefficients, this will keep the maximum result value within
 *                 full scale.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterColumn_16u_C4R(const Npp16u * pSrc, Npp32s nSrcStep, Npp16u * pDst, Npp32s nDstStep, NppiSize oROI, 
                         const Npp32s * pKernel, Npp32s nMaskSize, Npp32s nAnchor, Npp32s nDivisor);

/**
 * 16-bit unsigned four-channel 1D column convolution ignoring alpha-channel.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *                Coefficients are expected to be stored in reverse order.
 * \param nMaskSize Length of the linear kernel array.
 * \param nAnchor Y offset of the kernel origin frame of reference relative to the
 *                 source pixel.
 * \param nDivisor The factor by which the convolved summation from the Filter
 *                 operation should be divided.  If equal to the sum of
 *                 coefficients, this will keep the maximum result value within
 *                 full scale.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterColumn_16u_AC4R(const Npp16u * pSrc, Npp32s nSrcStep, Npp16u * pDst, Npp32s nDstStep, NppiSize oROI, 
                          const Npp32s * pKernel, Npp32s nMaskSize, Npp32s nAnchor, Npp32s nDivisor);

/**
 * 16-bit single-channel 1D column convolution.
 * 
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *                Coefficients are expected to be stored in reverse order.
 * \param nMaskSize Length of the linear kernel array.
 * \param nAnchor Y offset of the kernel origin frame of reference relative to the
 *                 source pixel.
 * \param nDivisor The factor by which the convolved summation from the Filter
 *                 operation should be divided.  If equal to the sum of
 *                 coefficients, this will keep the maximum result value within
 *                 full scale.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterColumn_16s_C1R(const Npp16s * pSrc, Npp32s nSrcStep, Npp16s * pDst, Npp32s nDstStep, NppiSize oROI, 
                         const Npp32s * pKernel, Npp32s nMaskSize, Npp32s nAnchor, Npp32s nDivisor);

/**
 * 16-bit three-channel 1D column convolution.
 * 
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *                Coefficients are expected to be stored in reverse order.
 * \param nMaskSize Length of the linear kernel array.
 * \param nAnchor Y offset of the kernel origin frame of reference relative to the
 *                 source pixel.
 * \param nDivisor The factor by which the convolved summation from the Filter
 *                 operation should be divided.  If equal to the sum of
 *                 coefficients, this will keep the maximum result value within
 *                 full scale.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterColumn_16s_C3R(const Npp16s * pSrc, Npp32s nSrcStep, Npp16s * pDst, Npp32s nDstStep, NppiSize oROI, 
                         const Npp32s * pKernel, Npp32s nMaskSize, Npp32s nAnchor, Npp32s nDivisor);

/**
 * 16-bit four-channel 1D column convolution.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *                Coefficients are expected to be stored in reverse order.
 * \param nMaskSize Length of the linear kernel array.
 * \param nAnchor Y offset of the kernel origin frame of reference relative to the
 *                 source pixel.
 * \param nDivisor The factor by which the convolved summation from the Filter
 *                 operation should be divided.  If equal to the sum of
 *                 coefficients, this will keep the maximum result value within
 *                 full scale.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterColumn_16s_C4R(const Npp16s * pSrc, Npp32s nSrcStep, Npp16s * pDst, Npp32s nDstStep, NppiSize oROI, 
                         const Npp32s * pKernel, Npp32s nMaskSize, Npp32s nAnchor, Npp32s nDivisor);

/**
 * 16-bit four-channel 1D column convolution ignoring alpha-channel.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *                Coefficients are expected to be stored in reverse order.
 * \param nMaskSize Length of the linear kernel array.
 * \param nAnchor Y offset of the kernel origin frame of reference relative to the
 *                 source pixel.
 * \param nDivisor The factor by which the convolved summation from the Filter
 *                 operation should be divided.  If equal to the sum of
 *                 coefficients, this will keep the maximum result value within
 *                 full scale.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterColumn_16s_AC4R(const Npp16s * pSrc, Npp32s nSrcStep, Npp16s * pDst, Npp32s nDstStep, NppiSize oROI, 
                          const Npp32s * pKernel, Npp32s nMaskSize, Npp32s nAnchor, Npp32s nDivisor);

/**
 * 32-bit float single-channel 1D column convolution.
 * 
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *                Coefficients are expected to be stored in reverse order.
 * \param nMaskSize Length of the linear kernel array.
 * \param nAnchor Y offset of the kernel origin frame of reference relative to the
 *                 source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterColumn_32f_C1R(const Npp32f * pSrc, Npp32s nSrcStep, Npp32f * pDst, Npp32s nDstStep, NppiSize oROI, 
                         const Npp32f * pKernel, Npp32s nMaskSize, Npp32s nAnchor);

/**
 * 32-bit float three-channel 1D column convolution.
 * 
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *                Coefficients are expected to be stored in reverse order.
 * \param nMaskSize Length of the linear kernel array.
 * \param nAnchor Y offset of the kernel origin frame of reference relative to the
 *                 source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterColumn_32f_C3R(const Npp32f * pSrc, Npp32s nSrcStep, Npp32f * pDst, Npp32s nDstStep, NppiSize oROI, 
                         const Npp32f * pKernel, Npp32s nMaskSize, Npp32s nAnchor);

/**
 * 32-bit float four-channel 1D column convolution.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *                Coefficients are expected to be stored in reverse order.
 * \param nMaskSize Length of the linear kernel array.
 * \param nAnchor Y offset of the kernel origin frame of reference relative to the
 *                 source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterColumn_32f_C4R(const Npp32f * pSrc, Npp32s nSrcStep, Npp32f * pDst, Npp32s nDstStep, NppiSize oROI, 
                         const Npp32f * pKernel, Npp32s nMaskSize, Npp32s nAnchor);

/**
 * 32-bit float four-channel 1D column convolution ignoring alpha-channel.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *                Coefficients are expected to be stored in reverse order.
 * \param nMaskSize Length of the linear kernel array.
 * \param nAnchor Y offset of the kernel origin frame of reference relative to the
 *                 source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterColumn_32f_AC4R(const Npp32f * pSrc, Npp32s nSrcStep, Npp32f * pDst, Npp32s nDstStep, NppiSize oROI, 
                          const Npp32f * pKernel, Npp32s nMaskSize, Npp32s nAnchor);

/**
 * 64-bit float single-channel 1D column convolution.
 * 
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *                Coefficients are expected to be stored in reverse order.
 * \param nMaskSize Length of the linear kernel array.
 * \param nAnchor Y offset of the kernel origin frame of reference relative to the
 *                 source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterColumn_64f_C1R(const Npp64f * pSrc, Npp32s nSrcStep, Npp64f * pDst, Npp32s nDstStep, NppiSize oROI, 
                         const Npp64f * pKernel, Npp32s nMaskSize, Npp32s nAnchor);


/** @} FilterColumn */

/** @name FilterColumn32f
 * 
 * FilterColumn using floating-point weights.
 * 
 * @{
 *
 */

/**
 * 8-bit unsigned single-channel 1D column convolution.
 * 
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *                Coefficients are expected to be stored in reverse order.
 * \param nMaskSize Length of the linear kernel array.
 * \param nAnchor Y offset of the kernel origin frame of reference relative to the
 *                 source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterColumn32f_8u_C1R(const Npp8u * pSrc, Npp32s nSrcStep, Npp8u * pDst, Npp32s nDstStep, NppiSize oROI, 
                           const Npp32f * pKernel, Npp32s nMaskSize, Npp32s nAnchor);

/**
 * 8-bit unsigned three-channel 1D column convolution.
 * 
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *                Coefficients are expected to be stored in reverse order.
 * \param nMaskSize Length of the linear kernel array.
 * \param nAnchor Y offset of the kernel origin frame of reference relative to the
 *                 source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterColumn32f_8u_C3R(const Npp8u * pSrc, Npp32s nSrcStep, Npp8u * pDst, Npp32s nDstStep, NppiSize oROI, 
                           const Npp32f * pKernel, Npp32s nMaskSize, Npp32s nAnchor);

/**
 * 8-bit unsigned four-channel 1D column convolution.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *                Coefficients are expected to be stored in reverse order.
 * \param nMaskSize Length of the linear kernel array.
 * \param nAnchor Y offset of the kernel origin frame of reference relative to the
 *                 source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterColumn32f_8u_C4R(const Npp8u * pSrc, Npp32s nSrcStep, Npp8u * pDst, Npp32s nDstStep, NppiSize oROI, 
                           const Npp32f * pKernel, Npp32s nMaskSize, Npp32s nAnchor);

/**
 * 8-bit unsigned four-channel 1D column convolution ignoring alpha-channel.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *                Coefficients are expected to be stored in reverse order.
 * \param nMaskSize Length of the linear kernel array.
 * \param nAnchor Y offset of the kernel origin frame of reference relative to the
 *                 source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterColumn32f_8u_AC4R(const Npp8u * pSrc, Npp32s nSrcStep, Npp8u * pDst, Npp32s nDstStep, NppiSize oROI, 
                            const Npp32f * pKernel, Npp32s nMaskSize, Npp32s nAnchor);

/**
 * 16-bit unsigned single-channel 1D column convolution.
 * 
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *                Coefficients are expected to be stored in reverse order.
 * \param nMaskSize Length of the linear kernel array.
 * \param nAnchor Y offset of the kernel origin frame of reference relative to the
 *                 source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterColumn32f_16u_C1R(const Npp16u * pSrc, Npp32s nSrcStep, Npp16u * pDst, Npp32s nDstStep, NppiSize oROI, 
                            const Npp32f * pKernel, Npp32s nMaskSize, Npp32s nAnchor);

/**
 * 16-bit unsigned three-channel 1D column convolution.
 * 
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *                Coefficients are expected to be stored in reverse order.
 * \param nMaskSize Length of the linear kernel array.
 * \param nAnchor Y offset of the kernel origin frame of reference relative to the
 *                 source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterColumn32f_16u_C3R(const Npp16u * pSrc, Npp32s nSrcStep, Npp16u * pDst, Npp32s nDstStep, NppiSize oROI, 
                            const Npp32f * pKernel, Npp32s nMaskSize, Npp32s nAnchor);

/**
 * 16-bit unsigned four-channel 1D column convolution.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *                Coefficients are expected to be stored in reverse order.
 * \param nMaskSize Length of the linear kernel array.
 * \param nAnchor Y offset of the kernel origin frame of reference relative to the
 *                 source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterColumn32f_16u_C4R(const Npp16u * pSrc, Npp32s nSrcStep, Npp16u * pDst, Npp32s nDstStep, NppiSize oROI, 
                            const Npp32f * pKernel, Npp32s nMaskSize, Npp32s nAnchor);

/**
 * 16-bit unsigned four-channel 1D column convolution ignoring alpha-channel.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *                Coefficients are expected to be stored in reverse order.
 * \param nMaskSize Length of the linear kernel array.
 * \param nAnchor Y offset of the kernel origin frame of reference relative to the
 *                 source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterColumn32f_16u_AC4R(const Npp16u * pSrc, Npp32s nSrcStep, Npp16u * pDst, Npp32s nDstStep, NppiSize oROI, 
                             const Npp32f * pKernel, Npp32s nMaskSize, Npp32s nAnchor);

/**
 * 16-bit single-channel 1D column convolution.
 * 
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *                Coefficients are expected to be stored in reverse order.
 * \param nMaskSize Length of the linear kernel array.
 * \param nAnchor Y offset of the kernel origin frame of reference relative to the
 *                 source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterColumn32f_16s_C1R(const Npp16s * pSrc, Npp32s nSrcStep, Npp16s * pDst, Npp32s nDstStep, NppiSize oROI, 
                            const Npp32f * pKernel, Npp32s nMaskSize, Npp32s nAnchor);

/**
 * 16-bit three-channel 1D column convolution.
 * 
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *                Coefficients are expected to be stored in reverse order.
 * \param nMaskSize Length of the linear kernel array.
 * \param nAnchor Y offset of the kernel origin frame of reference relative to the
 *                 source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterColumn32f_16s_C3R(const Npp16s * pSrc, Npp32s nSrcStep, Npp16s * pDst, Npp32s nDstStep, NppiSize oROI, 
                            const Npp32f * pKernel, Npp32s nMaskSize, Npp32s nAnchor);

/**
 * 16-bit four-channel 1D column convolution.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *                Coefficients are expected to be stored in reverse order.
 * \param nMaskSize Length of the linear kernel array.
 * \param nAnchor Y offset of the kernel origin frame of reference relative to the
 *                 source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterColumn32f_16s_C4R(const Npp16s * pSrc, Npp32s nSrcStep, Npp16s * pDst, Npp32s nDstStep, NppiSize oROI, 
                            const Npp32f * pKernel, Npp32s nMaskSize, Npp32s nAnchor);

/**
 * 16-bit four-channel 1D column convolution ignoring alpha-channel.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *                Coefficients are expected to be stored in reverse order.
 * \param nMaskSize Length of the linear kernel array.
 * \param nAnchor Y offset of the kernel origin frame of reference relative to the
 *                 source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterColumn32f_16s_AC4R(const Npp16s * pSrc, Npp32s nSrcStep, Npp16s * pDst, Npp32s nDstStep, NppiSize oROI, 
                             const Npp32f * pKernel, Npp32s nMaskSize, Npp32s nAnchor);

/** @} FilterColumn32f */


/** @name FilterRow
 * Apply convolution filter with user specified 1D row of weights.  
 * Result pixel is equal to the sum of the products between the kernel
 * coefficients (pKernel array) and corresponding neighboring row pixel
 * values in the source image defined by nKernelDim and nAnchorX, divided by
 * nDivisor. 
 * 
 * @{
 *
 */

/**
 * 8-bit unsigned single-channel 1D row convolution.
 * 
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *                Coefficients are expected to be stored in reverse order.
 * \param nMaskSize Length of the linear kernel array.
 * \param nAnchor X offset of the kernel origin frame of reference relative to the
 *                 source pixel.
 * \param nDivisor The factor by which the convolved summation from the Filter
 *                 operation should be divided.  If equal to the sum of
 *                 coefficients, this will keep the maximum result value within
 *                 full scale.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterRow_8u_C1R(const Npp8u * pSrc, Npp32s nSrcStep, Npp8u * pDst, Npp32s nDstStep, NppiSize oROI, 
                     const Npp32s * pKernel, Npp32s nMaskSize, Npp32s nAnchor, Npp32s nDivisor);

/**
 * 8-bit unsigned three-channel 1D row convolution.
 * 
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *                Coefficients are expected to be stored in reverse order.
 * \param nMaskSize Length of the linear kernel array.
 * \param nAnchor X offset of the kernel origin frame of reference relative to the
 *                 source pixel.
 * \param nDivisor The factor by which the convolved summation from the Filter
 *                 operation should be divided.  If equal to the sum of
 *                 coefficients, this will keep the maximum result value within
 *                 full scale.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterRow_8u_C3R(const Npp8u * pSrc, Npp32s nSrcStep, Npp8u * pDst, Npp32s nDstStep, NppiSize oROI, 
                     const Npp32s * pKernel, Npp32s nMaskSize, Npp32s nAnchor, Npp32s nDivisor);

/**
 * 8-bit unsigned four-channel 1D row convolution.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *                Coefficients are expected to be stored in reverse order.
 * \param nMaskSize Length of the linear kernel array.
 * \param nAnchor X offset of the kernel origin frame of reference relative to the
 *                 source pixel.
 * \param nDivisor The factor by which the convolved summation from the Filter
 *                 operation should be divided.  If equal to the sum of
 *                 coefficients, this will keep the maximum result value within
 *                 full scale.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterRow_8u_C4R(const Npp8u * pSrc, Npp32s nSrcStep, Npp8u * pDst, Npp32s nDstStep, NppiSize oROI, 
                     const Npp32s * pKernel, Npp32s nMaskSize, Npp32s nAnchor, Npp32s nDivisor);

/**
 * 8-bit unsigned four-channel 1D row convolution ignoring alpha-channel.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *                Coefficients are expected to be stored in reverse order.
 * \param nMaskSize Length of the linear kernel array.
 * \param nAnchor X offset of the kernel origin frame of reference relative to the
 *                 source pixel.
 * \param nDivisor The factor by which the convolved summation from the Filter
 *                 operation should be divided.  If equal to the sum of
 *                 coefficients, this will keep the maximum result value within
 *                 full scale.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterRow_8u_AC4R(const Npp8u * pSrc, Npp32s nSrcStep, Npp8u * pDst, Npp32s nDstStep, NppiSize oROI, 
                      const Npp32s * pKernel, Npp32s nMaskSize, Npp32s nAnchor, Npp32s nDivisor);

/**
 * 16-bit unsigned single-channel 1D row convolution.
 * 
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *                Coefficients are expected to be stored in reverse order.
 * \param nMaskSize Length of the linear kernel array.
 * \param nAnchor X offset of the kernel origin frame of reference relative to the
 *                 source pixel.
 * \param nDivisor The factor by which the convolved summation from the Filter
 *                 operation should be divided.  If equal to the sum of
 *                 coefficients, this will keep the maximum result value within
 *                 full scale.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterRow_16u_C1R(const Npp16u * pSrc, Npp32s nSrcStep, Npp16u * pDst, Npp32s nDstStep, NppiSize oROI, 
                      const Npp32s * pKernel, Npp32s nMaskSize, Npp32s nAnchor, Npp32s nDivisor);

/**
 * 16-bit unsigned three-channel 1D row convolution.
 * 
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *                Coefficients are expected to be stored in reverse order.
 * \param nMaskSize Length of the linear kernel array.
 * \param nAnchor X offset of the kernel origin frame of reference relative to the
 *                 source pixel.
 * \param nDivisor The factor by which the convolved summation from the Filter
 *                 operation should be divided.  If equal to the sum of
 *                 coefficients, this will keep the maximum result value within
 *                 full scale.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterRow_16u_C3R(const Npp16u * pSrc, Npp32s nSrcStep, Npp16u * pDst, Npp32s nDstStep, NppiSize oROI, 
                      const Npp32s * pKernel, Npp32s nMaskSize, Npp32s nAnchor, Npp32s nDivisor);

/**
 * 16-bit unsigned four-channel 1D row convolution.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *                Coefficients are expected to be stored in reverse order.
 * \param nMaskSize Length of the linear kernel array.
 * \param nAnchor X offset of the kernel origin frame of reference relative to the
 *                 source pixel.
 * \param nDivisor The factor by which the convolved summation from the Filter
 *                 operation should be divided.  If equal to the sum of
 *                 coefficients, this will keep the maximum result value within
 *                 full scale.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterRow_16u_C4R(const Npp16u * pSrc, Npp32s nSrcStep, Npp16u * pDst, Npp32s nDstStep, NppiSize oROI, 
                      const Npp32s * pKernel, Npp32s nMaskSize, Npp32s nAnchor, Npp32s nDivisor);

/**
 * 16-bit unsigned four-channel 1D row convolution ignoring alpha-channel.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *                Coefficients are expected to be stored in reverse order.
 * \param nMaskSize Length of the linear kernel array.
 * \param nAnchor X offset of the kernel origin frame of reference relative to the
 *                 source pixel.
 * \param nDivisor The factor by which the convolved summation from the Filter
 *                 operation should be divided.  If equal to the sum of
 *                 coefficients, this will keep the maximum result value within
 *                 full scale.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterRow_16u_AC4R(const Npp16u * pSrc, Npp32s nSrcStep, Npp16u * pDst, Npp32s nDstStep, NppiSize oROI, 
                       const Npp32s * pKernel, Npp32s nMaskSize, Npp32s nAnchor, Npp32s nDivisor);

/**
 * 16-bit single-channel 1D row convolution.
 * 
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *                Coefficients are expected to be stored in reverse order.
 * \param nMaskSize Length of the linear kernel array.
 * \param nAnchor X offset of the kernel origin frame of reference relative to the
 *                 source pixel.
 * \param nDivisor The factor by which the convolved summation from the Filter
 *                 operation should be divided.  If equal to the sum of
 *                 coefficients, this will keep the maximum result value within
 *                 full scale.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterRow_16s_C1R(const Npp16s * pSrc, Npp32s nSrcStep, Npp16s * pDst, Npp32s nDstStep, NppiSize oROI, 
                      const Npp32s * pKernel, Npp32s nMaskSize, Npp32s nAnchor, Npp32s nDivisor);

/**
 * 16-bit three-channel 1D row convolution.
 * 
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *                Coefficients are expected to be stored in reverse order.
 * \param nMaskSize Length of the linear kernel array.
 * \param nAnchor X offset of the kernel origin frame of reference relative to the
 *                 source pixel.
 * \param nDivisor The factor by which the convolved summation from the Filter
 *                 operation should be divided.  If equal to the sum of
 *                 coefficients, this will keep the maximum result value within
 *                 full scale.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterRow_16s_C3R(const Npp16s * pSrc, Npp32s nSrcStep, Npp16s * pDst, Npp32s nDstStep, NppiSize oROI, 
                      const Npp32s * pKernel, Npp32s nMaskSize, Npp32s nAnchor, Npp32s nDivisor);

/**
 * 16-bit four-channel 1D row convolution.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *                Coefficients are expected to be stored in reverse order.
 * \param nMaskSize Length of the linear kernel array.
 * \param nAnchor X offset of the kernel origin frame of reference relative to the
 *                 source pixel.
 * \param nDivisor The factor by which the convolved summation from the Filter
 *                 operation should be divided.  If equal to the sum of
 *                 coefficients, this will keep the maximum result value within
 *                 full scale.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterRow_16s_C4R(const Npp16s * pSrc, Npp32s nSrcStep, Npp16s * pDst, Npp32s nDstStep, NppiSize oROI, 
                      const Npp32s * pKernel, Npp32s nMaskSize, Npp32s nAnchor, Npp32s nDivisor);

/**
 * 16-bit four-channel 1D row convolution ignoring alpha-channel.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *                Coefficients are expected to be stored in reverse order.
 * \param nMaskSize Length of the linear kernel array.
 * \param nAnchor X offset of the kernel origin frame of reference relative to the
 *                 source pixel.
 * \param nDivisor The factor by which the convolved summation from the Filter
 *                 operation should be divided.  If equal to the sum of
 *                 coefficients, this will keep the maximum result value within
 *                 full scale.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterRow_16s_AC4R(const Npp16s * pSrc, Npp32s nSrcStep, Npp16s * pDst, Npp32s nDstStep, NppiSize oROI, 
                       const Npp32s * pKernel, Npp32s nMaskSize, Npp32s nAnchor, Npp32s nDivisor);


/**
 * 32-bit float single-channel 1D row convolution.
 * 
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *                Coefficients are expected to be stored in reverse order.
 * \param nMaskSize Length of the linear kernel array.
 * \param nAnchor X offset of the kernel origin frame of reference relative to the
 *                 source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterRow_32f_C1R(const Npp32f * pSrc, Npp32s nSrcStep, Npp32f * pDst, Npp32s nDstStep, NppiSize oROI, 
                      const Npp32f * pKernel, Npp32s nMaskSize, Npp32s nAnchor);

/**
 * 32-bit float three-channel 1D row convolution.
 * 
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *                Coefficients are expected to be stored in reverse order.
 * \param nMaskSize Length of the linear kernel array.
 * \param nAnchor X offset of the kernel origin frame of reference relative to the
 *                 source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterRow_32f_C3R(const Npp32f * pSrc, Npp32s nSrcStep, Npp32f * pDst, Npp32s nDstStep, NppiSize oROI, 
                      const Npp32f * pKernel, Npp32s nMaskSize, Npp32s nAnchor);

/**
 * 32-bit float four-channel 1D row convolution.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *                Coefficients are expected to be stored in reverse order.
 * \param nMaskSize Length of the linear kernel array.
 * \param nAnchor X offset of the kernel origin frame of reference relative to the
 *                 source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterRow_32f_C4R(const Npp32f * pSrc, Npp32s nSrcStep, Npp32f * pDst, Npp32s nDstStep, NppiSize oROI, 
                      const Npp32f * pKernel, Npp32s nMaskSize, Npp32s nAnchor);

/**
 * 32-bit float four-channel 1D row convolution ignoring alpha-channel.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *                Coefficients are expected to be stored in reverse order.
 * \param nMaskSize Length of the linear kernel array.
 * \param nAnchor X offset of the kernel origin frame of reference relative to the
 *                 source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterRow_32f_AC4R(const Npp32f * pSrc, Npp32s nSrcStep, Npp32f * pDst, Npp32s nDstStep, NppiSize oROI, 
                       const Npp32f * pKernel, Npp32s nMaskSize, Npp32s nAnchor);

/**
 * 64-bit float single-channel 1D row convolution.
 * 
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *                Coefficients are expected to be stored in reverse order.
 * \param nMaskSize Length of the linear kernel array.
 * \param nAnchor X offset of the kernel origin frame of reference relative to the
 *                 source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterRow_64f_C1R(const Npp64f * pSrc, Npp32s nSrcStep, Npp64f * pDst, Npp32s nDstStep, NppiSize oROI, 
                      const Npp64f * pKernel, Npp32s nMaskSize, Npp32s nAnchor);


/** @} FilterRow */

/** @name FilterRowBorder
 * General purpose 1D convolution filter with border control.
 *
 * Pixels under the mask are multiplied by the respective weights in the mask
 * and the results are summed. Before writing the result pixel the sum is scaled
 * back via division by nDivisor. If any portion of the mask overlaps the source
 * image boundary the requested border type operation is applied to all mask pixels
 * which fall outside of the source image.
 *
 * Currently only the NPP_BORDER_REPLICATE border type operation is supported.
 *
 * @{
 *
 */

/**
 * Single channel 8-bit unsigned 1D row convolution filter with border control.
 * 
 *
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *        Coeffcients are expected to be stored in reverse order.
 * \param nMaskSize Width of the kernel.
 * \param nAnchor X offset of the kernel origin frame of reference
 *        relative to the source pixel.
 * \param nDivisor The factor by which the convolved summation from the Filter
 *        operation should be divided.  If equal to the sum of coefficients,
 *        this will keep the maximum result value within full scale.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterRowBorder_8u_C1R(const Npp8u * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp8u * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                           const Npp32s * pKernel, Npp32s nMaskSize, Npp32s nAnchor, Npp32s nDivisor, NppiBorderType eBorderType);
  
/**
 * Three channel 8-bit unsigned 1D row convolution filter with border control.
 * 
 *
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *        Coeffcients are expected to be stored in reverse order.
 * \param nMaskSize Width of the kernel.
 * \param nAnchor X offset of the kernel origin frame of reference
 *        relative to the source pixel.
 * \param nDivisor The factor by which the convolved summation from the Filter
 *        operation should be divided.  If equal to the sum of coefficients,
 *        this will keep the maximum result value within full scale.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterRowBorder_8u_C3R(const Npp8u * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp8u * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                           const Npp32s * pKernel, Npp32s nMaskSize, Npp32s nAnchor, Npp32s nDivisor, NppiBorderType eBorderType);
                 
/**
 * Four channel channel 8-bit unsigned 1D row convolution filter with border control.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *        Coeffcients are expected to be stored in reverse order.
 * \param nMaskSize Width of the kernel.
 * \param nAnchor X offset of the kernel origin frame of reference
 *        relative to the source pixel.
 * \param nDivisor The factor by which the convolved summation from the Filter
 *        operation should be divided.  If equal to the sum of coefficients,
 *        this will keep the maximum result value within full scale.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterRowBorder_8u_C4R(const Npp8u * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp8u * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                           const Npp32s * pKernel, Npp32s nMaskSize, Npp32s nAnchor, Npp32s nDivisor, NppiBorderType eBorderType);

/**
 * Four channel 8-bit unsigned convolution 1D row filter with border control, ignoring alpha channel.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *        Coeffcients are expected to be stored in reverse order.
 * \param nMaskSize Width of the kernel.
 * \param nAnchor X offset of the kernel origin frame of reference
 *        relative to the source pixel.
 * \param nDivisor The factor by which the convolved summation from the Filter
 *        operation should be divided.  If equal to the sum of coefficients,
 *        this will keep the maximum result value within full scale.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterRowBorder_8u_AC4R(const Npp8u * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp8u * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                            const Npp32s * pKernel, Npp32s nMaskSize, Npp32s nAnchor, Npp32s nDivisor, NppiBorderType eBorderType);

/**
 * Single channel 16-bit unsigned convolution 1D row filter with border control.
 * 
 *
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *        Coeffcients are expected to be stored in reverse order.
 * \param nMaskSize Width of the kernel.
 * \param nAnchor X offset of the kernel origin frame of reference
 *        relative to the source pixel.
 * \param nDivisor The factor by which the convolved summation from the Filter
 *        operation should be divided.  If equal to the sum of coefficients,
 *        this will keep the maximum result value within full scale.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterRowBorder_16u_C1R(const Npp16u * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp16u * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                            const Npp32s * pKernel, Npp32s nMaskSize, Npp32s nAnchor, Npp32s nDivisor, NppiBorderType eBorderType);
  
/**
 * Three channel 16-bit unsigned 1D row convolution filter with border control.
 * 
 *
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *        Coeffcients are expected to be stored in reverse order.
 * \param nMaskSize Width of the kernel.
 * \param nAnchor X offset of the kernel origin frame of reference
 *        relative to the source pixel.
 * \param nDivisor The factor by which the convolved summation from the Filter
 *        operation should be divided.  If equal to the sum of coefficients,
 *        this will keep the maximum result value within full scale.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterRowBorder_16u_C3R(const Npp16u * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp16u * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                            const Npp32s * pKernel, Npp32s nMaskSize, Npp32s nAnchor, Npp32s nDivisor, NppiBorderType eBorderType);
                 
/**
 * Four channel channel 16-bit 1D row unsigned convolution filter with border control.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *        Coeffcients are expected to be stored in reverse order.
 * \param nMaskSize Width of the kernel.
 * \param nAnchor X offset of the kernel origin frame of reference
 *        relative to the source pixel.
 * \param nDivisor The factor by which the convolved summation from the Filter
 *        operation should be divided.  If equal to the sum of coefficients,
 *        this will keep the maximum result value within full scale.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterRowBorder_16u_C4R(const Npp16u * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp16u * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                            const Npp32s * pKernel, Npp32s nMaskSize, Npp32s nAnchor, Npp32s nDivisor, NppiBorderType eBorderType);

/**
 * Four channel 16-bit unsigned 1D row convolution filter with border control, ignoring alpha channel.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *        Coeffcients are expected to be stored in reverse order.
 * \param nMaskSize Width of the kernel.
 * \param nAnchor X offset of the kernel origin frame of reference
 *        relative to the source pixel.
 * \param nDivisor The factor by which the convolved summation from the Filter
 *        operation should be divided.  If equal to the sum of coefficients,
 *        this will keep the maximum result value within full scale.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterRowBorder_16u_AC4R(const Npp16u * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp16u * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                             const Npp32s * pKernel, Npp32s nMaskSize, Npp32s nAnchor, Npp32s nDivisor, NppiBorderType eBorderType);

/**
 * Single channel 16-bit 1D row convolution filter with border control.
 * 
 *
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *        Coeffcients are expected to be stored in reverse order.
 * \param nMaskSize Width of the kernel.
 * \param nAnchor X offset of the kernel origin frame of reference
 *        relative to the source pixel.
 * \param nDivisor The factor by which the convolved summation from the Filter
 *        operation should be divided.  If equal to the sum of coefficients,
 *        this will keep the maximum result value within full scale.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterRowBorder_16s_C1R(const Npp16s * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp16s * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                            const Npp32s * pKernel, Npp32s nMaskSize, Npp32s nAnchor, Npp32s nDivisor, NppiBorderType eBorderType);
  
/**
 * Three channel 16-bit 1D row convolution filter with border control.
 * 
 *
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *        Coeffcients are expected to be stored in reverse order.
 * \param nMaskSize Width of the kernel.
 * \param nAnchor X offset of the kernel origin frame of reference
 *        relative to the source pixel.
 * \param nDivisor The factor by which the convolved summation from the Filter
 *        operation should be divided.  If equal to the sum of coefficients,
 *        this will keep the maximum result value within full scale.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterRowBorder_16s_C3R(const Npp16s * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp16s * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                            const Npp32s * pKernel, Npp32s nMaskSize, Npp32s nAnchor, Npp32s nDivisor, NppiBorderType eBorderType);
                 
/**
 * Four channel channel 16-bit 1D row convolution filter with border control.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *        Coeffcients are expected to be stored in reverse order.
 * \param nMaskSize Width of the kernel.
 * \param nAnchor X offset of the kernel origin frame of reference
 *        relative to the source pixel.
 * \param nDivisor The factor by which the convolved summation from the Filter
 *        operation should be divided.  If equal to the sum of coefficients,
 *        this will keep the maximum result value within full scale.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterRowBorder_16s_C4R(const Npp16s * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp16s * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                            const Npp32s * pKernel, Npp32s nMaskSize, Npp32s nAnchor, Npp32s nDivisor, NppiBorderType eBorderType);

/**
 * Four channel 16-bit 1D row convolution filter with border control, ignoring alpha channel.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *        Coeffcients are expected to be stored in reverse order.
 * \param nMaskSize Width of the kernel.
 * \param nAnchor X offset of the kernel origin frame of reference
 *        relative to the source pixel.
 * \param nDivisor The factor by which the convolved summation from the Filter
 *        operation should be divided.  If equal to the sum of coefficients,
 *        this will keep the maximum result value within full scale.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterRowBorder_16s_AC4R(const Npp16s * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp16s * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                             const Npp32s * pKernel, Npp32s nMaskSize, Npp32s nAnchor, Npp32s nDivisor, NppiBorderType eBorderType);
 
/**
 * Single channel 32-bit float 1D row convolution filter with border control.
 * 
 *
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *        Coeffcients are expected to be stored in reverse order.
 * \param nMaskSize Width of the kernel.
 * \param nAnchor X offset of the kernel origin frame of reference
 *        relative to the source pixel.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus
nppiFilterRowBorder_32f_C1R(const Npp32f * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp32f * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                            const Npp32f * pKernel, Npp32s nMaskSize, Npp32s nAnchor, NppiBorderType eBorderType);

/**
 * Three channel 32-bit float 1D row convolution filter with border control.
 * 
 *
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *        Coeffcients are expected to be stored in reverse order.
 * \param nMaskSize Width of the kernel.
 * \param nAnchor X offset of the kernel origin frame of reference
 *        relative to the source pixel.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus
nppiFilterRowBorder_32f_C3R(const Npp32f * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp32f * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                            const Npp32f * pKernel, Npp32s nMaskSize, Npp32s nAnchor, NppiBorderType eBorderType);

/**
 * Four channel 32-bit float 1D row convolution filter with border control.
 * 
 *
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *        Coeffcients are expected to be stored in reverse order.
 * \param nMaskSize Width of the kernel.
 * \param nAnchor X offset of the kernel origin frame of reference
 *        relative to the source pixel.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus
nppiFilterRowBorder_32f_C4R(const Npp32f * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp32f * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                            const Npp32f * pKernel, Npp32s nMaskSize, Npp32s nAnchor, NppiBorderType eBorderType);

/**
 * Four channel 32-bit float 1D row convolution filter with border control, ignoring alpha channel.
 * 
 *
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *        Coeffcients are expected to be stored in reverse order.
 * \param nMaskSize Width of the kernel.
 * \param nAnchor X offset of the kernel origin frame of reference
 *        relative to the source pixel.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus
nppiFilterRowBorder_32f_AC4R(const Npp32f * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp32f * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                             const Npp32f * pKernel, Npp32s nMaskSize, Npp32s nAnchor, NppiBorderType eBorderType);

/** @} FilterRowBorder */

/** @name FilterRow32f
 * 
 * FilterRow using floating-point weights.
 * 
 * @{
 *
 */

/**
 * 8-bit unsigned single-channel 1D row convolution.
 * 
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *                Coefficients are expected to be stored in reverse order.
 * \param nMaskSize Length of the linear kernel array.
 * \param nAnchor X offset of the kernel origin frame of reference relative to the
 *                 source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterRow32f_8u_C1R(const Npp8u * pSrc, Npp32s nSrcStep, Npp8u * pDst, Npp32s nDstStep, NppiSize oROI, 
                        const Npp32f * pKernel, Npp32s nMaskSize, Npp32s nAnchor);

/**
 * 8-bit unsigned three-channel 1D row convolution.
 * 
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *                Coefficients are expected to be stored in reverse order.
 * \param nMaskSize Length of the linear kernel array.
 * \param nAnchor X offset of the kernel origin frame of reference relative to the
 *                 source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterRow32f_8u_C3R(const Npp8u * pSrc, Npp32s nSrcStep, Npp8u * pDst, Npp32s nDstStep, NppiSize oROI, 
                        const Npp32f * pKernel, Npp32s nMaskSize, Npp32s nAnchor);

/**
 * 8-bit unsigned four-channel 1D row convolution.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *                Coefficients are expected to be stored in reverse order.
 * \param nMaskSize Length of the linear kernel array.
 * \param nAnchor X offset of the kernel origin frame of reference relative to the
 *                 source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterRow32f_8u_C4R(const Npp8u * pSrc, Npp32s nSrcStep, Npp8u * pDst, Npp32s nDstStep, NppiSize oROI, 
                        const Npp32f * pKernel, Npp32s nMaskSize, Npp32s nAnchor);

/**
 * 8-bit unsigned four-channel 1D row convolution ignoring alpha-channel.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *                Coefficients are expected to be stored in reverse order.
 * \param nMaskSize Length of the linear kernel array.
 * \param nAnchor X offset of the kernel origin frame of reference relative to the
 *                 source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterRow32f_8u_AC4R(const Npp8u * pSrc, Npp32s nSrcStep, Npp8u * pDst, Npp32s nDstStep, NppiSize oROI, 
                         const Npp32f * pKernel, Npp32s nMaskSize, Npp32s nAnchor);

/**
 * 16-bit unsigned single-channel 1D row convolution.
 * 
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *                Coefficients are expected to be stored in reverse order.
 * \param nMaskSize Length of the linear kernel array.
 * \param nAnchor X offset of the kernel origin frame of reference relative to the
 *                 source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterRow32f_16u_C1R(const Npp16u * pSrc, Npp32s nSrcStep, Npp16u * pDst, Npp32s nDstStep, NppiSize oROI, 
                         const Npp32f * pKernel, Npp32s nMaskSize, Npp32s nAnchor);

/**
 * 16-bit unsigned three-channel 1D row convolution.
 * 
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *                Coefficients are expected to be stored in reverse order.
 * \param nMaskSize Length of the linear kernel array.
 * \param nAnchor X offset of the kernel origin frame of reference relative to the
 *                 source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterRow32f_16u_C3R(const Npp16u * pSrc, Npp32s nSrcStep, Npp16u * pDst, Npp32s nDstStep, NppiSize oROI, 
                         const Npp32f * pKernel, Npp32s nMaskSize, Npp32s nAnchor);

/**
 * 16-bit unsigned four-channel 1D row convolution.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *                Coefficients are expected to be stored in reverse order.
 * \param nMaskSize Length of the linear kernel array.
 * \param nAnchor X offset of the kernel origin frame of reference relative to the
 *                 source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterRow32f_16u_C4R(const Npp16u * pSrc, Npp32s nSrcStep, Npp16u * pDst, Npp32s nDstStep, NppiSize oROI, 
                         const Npp32f * pKernel, Npp32s nMaskSize, Npp32s nAnchor);

/**
 * 16-bit unsigned four-channel 1D row convolution ignoring alpha-channel.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *                Coefficients are expected to be stored in reverse order.
 * \param nMaskSize Length of the linear kernel array.
 * \param nAnchor X offset of the kernel origin frame of reference relative to the
 *                 source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterRow32f_16u_AC4R(const Npp16u * pSrc, Npp32s nSrcStep, Npp16u * pDst, Npp32s nDstStep, NppiSize oROI, 
                          const Npp32f * pKernel, Npp32s nMaskSize, Npp32s nAnchor);

/**
 * 16-bit single-channel 1D row convolution.
 * 
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *                Coefficients are expected to be stored in reverse order.
 * \param nMaskSize Length of the linear kernel array.
 * \param nAnchor X offset of the kernel origin frame of reference relative to the
 *                 source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterRow32f_16s_C1R(const Npp16s * pSrc, Npp32s nSrcStep, Npp16s * pDst, Npp32s nDstStep, NppiSize oROI, 
                         const Npp32f * pKernel, Npp32s nMaskSize, Npp32s nAnchor);

/**
 * 16-bit three-channel 1D row convolution.
 * 
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *                Coefficients are expected to be stored in reverse order.
 * \param nMaskSize Length of the linear kernel array.
 * \param nAnchor X offset of the kernel origin frame of reference relative to the
 *                 source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterRow32f_16s_C3R(const Npp16s * pSrc, Npp32s nSrcStep, Npp16s * pDst, Npp32s nDstStep, NppiSize oROI, 
                         const Npp32f * pKernel, Npp32s nMaskSize, Npp32s nAnchor);

/**
 * 16-bit four-channel 1D row convolution.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *                Coefficients are expected to be stored in reverse order.
 * \param nMaskSize Length of the linear kernel array.
 * \param nAnchor X offset of the kernel origin frame of reference relative to the
 *                 source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterRow32f_16s_C4R(const Npp16s * pSrc, Npp32s nSrcStep, Npp16s * pDst, Npp32s nDstStep, NppiSize oROI, 
                         const Npp32f * pKernel, Npp32s nMaskSize, Npp32s nAnchor);

/**
 * 16-bit four-channel 1D row convolution ignoring alpha-channel.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *                Coefficients are expected to be stored in reverse order.
 * \param nMaskSize Length of the linear kernel array.
 * \param nAnchor X offset of the kernel origin frame of reference relative to the
 *                 source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterRow32f_16s_AC4R(const Npp16s * pSrc, Npp32s nSrcStep, Npp16s * pDst, Npp32s nDstStep, NppiSize oROI, 
                          const Npp32f * pKernel, Npp32s nMaskSize, Npp32s nAnchor);

/** @} FilterRow32f */

/** @defgroup image_1D_window_sum 1D Window Sum
 *
 * @{
 *
 */

/** @name 1D Window Sum
 *  1D mask Window Sum for 8 and 16 bit images.
 *
 * @{
 *
 */

/**
 * One channel 8-bit unsigned 1D (column) sum to 32f.
 *
 * Apply Column Window Summation filter over a 1D mask region around each
 * source pixel for 1-channel 8 bit/pixel input images with 32-bit floating point
 * output.  
 * Result 32-bit floating point pixel is equal to the sum of the corresponding and
 * neighboring column pixel values in a mask region of the source image defined by
 * nMaskSize and nAnchor. 
 *
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oROI \ref roi_specification.
 * \param nMaskSize Length of the linear kernel array.
 * \param nAnchor Y offset of the kernel origin frame of reference relative to the
 *        source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus nppiSumWindowColumn_8u32f_C1R(const Npp8u  * pSrc, Npp32s nSrcStep, 
                                              Npp32f * pDst, Npp32s nDstStep, NppiSize oROI, 
                                        Npp32s nMaskSize, Npp32s nAnchor);


/**
 * Three channel 8-bit unsigned 1D (column) sum to 32f.
 *
 * Apply Column Window Summation filter over a 1D mask region around each
 * source pixel for 3-channel 8 bit/pixel input images with 32-bit floating point
 * output.  
 * Result 32-bit floating point pixel is equal to the sum of the corresponding and
 * neighboring column pixel values in a mask region of the source image defined by
 * nMaskSize and nAnchor. 
 *
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oROI \ref roi_specification.
 * \param nMaskSize Length of the linear kernel array.
 * \param nAnchor Y offset of the kernel origin frame of reference relative to the
 *        source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus nppiSumWindowColumn_8u32f_C3R(const Npp8u  * pSrc, Npp32s nSrcStep, 
                                              Npp32f * pDst, Npp32s nDstStep, NppiSize oROI, 
                                        Npp32s nMaskSize, Npp32s nAnchor);


/**
 * Four channel 8-bit unsigned 1D (column) sum to 32f.
 *
 * Apply Column Window Summation filter over a 1D mask region around each
 * source pixel for 4-channel 8 bit/pixel input images with 32-bit floating point
 * output.  
 * Result 32-bit floating point pixel is equal to the sum of the corresponding and
 * neighboring column pixel values in a mask region of the source image defined by
 * nMaskSize and nAnchor. 
 *
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oROI \ref roi_specification.
 * \param nMaskSize Length of the linear kernel array.
 * \param nAnchor Y offset of the kernel origin frame of reference relative to the
 *        source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus nppiSumWindowColumn_8u32f_C4R(const Npp8u  * pSrc, Npp32s nSrcStep, 
                                              Npp32f * pDst, Npp32s nDstStep, NppiSize oROI, 
                                        Npp32s nMaskSize, Npp32s nAnchor);

/**
 * One channel 16-bit unsigned 1D (column) sum to 32f.
 *
 * Apply Column Window Summation filter over a 1D mask region around each
 * source pixel for 1-channel 16 bit/pixel input images with 32-bit floating point
 * output.  
 * Result 32-bit floating point pixel is equal to the sum of the corresponding and
 * neighboring column pixel values in a mask region of the source image defined by
 * nMaskSize and nAnchor. 
 *
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oROI \ref roi_specification.
 * \param nMaskSize Length of the linear kernel array.
 * \param nAnchor Y offset of the kernel origin frame of reference relative to the
 *        source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus nppiSumWindowColumn_16u32f_C1R(const Npp16u * pSrc, Npp32s nSrcStep, 
                                               Npp32f * pDst, Npp32s nDstStep, NppiSize oROI, 
                                         Npp32s nMaskSize, Npp32s nAnchor);

/**
 * Three channel 16-bit unsigned 1D (column) sum to 32f.
 *
 * Apply Column Window Summation filter over a 1D mask region around each
 * source pixel for 3-channel 16 bit/pixel input images with 32-bit floating point
 * output.  
 * Result 32-bit floating point pixel is equal to the sum of the corresponding and
 * neighboring column pixel values in a mask region of the source image defined by
 * nMaskSize and nAnchor. 
 *
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oROI \ref roi_specification.
 * \param nMaskSize Length of the linear kernel array.
 * \param nAnchor Y offset of the kernel origin frame of reference relative to the
 *        source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus nppiSumWindowColumn_16u32f_C3R(const Npp16u * pSrc, Npp32s nSrcStep, 
                                               Npp32f * pDst, Npp32s nDstStep, NppiSize oROI, 
                                         Npp32s nMaskSize, Npp32s nAnchor);

/**
 * Four channel 16-bit unsigned 1D (column) sum to 32f.
 *
 * Apply Column Window Summation filter over a 1D mask region around each
 * source pixel for 4-channel 16 bit/pixel input images with 32-bit floating point
 * output.  
 * Result 32-bit floating point pixel is equal to the sum of the corresponding and
 * neighboring column pixel values in a mask region of the source image defined by
 * nMaskSize and nAnchor. 
 *
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oROI \ref roi_specification.
 * \param nMaskSize Length of the linear kernel array.
 * \param nAnchor Y offset of the kernel origin frame of reference relative to the
 *        source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus nppiSumWindowColumn_16u32f_C4R(const Npp16u * pSrc, Npp32s nSrcStep, 
                                               Npp32f * pDst, Npp32s nDstStep, NppiSize oROI, 
                                         Npp32s nMaskSize, Npp32s nAnchor);

/**
 * One channel 16-bit signed 1D (column) sum to 32f.
 *
 * Apply Column Window Summation filter over a 1D mask region around each
 * source pixel for 1-channel 16 bit/pixel input images with 32-bit floating point
 * output.  
 * Result 32-bit floating point pixel is equal to the sum of the corresponding and
 * neighboring column pixel values in a mask region of the source image defined by
 * nMaskSize and nAnchor. 
 *
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oROI \ref roi_specification.
 * \param nMaskSize Length of the linear kernel array.
 * \param nAnchor Y offset of the kernel origin frame of reference relative to the
 *        source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus nppiSumWindowColumn_16s32f_C1R(const Npp16s * pSrc, Npp32s nSrcStep, 
                                               Npp32f * pDst, Npp32s nDstStep, NppiSize oROI, 
                                         Npp32s nMaskSize, Npp32s nAnchor);

/**
 * Three channel 16-bit signed 1D (column) sum to 32f.
 *
 * Apply Column Window Summation filter over a 1D mask region around each
 * source pixel for 1-channel 16 bit/pixel input images with 32-bit floating point
 * output.  
 * Result 32-bit floating point pixel is equal to the sum of the corresponding and
 * neighboring column pixel values in a mask region of the source image defined by
 * nMaskSize and nAnchor. 
 *
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oROI \ref roi_specification.
 * \param nMaskSize Length of the linear kernel array.
 * \param nAnchor Y offset of the kernel origin frame of reference relative to the
 *        source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus nppiSumWindowColumn_16s32f_C3R(const Npp16s * pSrc, Npp32s nSrcStep, 
                                               Npp32f * pDst, Npp32s nDstStep, NppiSize oROI, 
                                         Npp32s nMaskSize, Npp32s nAnchor);

/**
 * Four channel 16-bit signed 1D (column) sum to 32f.
 *
 * Apply Column Window Summation filter over a 1D mask region around each
 * source pixel for 4-channel 16 bit/pixel input images with 32-bit floating point
 * output.  
 * Result 32-bit floating point pixel is equal to the sum of the corresponding and
 * neighboring column pixel values in a mask region of the source image defined by
 * nMaskSize and nAnchor. 
 *
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oROI \ref roi_specification.
 * \param nMaskSize Length of the linear kernel array.
 * \param nAnchor Y offset of the kernel origin frame of reference relative to the
 *        source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus nppiSumWindowColumn_16s32f_C4R(const Npp16s * pSrc, Npp32s nSrcStep, 
                                               Npp32f * pDst, Npp32s nDstStep, NppiSize oROI, 
                                         Npp32s nMaskSize, Npp32s nAnchor);

/**
 * One channel 8-bit unsigned 1D (row) sum to 32f.
 *
 * Apply Row Window Summation filter over a 1D mask region around each source
 * pixel for 1-channel 8-bit pixel input images with 32-bit floating point output.  
 * Result 32-bit floating point pixel is equal to the sum of the corresponding and
 * neighboring row pixel values in a mask region of the source image defined
 * by iKernelDim and iAnchorX. 
 *
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oROI \ref roi_specification.
 * \param nMaskSize Length of the linear kernel array.
 * \param nAnchor X offset of the kernel origin frame of reference relative to the
 *        source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiSumWindowRow_8u32f_C1R(const Npp8u  * pSrc, Npp32s nSrcStep, 
                                 Npp32f * pDst, Npp32s nDstStep, 
                           NppiSize oROI, Npp32s nMaskSize, Npp32s nAnchor);

/**
 * Three channel 8-bit unsigned 1D (row) sum to 32f.
 *
 * Apply Row Window Summation filter over a 1D mask region around each source
 * pixel for 3-channel 8-bit pixel input images with 32-bit floating point output.  
 * Result 32-bit floating point pixel is equal to the sum of the corresponding and
 * neighboring row pixel values in a mask region of the source image defined
 * by iKernelDim and iAnchorX. 
 *
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oROI \ref roi_specification.
 * \param nMaskSize Length of the linear kernel array.
 * \param nAnchor X offset of the kernel origin frame of reference relative to the
 *        source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiSumWindowRow_8u32f_C3R(const Npp8u  * pSrc, Npp32s nSrcStep, 
                                 Npp32f * pDst, Npp32s nDstStep, 
                           NppiSize oROI, Npp32s nMaskSize, Npp32s nAnchor);

/**
 * Four channel 8-bit unsigned 1D (row) sum to 32f.
 *
 * Apply Row Window Summation filter over a 1D mask region around each source
 * pixel for 4-channel 8-bit pixel input images with 32-bit floating point output.  
 * Result 32-bit floating point pixel is equal to the sum of the corresponding and
 * neighboring row pixel values in a mask region of the source image defined
 * by iKernelDim and iAnchorX. 
 *
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oROI \ref roi_specification.
 * \param nMaskSize Length of the linear kernel array.
 * \param nAnchor X offset of the kernel origin frame of reference relative to the
 *        source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiSumWindowRow_8u32f_C4R(const Npp8u  * pSrc, Npp32s nSrcStep, 
                                 Npp32f * pDst, Npp32s nDstStep, 
                           NppiSize oROI, Npp32s nMaskSize, Npp32s nAnchor);

/**
 * One channel 16-bit unsigned 1D (row) sum to 32f.
 *
 * Apply Row Window Summation filter over a 1D mask region around each source
 * pixel for 1-channel 16-bit pixel input images with 32-bit floating point output.  
 * Result 32-bit floating point pixel is equal to the sum of the corresponding and
 * neighboring row pixel values in a mask region of the source image defined
 * by iKernelDim and iAnchorX. 
 *
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oROI \ref roi_specification.
 * \param nMaskSize Length of the linear kernel array.
 * \param nAnchor X offset of the kernel origin frame of reference relative to the
 *        source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiSumWindowRow_16u32f_C1R(const Npp16u * pSrc, Npp32s nSrcStep, 
                                  Npp32f * pDst, Npp32s nDstStep, 
                            NppiSize oROI, Npp32s nMaskSize, Npp32s nAnchor);

/**
 * Three channel 16-bit unsigned 1D (row) sum to 32f.
 *
 * Apply Row Window Summation filter over a 1D mask region around each source
 * pixel for 3-channel 16-bit pixel input images with 32-bit floating point output.  
 * Result 32-bit floating point pixel is equal to the sum of the corresponding and
 * neighboring row pixel values in a mask region of the source image defined
 * by iKernelDim and iAnchorX. 
 *
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oROI \ref roi_specification.
 * \param nMaskSize Length of the linear kernel array.
 * \param nAnchor X offset of the kernel origin frame of reference relative to the
 *        source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiSumWindowRow_16u32f_C3R(const Npp16u * pSrc, Npp32s nSrcStep, 
                                  Npp32f * pDst, Npp32s nDstStep, 
                            NppiSize oROI, Npp32s nMaskSize, Npp32s nAnchor);

/**
 * Four channel 16-bit unsigned 1D (row) sum to 32f.
 *
 * Apply Row Window Summation filter over a 1D mask region around each source
 * pixel for 4-channel 16-bit pixel input images with 32-bit floating point output.  
 * Result 32-bit floating point pixel is equal to the sum of the corresponding and
 * neighboring row pixel values in a mask region of the source image defined
 * by iKernelDim and iAnchorX. 
 *
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oROI \ref roi_specification.
 * \param nMaskSize Length of the linear kernel array.
 * \param nAnchor X offset of the kernel origin frame of reference relative to the
 *        source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiSumWindowRow_16u32f_C4R(const Npp16u * pSrc, Npp32s nSrcStep, 
                                  Npp32f * pDst, Npp32s nDstStep, 
                            NppiSize oROI, Npp32s nMaskSize, Npp32s nAnchor);

/**
 * One channel 16-bit signed 1D (row) sum to 32f.
 *
 * Apply Row Window Summation filter over a 1D mask region around each source
 * pixel for 1-channel 16-bit pixel input images with 32-bit floating point output.  
 * Result 32-bit floating point pixel is equal to the sum of the corresponding and
 * neighboring row pixel values in a mask region of the source image defined
 * by iKernelDim and iAnchorX. 
 *
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oROI \ref roi_specification.
 * \param nMaskSize Length of the linear kernel array.
 * \param nAnchor X offset of the kernel origin frame of reference relative to the
 *        source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiSumWindowRow_16s32f_C1R(const Npp16s * pSrc, Npp32s nSrcStep, 
                                  Npp32f * pDst, Npp32s nDstStep, 
                            NppiSize oROI, Npp32s nMaskSize, Npp32s nAnchor);
/**
 * Three channel 16-bit signed 1D (row) sum to 32f.
 *
 * Apply Row Window Summation filter over a 1D mask region around each source
 * pixel for 3-channel 16-bit pixel input images with 32-bit floating point output.  
 * Result 32-bit floating point pixel is equal to the sum of the corresponding and
 * neighboring row pixel values in a mask region of the source image defined
 * by iKernelDim and iAnchorX. 
 *
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oROI \ref roi_specification.
 * \param nMaskSize Length of the linear kernel array.
 * \param nAnchor X offset of the kernel origin frame of reference relative to the
 *        source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiSumWindowRow_16s32f_C3R(const Npp16s * pSrc, Npp32s nSrcStep, 
                                  Npp32f * pDst, Npp32s nDstStep, 
                            NppiSize oROI, Npp32s nMaskSize, Npp32s nAnchor);

/**
 * Four channel 16-bit signed 1D (row) sum to 32f.
 *
 * Apply Row Window Summation filter over a 1D mask region around each source
 * pixel for 4-channel 16-bit pixel input images with 32-bit floating point output.  
 * Result 32-bit floating point pixel is equal to the sum of the corresponding and
 * neighboring row pixel values in a mask region of the source image defined
 * by iKernelDim and iAnchorX. 
 *
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oROI \ref roi_specification.
 * \param nMaskSize Length of the linear kernel array.
 * \param nAnchor X offset of the kernel origin frame of reference relative to the
 *        source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiSumWindowRow_16s32f_C4R(const Npp16s * pSrc, Npp32s nSrcStep, 
                                  Npp32f * pDst, Npp32s nDstStep, 
                            NppiSize oROI, Npp32s nMaskSize, Npp32s nAnchor);
/** @} */

/** @} image_1D_window_sum */

/** @defgroup image_convolution Convolution
 *
 * @{
 *
 */

/** @name Filter
 * General purpose 2D convolution filter.
 *
 * Pixels under the mask are multiplied by the respective weights in the mask
 * and the results are summed. Before writing the result pixel the sum is scaled
 * back via division by nDivisor.
 *
 * @{
 *
 */

/**
 * Single channel 8-bit unsigned convolution filter.
 * 
 *
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *        Coeffcients are expected to be stored in reverse order.
 * \param oKernelSize Width and Height of the rectangular kernel.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \param nDivisor The factor by which the convolved summation from the Filter
 *        operation should be divided.  If equal to the sum of coefficients,
 *        this will keep the maximum result value within full scale.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilter_8u_C1R(const Npp8u * pSrc, Npp32s nSrcStep, Npp8u * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                  const Npp32s * pKernel, NppiSize oKernelSize, NppiPoint oAnchor, Npp32s nDivisor);
  
/**
 * Three channel 8-bit unsigned convolution filter.
 * 
 *
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *        Coeffcients are expected to be stored in reverse order.
 * \param oKernelSize Width and Height of the rectangular kernel.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \param nDivisor The factor by which the convolved summation from the Filter
 *        operation should be divided.  If equal to the sum of coefficients,
 *        this will keep the maximum result value within full scale.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilter_8u_C3R(const Npp8u * pSrc, Npp32s nSrcStep, Npp8u * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                  const Npp32s * pKernel, NppiSize oKernelSize, NppiPoint oAnchor, Npp32s nDivisor);
                 
/**
 * Four channel channel 8-bit unsigned convolution filter.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *        Coeffcients are expected to be stored in reverse order.
 * \param oKernelSize Width and Height of the rectangular kernel.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \param nDivisor The factor by which the convolved summation from the Filter
 *        operation should be divided.  If equal to the sum of coefficients,
 *        this will keep the maximum result value within full scale.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilter_8u_C4R(const Npp8u * pSrc, Npp32s nSrcStep, Npp8u * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                  const Npp32s * pKernel, NppiSize oKernelSize, NppiPoint oAnchor, Npp32s nDivisor);

/**
 * Four channel 8-bit unsigned convolution filter, ignoring alpha channel.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *        Coeffcients are expected to be stored in reverse order.
 * \param oKernelSize Width and Height of the rectangular kernel.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \param nDivisor The factor by which the convolved summation from the Filter
 *        operation should be divided.  If equal to the sum of coefficients,
 *        this will keep the maximum result value within full scale.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilter_8u_AC4R(const Npp8u * pSrc, Npp32s nSrcStep, Npp8u * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                   const Npp32s * pKernel, NppiSize oKernelSize, NppiPoint oAnchor, Npp32s nDivisor);

/**
 * Single channel 16-bit unsigned convolution filter.
 * 
 *
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *        Coeffcients are expected to be stored in reverse order.
 * \param oKernelSize Width and Height of the rectangular kernel.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \param nDivisor The factor by which the convolved summation from the Filter
 *        operation should be divided.  If equal to the sum of coefficients,
 *        this will keep the maximum result value within full scale.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilter_16u_C1R(const Npp16u * pSrc, Npp32s nSrcStep, Npp16u * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                   const Npp32s * pKernel, NppiSize oKernelSize, NppiPoint oAnchor, Npp32s nDivisor);
  
/**
 * Three channel 16-bit unsigned convolution filter.
 * 
 *
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *        Coeffcients are expected to be stored in reverse order.
 * \param oKernelSize Width and Height of the rectangular kernel.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \param nDivisor The factor by which the convolved summation from the Filter
 *        operation should be divided.  If equal to the sum of coefficients,
 *        this will keep the maximum result value within full scale.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilter_16u_C3R(const Npp16u * pSrc, Npp32s nSrcStep, Npp16u * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                   const Npp32s * pKernel, NppiSize oKernelSize, NppiPoint oAnchor, Npp32s nDivisor);
                 
/**
 * Four channel channel 16-bit unsigned convolution filter.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *        Coeffcients are expected to be stored in reverse order.
 * \param oKernelSize Width and Height of the rectangular kernel.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \param nDivisor The factor by which the convolved summation from the Filter
 *        operation should be divided.  If equal to the sum of coefficients,
 *        this will keep the maximum result value within full scale.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilter_16u_C4R(const Npp16u * pSrc, Npp32s nSrcStep, Npp16u * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                   const Npp32s * pKernel, NppiSize oKernelSize, NppiPoint oAnchor, Npp32s nDivisor);

/**
 * Four channel 16-bit unsigned convolution filter, ignoring alpha channel.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *        Coeffcients are expected to be stored in reverse order.
 * \param oKernelSize Width and Height of the rectangular kernel.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \param nDivisor The factor by which the convolved summation from the Filter
 *        operation should be divided.  If equal to the sum of coefficients,
 *        this will keep the maximum result value within full scale.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilter_16u_AC4R(const Npp16u * pSrc, Npp32s nSrcStep, Npp16u * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                   const Npp32s * pKernel, NppiSize oKernelSize, NppiPoint oAnchor, Npp32s nDivisor);

/**
 * Single channel 16-bit convolution filter.
 * 
 *
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *        Coeffcients are expected to be stored in reverse order.
 * \param oKernelSize Width and Height of the rectangular kernel.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \param nDivisor The factor by which the convolved summation from the Filter
 *        operation should be divided.  If equal to the sum of coefficients,
 *        this will keep the maximum result value within full scale.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilter_16s_C1R(const Npp16s * pSrc, Npp32s nSrcStep, Npp16s * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                   const Npp32s * pKernel, NppiSize oKernelSize, NppiPoint oAnchor, Npp32s nDivisor);
  
/**
 * Three channel 16-bit convolution filter.
 * 
 *
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *        Coeffcients are expected to be stored in reverse order.
 * \param oKernelSize Width and Height of the rectangular kernel.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \param nDivisor The factor by which the convolved summation from the Filter
 *        operation should be divided.  If equal to the sum of coefficients,
 *        this will keep the maximum result value within full scale.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilter_16s_C3R(const Npp16s * pSrc, Npp32s nSrcStep, Npp16s * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                   const Npp32s * pKernel, NppiSize oKernelSize, NppiPoint oAnchor, Npp32s nDivisor);
                 
/**
 * Four channel channel 16-bit convolution filter.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *        Coeffcients are expected to be stored in reverse order.
 * \param oKernelSize Width and Height of the rectangular kernel.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \param nDivisor The factor by which the convolved summation from the Filter
 *        operation should be divided.  If equal to the sum of coefficients,
 *        this will keep the maximum result value within full scale.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilter_16s_C4R(const Npp16s * pSrc, Npp32s nSrcStep, Npp16s * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                   const Npp32s * pKernel, NppiSize oKernelSize, NppiPoint oAnchor, Npp32s nDivisor);

/**
 * Four channel 16-bit convolution filter, ignoring alpha channel.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *        Coeffcients are expected to be stored in reverse order.
 * \param oKernelSize Width and Height of the rectangular kernel.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \param nDivisor The factor by which the convolved summation from the Filter
 *        operation should be divided.  If equal to the sum of coefficients,
 *        this will keep the maximum result value within full scale.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilter_16s_AC4R(const Npp16s * pSrc, Npp32s nSrcStep, Npp16s * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                    const Npp32s * pKernel, NppiSize oKernelSize, NppiPoint oAnchor, Npp32s nDivisor);
 
/**
 * Single channel 32-bit float convolution filter.
 * 
 *
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *        Coeffcients are expected to be stored in reverse order.
 * \param oKernelSize Width and Height of the rectangular kernel.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus
nppiFilter_32f_C1R(const Npp32f * pSrc, Npp32s nSrcStep, Npp32f * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                   const Npp32f * pKernel, NppiSize oKernelSize, NppiPoint oAnchor);

/**
 * Two channel 32-bit float convolution filter.
 * 
 *
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *        Coeffcients are expected to be stored in reverse order.
 * \param oKernelSize Width and Height of the rectangular kernel.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus
nppiFilter_32f_C2R(const Npp32f * pSrc, Npp32s nSrcStep, Npp32f * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                   const Npp32f * pKernel, NppiSize oKernelSize, NppiPoint oAnchor);

/**
 * Three channel 32-bit float convolution filter.
 * 
 *
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *        Coeffcients are expected to be stored in reverse order.
 * \param oKernelSize Width and Height of the rectangular kernel.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus
nppiFilter_32f_C3R(const Npp32f * pSrc, Npp32s nSrcStep, Npp32f * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                   const Npp32f * pKernel, NppiSize oKernelSize, NppiPoint oAnchor);

/**
 * Four channel 32-bit float convolution filter.
 * 
 *
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *        Coeffcients are expected to be stored in reverse order.
 * \param oKernelSize Width and Height of the rectangular kernel.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus
nppiFilter_32f_C4R(const Npp32f * pSrc, Npp32s nSrcStep, Npp32f * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                   const Npp32f * pKernel, NppiSize oKernelSize, NppiPoint oAnchor);

/**
 * Four channel 32-bit float convolution filter, ignoring alpha channel.
 * 
 *
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *        Coeffcients are expected to be stored in reverse order.
 * \param oKernelSize Width and Height of the rectangular kernel.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus
nppiFilter_32f_AC4R(const Npp32f * pSrc, Npp32s nSrcStep, Npp32f * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                    const Npp32f * pKernel, NppiSize oKernelSize, NppiPoint oAnchor);

/**
 * Single channel 64-bit float convolution filter.
 * 
 *
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *        Coeffcients are expected to be stored in reverse order.
 * \param oKernelSize Width and Height of the rectangular kernel.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus
nppiFilter_64f_C1R(const Npp64f * pSrc, Npp32s nSrcStep, Npp64f * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                   const Npp64f * pKernel, NppiSize oKernelSize, NppiPoint oAnchor);


/** @} Filter */

/** @name Filter32f
 * General purpose 2D convolution filter using floating-point weights.
 *
 * Pixels under the mask are multiplied by the respective weights in the mask
 * and the results are summed. 
 *
 * @{
 *
 */


/**
 * Single channel 8-bit unsigned convolution filter.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *        Coeffcients are expected to be stored in reverse order.
 * \param oKernelSize Width and Height of the rectangular kernel.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus
nppiFilter32f_8u_C1R(const Npp8u * pSrc, int nSrcStep, Npp8u * pDst, int nDstStep, NppiSize oSizeROI, 
                     const Npp32f * pKernel, NppiSize oKernelSize, NppiPoint oAnchor);
        
/**
 * Two channel 8-bit unsigned convolution filter.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *        Coeffcients are expected to be stored in reverse order.
 * \param oKernelSize Width and Height of the rectangular kernel.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus
nppiFilter32f_8u_C2R(const Npp8u * pSrc, int nSrcStep, Npp8u * pDst, int nDstStep, NppiSize oSizeROI, 
                     const Npp32f * pKernel, NppiSize oKernelSize, NppiPoint oAnchor);
        
/**
 * Three channel 8-bit unsigned convolution filter.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *        Coeffcients are expected to be stored in reverse order.
 * \param oKernelSize Width and Height of the rectangular kernel.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus
nppiFilter32f_8u_C3R(const Npp8u * pSrc, int nSrcStep, Npp8u * pDst, int nDstStep, NppiSize oSizeROI, 
                     const Npp32f * pKernel, NppiSize oKernelSize, NppiPoint oAnchor);
        
/**
 * Four channel 8-bit unsigned convolution filter.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *        Coeffcients are expected to be stored in reverse order.
 * \param oKernelSize Width and Height of the rectangular kernel.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus
nppiFilter32f_8u_C4R(const Npp8u * pSrc, int nSrcStep, Npp8u * pDst, int nDstStep, NppiSize oSizeROI, 
                     const Npp32f * pKernel, NppiSize oKernelSize, NppiPoint oAnchor);
        
/**
 * Four channel 8-bit unsigned convolution filter, ignorint alpha channel.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *        Coeffcients are expected to be stored in reverse order.
 * \param oKernelSize Width and Height of the rectangular kernel.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus
nppiFilter32f_8u_AC4R(const Npp8u * pSrc, int nSrcStep, Npp8u * pDst, int nDstStep, NppiSize oSizeROI, 
                      const Npp32f * pKernel, NppiSize oKernelSize, NppiPoint oAnchor);

/**
 * Single channel 8-bit signed convolution filter.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *        Coeffcients are expected to be stored in reverse order.
 * \param oKernelSize Width and Height of the rectangular kernel.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus
nppiFilter32f_8s_C1R(const Npp8s * pSrc, int nSrcStep, Npp8s * pDst, int nDstStep, NppiSize oSizeROI, 
                     const Npp32f * pKernel, NppiSize oKernelSize, NppiPoint oAnchor);

/**
 * Two channel 8-bit signed convolution filter.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *        Coeffcients are expected to be stored in reverse order.
 * \param oKernelSize Width and Height of the rectangular kernel.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus
nppiFilter32f_8s_C2R(const Npp8s * pSrc, int nSrcStep, Npp8s * pDst, int nDstStep, NppiSize oSizeROI, 
                     const Npp32f * pKernel, NppiSize oKernelSize, NppiPoint oAnchor);

/**
 * Three channel 8-bit signed convolution filter.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *        Coeffcients are expected to be stored in reverse order.
 * \param oKernelSize Width and Height of the rectangular kernel.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus
nppiFilter32f_8s_C3R(const Npp8s * pSrc, int nSrcStep, Npp8s * pDst, int nDstStep, NppiSize oSizeROI, 
                     const Npp32f * pKernel, NppiSize oKernelSize, NppiPoint oAnchor);

/**
 * Four channel 8-bit signed convolution filter.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *        Coeffcients are expected to be stored in reverse order.
 * \param oKernelSize Width and Height of the rectangular kernel.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus
nppiFilter32f_8s_C4R(const Npp8s * pSrc, int nSrcStep, Npp8s * pDst, int nDstStep, NppiSize oSizeROI, 
                     const Npp32f * pKernel, NppiSize oKernelSize, NppiPoint oAnchor);

/**
 * Four channel 8-bit signed convolution filter, ignoring alpha channel.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *        Coeffcients are expected to be stored in reverse order.
 * \param oKernelSize Width and Height of the rectangular kernel.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus
nppiFilter32f_8s_AC4R(const Npp8s * pSrc, int nSrcStep, Npp8s * pDst, int nDstStep, NppiSize oSizeROI, 
                      const Npp32f * pKernel, NppiSize oKernelSize, NppiPoint oAnchor);

/**
 * Single channel 16-bit unsigned convolution filter.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *        Coeffcients are expected to be stored in reverse order.
 * \param oKernelSize Width and Height of the rectangular kernel.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus
nppiFilter32f_16u_C1R(const Npp16u * pSrc, int nSrcStep, Npp16u * pDst, int nDstStep, NppiSize oSizeROI, 
                      const Npp32f * pKernel, NppiSize oKernelSize, NppiPoint oAnchor);

/**
 * Three channel 16-bit unsigned convolution filter.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *        Coeffcients are expected to be stored in reverse order.
 * \param oKernelSize Width and Height of the rectangular kernel.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus
nppiFilter32f_16u_C3R(const Npp16u * pSrc, int nSrcStep, Npp16u * pDst, int nDstStep, NppiSize oSizeROI, 
                      const Npp32f * pKernel, NppiSize oKernelSize, NppiPoint oAnchor);

/**
 * Four channel 16-bit unsigned convolution filter.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *        Coeffcients are expected to be stored in reverse order.
 * \param oKernelSize Width and Height of the rectangular kernel.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus
nppiFilter32f_16u_C4R(const Npp16u * pSrc, int nSrcStep, Npp16u * pDst, int nDstStep, NppiSize oSizeROI, 
                      const Npp32f * pKernel, NppiSize oKernelSize, NppiPoint oAnchor);

/**
 * Four channel 16-bit unsigned convolution filter, ignoring alpha channel.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *        Coeffcients are expected to be stored in reverse order.
 * \param oKernelSize Width and Height of the rectangular kernel.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus
nppiFilter32f_16u_AC4R(const Npp16u * pSrc, int nSrcStep, Npp16u * pDst, int nDstStep, NppiSize oSizeROI, 
                       const Npp32f * pKernel, NppiSize oKernelSize, NppiPoint oAnchor);

/**
 * Single channel 16-bit convolution filter.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *        Coeffcients are expected to be stored in reverse order.
 * \param oKernelSize Width and Height of the rectangular kernel.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus
nppiFilter32f_16s_C1R(const Npp16s * pSrc, int nSrcStep, Npp16s * pDst, int nDstStep, NppiSize oSizeROI, 
                      const Npp32f * pKernel, NppiSize oKernelSize, NppiPoint oAnchor);

/**
 * Three channel 16-bit convolution filter.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *        Coeffcients are expected to be stored in reverse order.
 * \param oKernelSize Width and Height of the rectangular kernel.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus
nppiFilter32f_16s_C3R(const Npp16s * pSrc, int nSrcStep, Npp16s * pDst, int nDstStep, NppiSize oSizeROI, 
                      const Npp32f * pKernel, NppiSize oKernelSize, NppiPoint oAnchor);

/**
 * Four channel 16-bit convolution filter.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *        Coeffcients are expected to be stored in reverse order.
 * \param oKernelSize Width and Height of the rectangular kernel.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus
nppiFilter32f_16s_C4R(const Npp16s * pSrc, int nSrcStep, Npp16s * pDst, int nDstStep, NppiSize oSizeROI, 
                      const Npp32f * pKernel, NppiSize oKernelSize, NppiPoint oAnchor);

/**
 * Four channel 16-bit convolution filter, ignoring alpha channel.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *        Coeffcients are expected to be stored in reverse order.
 * \param oKernelSize Width and Height of the rectangular kernel.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus
nppiFilter32f_16s_AC4R(const Npp16s * pSrc, int nSrcStep, Npp16s * pDst, int nDstStep, NppiSize oSizeROI, 
                       const Npp32f * pKernel, NppiSize oKernelSize, NppiPoint oAnchor);


/**
 * Single channel 32-bit convolution filter.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *        Coeffcients are expected to be stored in reverse order.
 * \param oKernelSize Width and Height of the rectangular kernel.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus
nppiFilter32f_32s_C1R(const Npp32s * pSrc, int nSrcStep, Npp32s * pDst, int nDstStep, NppiSize oSizeROI, 
                      const Npp32f * pKernel, NppiSize oKernelSize, NppiPoint oAnchor);

/**
 * Three channel 32-bit convolution filter.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *        Coeffcients are expected to be stored in reverse order.
 * \param oKernelSize Width and Height of the rectangular kernel.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus
nppiFilter32f_32s_C3R(const Npp32s * pSrc, int nSrcStep, Npp32s * pDst, int nDstStep, NppiSize oSizeROI, 
                      const Npp32f * pKernel, NppiSize oKernelSize, NppiPoint oAnchor);

/**
 * Four channel 32-bit convolution filter.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *        Coeffcients are expected to be stored in reverse order.
 * \param oKernelSize Width and Height of the rectangular kernel.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus
nppiFilter32f_32s_C4R(const Npp32s * pSrc, int nSrcStep, Npp32s * pDst, int nDstStep, NppiSize oSizeROI, 
                      const Npp32f * pKernel, NppiSize oKernelSize, NppiPoint oAnchor);

/**
 * Four channel 32-bit convolution filter, ignoring alpha channel.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *        Coeffcients are expected to be stored in reverse order.
 * \param oKernelSize Width and Height of the rectangular kernel.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus
nppiFilter32f_32s_AC4R(const Npp32s * pSrc, int nSrcStep, Npp32s * pDst, int nDstStep, NppiSize oSizeROI, 
                       const Npp32f * pKernel, NppiSize oKernelSize, NppiPoint oAnchor);


/**
 * Single channel 8-bit unsigned to 16-bit signed convolution filter.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *        Coeffcients are expected to be stored in reverse order.
 * \param oKernelSize Width and Height of the rectangular kernel.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus
nppiFilter32f_8u16s_C1R(const Npp8u * pSrc, int nSrcStep, Npp16s * pDst, int nDstStep, NppiSize oSizeROI, 
                        const Npp32f * pKernel, NppiSize oKernelSize, NppiPoint oAnchor);

/**
 * Three channel 8-bit unsigned to 16-bit signed convolution filter.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *        Coeffcients are expected to be stored in reverse order.
 * \param oKernelSize Width and Height of the rectangular kernel.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus
nppiFilter32f_8u16s_C3R(const Npp8u * pSrc, int nSrcStep, Npp16s * pDst, int nDstStep, NppiSize oSizeROI, 
                        const Npp32f * pKernel, NppiSize oKernelSize, NppiPoint oAnchor);

/**
 * Four channel 8-bit unsigned to 16-bit signed convolution filter.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *        Coeffcients are expected to be stored in reverse order.
 * \param oKernelSize Width and Height of the rectangular kernel.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus
nppiFilter32f_8u16s_C4R(const Npp8u * pSrc, int nSrcStep, Npp16s * pDst, int nDstStep, NppiSize oSizeROI, 
                        const Npp32f * pKernel, NppiSize oKernelSize, NppiPoint oAnchor);

/**
 * Four channel 8-bit unsigned to 16-bit signed convolution filter, ignoring alpha channel.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *        Coeffcients are expected to be stored in reverse order.
 * \param oKernelSize Width and Height of the rectangular kernel.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus
nppiFilter32f_8u16s_AC4R(const Npp8u * pSrc, int nSrcStep, Npp16s * pDst, int nDstStep, NppiSize oSizeROI, 
                         const Npp32f * pKernel, NppiSize oKernelSize, NppiPoint oAnchor);

/**
 * Single channel 8-bit to 16-bit signed convolution filter.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *        Coeffcients are expected to be stored in reverse order.
 * \param oKernelSize Width and Height of the rectangular kernel.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus
nppiFilter32f_8s16s_C1R(const Npp8s * pSrc, int nSrcStep, Npp16s * pDst, int nDstStep, NppiSize oSizeROI, 
                        const Npp32f * pKernel, NppiSize oKernelSize, NppiPoint oAnchor);

/**
 * Three channel 8-bit to 16-bit signed convolution filter.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *        Coeffcients are expected to be stored in reverse order.
 * \param oKernelSize Width and Height of the rectangular kernel.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus
nppiFilter32f_8s16s_C3R(const Npp8s * pSrc, int nSrcStep, Npp16s * pDst, int nDstStep, NppiSize oSizeROI, 
                        const Npp32f * pKernel, NppiSize oKernelSize, NppiPoint oAnchor);

/**
 * Four channel 8-bit to 16-bit signed convolution filter.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *        Coeffcients are expected to be stored in reverse order.
 * \param oKernelSize Width and Height of the rectangular kernel.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus
nppiFilter32f_8s16s_C4R(const Npp8s * pSrc, int nSrcStep, Npp16s * pDst, int nDstStep, NppiSize oSizeROI, 
                        const Npp32f * pKernel, NppiSize oKernelSize, NppiPoint oAnchor);

/**
 * Four channel 8-bit to 16-bit signed convolution filter, ignoring alpha channel.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *        Coeffcients are expected to be stored in reverse order.
 * \param oKernelSize Width and Height of the rectangular kernel.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus
nppiFilter32f_8s16s_AC4R(const Npp8s * pSrc, int nSrcStep, Npp16s * pDst, int nDstStep, NppiSize oSizeROI, 
                         const Npp32f * pKernel, NppiSize oKernelSize, NppiPoint oAnchor);


/** @} Filter32f */

/** @name FilterBorder
 * General purpose 2D convolution filter with border control.
 *
 * Pixels under the mask are multiplied by the respective weights in the mask
 * and the results are summed. Before writing the result pixel the sum is scaled
 * back via division by nDivisor. If any portion of the mask overlaps the source
 * image boundary the requested border type operation is applied to all mask pixels
 * which fall outside of the source image.
 *
 * Currently only the NPP_BORDER_REPLICATE border type operation is supported.
 *
 * @{
 *
 */

/**
 * Single channel 8-bit unsigned convolution filter with border control.
 * 
 *
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *        Coeffcients are expected to be stored in reverse order.
 * \param oKernelSize Width and Height of the rectangular kernel.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \param nDivisor The factor by which the convolved summation from the Filter
 *        operation should be divided.  If equal to the sum of coefficients,
 *        this will keep the maximum result value within full scale.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterBorder_8u_C1R(const Npp8u * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp8u * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                        const Npp32s * pKernel, NppiSize oKernelSize, NppiPoint oAnchor, Npp32s nDivisor, NppiBorderType eBorderType);
  
/**
 * Three channel 8-bit unsigned convolution filter with border control.
 * 
 *
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *        Coeffcients are expected to be stored in reverse order.
 * \param oKernelSize Width and Height of the rectangular kernel.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \param nDivisor The factor by which the convolved summation from the Filter
 *        operation should be divided.  If equal to the sum of coefficients,
 *        this will keep the maximum result value within full scale.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterBorder_8u_C3R(const Npp8u * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp8u * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                        const Npp32s * pKernel, NppiSize oKernelSize, NppiPoint oAnchor, Npp32s nDivisor, NppiBorderType eBorderType);
                 
/**
 * Four channel channel 8-bit unsigned convolution filter with border control.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *        Coeffcients are expected to be stored in reverse order.
 * \param oKernelSize Width and Height of the rectangular kernel.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \param nDivisor The factor by which the convolved summation from the Filter
 *        operation should be divided.  If equal to the sum of coefficients,
 *        this will keep the maximum result value within full scale.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterBorder_8u_C4R(const Npp8u * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp8u * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                        const Npp32s * pKernel, NppiSize oKernelSize, NppiPoint oAnchor, Npp32s nDivisor, NppiBorderType eBorderType);

/**
 * Four channel 8-bit unsigned convolution filter with border control, ignoring alpha channel.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *        Coeffcients are expected to be stored in reverse order.
 * \param oKernelSize Width and Height of the rectangular kernel.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \param nDivisor The factor by which the convolved summation from the Filter
 *        operation should be divided.  If equal to the sum of coefficients,
 *        this will keep the maximum result value within full scale.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterBorder_8u_AC4R(const Npp8u * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp8u * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                         const Npp32s * pKernel, NppiSize oKernelSize, NppiPoint oAnchor, Npp32s nDivisor, NppiBorderType eBorderType);

/**
 * Single channel 16-bit unsigned convolution filter with border control.
 * 
 *
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *        Coeffcients are expected to be stored in reverse order.
 * \param oKernelSize Width and Height of the rectangular kernel.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \param nDivisor The factor by which the convolved summation from the Filter
 *        operation should be divided.  If equal to the sum of coefficients,
 *        this will keep the maximum result value within full scale.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterBorder_16u_C1R(const Npp16u * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp16u * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                         const Npp32s * pKernel, NppiSize oKernelSize, NppiPoint oAnchor, Npp32s nDivisor, NppiBorderType eBorderType);
  
/**
 * Three channel 16-bit unsigned convolution filter with border control.
 * 
 *
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *        Coeffcients are expected to be stored in reverse order.
 * \param oKernelSize Width and Height of the rectangular kernel.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \param nDivisor The factor by which the convolved summation from the Filter
 *        operation should be divided.  If equal to the sum of coefficients,
 *        this will keep the maximum result value within full scale.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterBorder_16u_C3R(const Npp16u * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp16u * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                         const Npp32s * pKernel, NppiSize oKernelSize, NppiPoint oAnchor, Npp32s nDivisor, NppiBorderType eBorderType);
                 
/**
 * Four channel channel 16-bit unsigned convolution filter with border control.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *        Coeffcients are expected to be stored in reverse order.
 * \param oKernelSize Width and Height of the rectangular kernel.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \param nDivisor The factor by which the convolved summation from the Filter
 *        operation should be divided.  If equal to the sum of coefficients,
 *        this will keep the maximum result value within full scale.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterBorder_16u_C4R(const Npp16u * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp16u * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                         const Npp32s * pKernel, NppiSize oKernelSize, NppiPoint oAnchor, Npp32s nDivisor, NppiBorderType eBorderType);

/**
 * Four channel 16-bit unsigned convolution filter with border control, ignoring alpha channel.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *        Coeffcients are expected to be stored in reverse order.
 * \param oKernelSize Width and Height of the rectangular kernel.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \param nDivisor The factor by which the convolved summation from the Filter
 *        operation should be divided.  If equal to the sum of coefficients,
 *        this will keep the maximum result value within full scale.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterBorder_16u_AC4R(const Npp16u * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp16u * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                          const Npp32s * pKernel, NppiSize oKernelSize, NppiPoint oAnchor, Npp32s nDivisor, NppiBorderType eBorderType);

/**
 * Single channel 16-bit convolution filter with border control.
 * 
 *
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *        Coeffcients are expected to be stored in reverse order.
 * \param oKernelSize Width and Height of the rectangular kernel.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \param nDivisor The factor by which the convolved summation from the Filter
 *        operation should be divided.  If equal to the sum of coefficients,
 *        this will keep the maximum result value within full scale.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterBorder_16s_C1R(const Npp16s * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp16s * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                         const Npp32s * pKernel, NppiSize oKernelSize, NppiPoint oAnchor, Npp32s nDivisor, NppiBorderType eBorderType);
  
/**
 * Three channel 16-bit convolution filter with border control.
 * 
 *
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *        Coeffcients are expected to be stored in reverse order.
 * \param oKernelSize Width and Height of the rectangular kernel.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \param nDivisor The factor by which the convolved summation from the Filter
 *        operation should be divided.  If equal to the sum of coefficients,
 *        this will keep the maximum result value within full scale.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterBorder_16s_C3R(const Npp16s * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp16s * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                         const Npp32s * pKernel, NppiSize oKernelSize, NppiPoint oAnchor, Npp32s nDivisor, NppiBorderType eBorderType);
                 
/**
 * Four channel channel 16-bit convolution filter with border control.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *        Coeffcients are expected to be stored in reverse order.
 * \param oKernelSize Width and Height of the rectangular kernel.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \param nDivisor The factor by which the convolved summation from the Filter
 *        operation should be divided.  If equal to the sum of coefficients,
 *        this will keep the maximum result value within full scale.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterBorder_16s_C4R(const Npp16s * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp16s * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                         const Npp32s * pKernel, NppiSize oKernelSize, NppiPoint oAnchor, Npp32s nDivisor, NppiBorderType eBorderType);

/**
 * Four channel 16-bit convolution filter with border control, ignoring alpha channel.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *        Coeffcients are expected to be stored in reverse order.
 * \param oKernelSize Width and Height of the rectangular kernel.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \param nDivisor The factor by which the convolved summation from the Filter
 *        operation should be divided.  If equal to the sum of coefficients,
 *        this will keep the maximum result value within full scale.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterBorder_16s_AC4R(const Npp16s * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp16s * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                          const Npp32s * pKernel, NppiSize oKernelSize, NppiPoint oAnchor, Npp32s nDivisor, NppiBorderType eBorderType);
 
/**
 * Single channel 32-bit float convolution filter with border control.
 * 
 *
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *        Coeffcients are expected to be stored in reverse order.
 * \param oKernelSize Width and Height of the rectangular kernel.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus
nppiFilterBorder_32f_C1R(const Npp32f * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp32f * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                         const Npp32f * pKernel, NppiSize oKernelSize, NppiPoint oAnchor, NppiBorderType eBorderType);

/**
 * Two channel 32-bit float convolution filter with border control.
 * 
 *
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *        Coeffcients are expected to be stored in reverse order.
 * \param oKernelSize Width and Height of the rectangular kernel.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus
nppiFilterBorder_32f_C2R(const Npp32f * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp32f * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                         const Npp32f * pKernel, NppiSize oKernelSize, NppiPoint oAnchor, NppiBorderType eBorderType);

/**
 * Three channel 32-bit float convolution filter with border control.
 * 
 *
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *        Coeffcients are expected to be stored in reverse order.
 * \param oKernelSize Width and Height of the rectangular kernel.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus
nppiFilterBorder_32f_C3R(const Npp32f * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp32f * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                         const Npp32f * pKernel, NppiSize oKernelSize, NppiPoint oAnchor, NppiBorderType eBorderType);

/**
 * Four channel 32-bit float convolution filter with border control.
 * 
 *
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *        Coeffcients are expected to be stored in reverse order.
 * \param oKernelSize Width and Height of the rectangular kernel.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus
nppiFilterBorder_32f_C4R(const Npp32f * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp32f * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                         const Npp32f * pKernel, NppiSize oKernelSize, NppiPoint oAnchor, NppiBorderType eBorderType);

/**
 * Four channel 32-bit float convolution filter with border control, ignoring alpha channel.
 * 
 *
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *        Coeffcients are expected to be stored in reverse order.
 * \param oKernelSize Width and Height of the rectangular kernel.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus
nppiFilterBorder_32f_AC4R(const Npp32f * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp32f * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                          const Npp32f * pKernel, NppiSize oKernelSize, NppiPoint oAnchor, NppiBorderType eBorderType);

/** @} FilterBorder */

/** @name FilterBorder32f
 * General purpose 2D convolution filter using floating-point weights with border control.
 *
 * Pixels under the mask are multiplied by the respective weights in the mask
 * and the results are summed. Before writing the result pixel the sum is scaled
 * back via division by nDivisor. If any portion of the mask overlaps the source
 * image boundary the requested border type operation is applied to all mask pixels
 * which fall outside of the source image.
 *
 * Currently only the NPP_BORDER_REPLICATE border type operation is supported.
 *
 * @{
 *
 */


/**
 * Single channel 8-bit unsigned convolution filter with border control.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *        Coeffcients are expected to be stored in reverse order.
 * \param oKernelSize Width and Height of the rectangular kernel.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus
nppiFilterBorder32f_8u_C1R(const Npp8u * pSrc, int nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp8u * pDst, int nDstStep, NppiSize oSizeROI, 
                           const Npp32f * pKernel, NppiSize oKernelSize, NppiPoint oAnchor, NppiBorderType eBorderType);
        
/**
 * Two channel 8-bit unsigned convolution filter with border control.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *        Coeffcients are expected to be stored in reverse order.
 * \param oKernelSize Width and Height of the rectangular kernel.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus
nppiFilterBorder32f_8u_C2R(const Npp8u * pSrc, int nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp8u * pDst, int nDstStep, NppiSize oSizeROI, 
                           const Npp32f * pKernel, NppiSize oKernelSize, NppiPoint oAnchor, NppiBorderType eBorderType);
        
/**
 * Three channel 8-bit unsigned convolution filter with border control.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *        Coeffcients are expected to be stored in reverse order.
 * \param oKernelSize Width and Height of the rectangular kernel.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus
nppiFilterBorder32f_8u_C3R(const Npp8u * pSrc, int nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp8u * pDst, int nDstStep, NppiSize oSizeROI, 
                           const Npp32f * pKernel, NppiSize oKernelSize, NppiPoint oAnchor, NppiBorderType eBorderType);
        
/**
 * Four channel 8-bit unsigned convolution filter with border control.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *        Coeffcients are expected to be stored in reverse order.
 * \param oKernelSize Width and Height of the rectangular kernel.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus
nppiFilterBorder32f_8u_C4R(const Npp8u * pSrc, int nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp8u * pDst, int nDstStep, NppiSize oSizeROI, 
                           const Npp32f * pKernel, NppiSize oKernelSize, NppiPoint oAnchor, NppiBorderType eBorderType);
        
/**
 * Four channel 8-bit unsigned convolution filter with border control, ignorint alpha channel.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *        Coeffcients are expected to be stored in reverse order.
 * \param oKernelSize Width and Height of the rectangular kernel.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus
nppiFilterBorder32f_8u_AC4R(const Npp8u * pSrc, int nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp8u * pDst, int nDstStep, NppiSize oSizeROI, 
                            const Npp32f * pKernel, NppiSize oKernelSize, NppiPoint oAnchor, NppiBorderType eBorderType);

/**
 * Single channel 8-bit signed convolution filter with border control.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *        Coeffcients are expected to be stored in reverse order.
 * \param oKernelSize Width and Height of the rectangular kernel.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus
nppiFilterBorder32f_8s_C1R(const Npp8s * pSrc, int nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp8s * pDst, int nDstStep, NppiSize oSizeROI, 
                           const Npp32f * pKernel, NppiSize oKernelSize, NppiPoint oAnchor, NppiBorderType eBorderType);

/**
 * Two channel 8-bit signed convolution filter with border control.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *        Coeffcients are expected to be stored in reverse order.
 * \param oKernelSize Width and Height of the rectangular kernel.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus
nppiFilterBorder32f_8s_C2R(const Npp8s * pSrc, int nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp8s * pDst, int nDstStep, NppiSize oSizeROI, 
                           const Npp32f * pKernel, NppiSize oKernelSize, NppiPoint oAnchor, NppiBorderType eBorderType);

/**
 * Three channel 8-bit signed convolution filter with border control.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *        Coeffcients are expected to be stored in reverse order.
 * \param oKernelSize Width and Height of the rectangular kernel.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus
nppiFilterBorder32f_8s_C3R(const Npp8s * pSrc, int nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp8s * pDst, int nDstStep, NppiSize oSizeROI, 
                           const Npp32f * pKernel, NppiSize oKernelSize, NppiPoint oAnchor, NppiBorderType eBorderType);

/**
 * Four channel 8-bit signed convolution filter with border control.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *        Coeffcients are expected to be stored in reverse order.
 * \param oKernelSize Width and Height of the rectangular kernel.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus
nppiFilterBorder32f_8s_C4R(const Npp8s * pSrc, int nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp8s * pDst, int nDstStep, NppiSize oSizeROI, 
                           const Npp32f * pKernel, NppiSize oKernelSize, NppiPoint oAnchor, NppiBorderType eBorderType);

/**
 * Four channel 8-bit signed convolution filter with border control, ignoring alpha channel.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *        Coeffcients are expected to be stored in reverse order.
 * \param oKernelSize Width and Height of the rectangular kernel.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus
nppiFilterBorder32f_8s_AC4R(const Npp8s * pSrc, int nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp8s * pDst, int nDstStep, NppiSize oSizeROI, 
                            const Npp32f * pKernel, NppiSize oKernelSize, NppiPoint oAnchor, NppiBorderType eBorderType);

/**
 * Single channel 16-bit unsigned convolution filter with border control.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *        Coeffcients are expected to be stored in reverse order.
 * \param oKernelSize Width and Height of the rectangular kernel.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus
nppiFilterBorder32f_16u_C1R(const Npp16u * pSrc, int nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp16u * pDst, int nDstStep, NppiSize oSizeROI, 
                            const Npp32f * pKernel, NppiSize oKernelSize, NppiPoint oAnchor, NppiBorderType eBorderType);

/**
 * Three channel 16-bit unsigned convolution filter with border control.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *        Coeffcients are expected to be stored in reverse order.
 * \param oKernelSize Width and Height of the rectangular kernel.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus
nppiFilterBorder32f_16u_C3R(const Npp16u * pSrc, int nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp16u * pDst, int nDstStep, NppiSize oSizeROI, 
                            const Npp32f * pKernel, NppiSize oKernelSize, NppiPoint oAnchor, NppiBorderType eBorderType);

/**
 * Four channel 16-bit unsigned convolution filter with border control.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *        Coeffcients are expected to be stored in reverse order.
 * \param oKernelSize Width and Height of the rectangular kernel.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus
nppiFilterBorder32f_16u_C4R(const Npp16u * pSrc, int nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp16u * pDst, int nDstStep, NppiSize oSizeROI, 
                            const Npp32f * pKernel, NppiSize oKernelSize, NppiPoint oAnchor, NppiBorderType eBorderType);

/**
 * Four channel 16-bit unsigned convolution filter with border control, ignoring alpha channel.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *        Coeffcients are expected to be stored in reverse order.
 * \param oKernelSize Width and Height of the rectangular kernel.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus
nppiFilterBorder32f_16u_AC4R(const Npp16u * pSrc, int nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp16u * pDst, int nDstStep, NppiSize oSizeROI, 
                             const Npp32f * pKernel, NppiSize oKernelSize, NppiPoint oAnchor, NppiBorderType eBorderType);

/**
 * Single channel 16-bit convolution filter with border control.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *        Coeffcients are expected to be stored in reverse order.
 * \param oKernelSize Width and Height of the rectangular kernel.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus
nppiFilterBorder32f_16s_C1R(const Npp16s * pSrc, int nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp16s * pDst, int nDstStep, NppiSize oSizeROI, 
                            const Npp32f * pKernel, NppiSize oKernelSize, NppiPoint oAnchor, NppiBorderType eBorderType);

/**
 * Three channel 16-bit convolution filter with border control.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *        Coeffcients are expected to be stored in reverse order.
 * \param oKernelSize Width and Height of the rectangular kernel.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus
nppiFilterBorder32f_16s_C3R(const Npp16s * pSrc, int nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp16s * pDst, int nDstStep, NppiSize oSizeROI, 
                            const Npp32f * pKernel, NppiSize oKernelSize, NppiPoint oAnchor, NppiBorderType eBorderType);

/**
 * Four channel 16-bit convolution filter with border control.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *        Coeffcients are expected to be stored in reverse order.
 * \param oKernelSize Width and Height of the rectangular kernel.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus
nppiFilterBorder32f_16s_C4R(const Npp16s * pSrc, int nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp16s * pDst, int nDstStep, NppiSize oSizeROI, 
                            const Npp32f * pKernel, NppiSize oKernelSize, NppiPoint oAnchor, NppiBorderType eBorderType);

/**
 * Four channel 16-bit convolution filter with border control, ignoring alpha channel.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *        Coeffcients are expected to be stored in reverse order.
 * \param oKernelSize Width and Height of the rectangular kernel.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus
nppiFilterBorder32f_16s_AC4R(const Npp16s * pSrc, int nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp16s * pDst, int nDstStep, NppiSize oSizeROI, 
                             const Npp32f * pKernel, NppiSize oKernelSize, NppiPoint oAnchor, NppiBorderType eBorderType);


/**
 * Single channel 32-bit convolution filter with border control.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *        Coeffcients are expected to be stored in reverse order.
 * \param oKernelSize Width and Height of the rectangular kernel.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus
nppiFilterBorder32f_32s_C1R(const Npp32s * pSrc, int nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp32s * pDst, int nDstStep, NppiSize oSizeROI, 
                            const Npp32f * pKernel, NppiSize oKernelSize, NppiPoint oAnchor, NppiBorderType eBorderType);

/**
 * Three channel 32-bit convolution filter with border control.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *        Coeffcients are expected to be stored in reverse order.
 * \param oKernelSize Width and Height of the rectangular kernel.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus
nppiFilterBorder32f_32s_C3R(const Npp32s * pSrc, int nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp32s * pDst, int nDstStep, NppiSize oSizeROI, 
                            const Npp32f * pKernel, NppiSize oKernelSize, NppiPoint oAnchor, NppiBorderType eBorderType);

/**
 * Four channel 32-bit convolution filter with border control.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *        Coeffcients are expected to be stored in reverse order.
 * \param oKernelSize Width and Height of the rectangular kernel.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus
nppiFilterBorder32f_32s_C4R(const Npp32s * pSrc, int nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp32s * pDst, int nDstStep, NppiSize oSizeROI, 
                            const Npp32f * pKernel, NppiSize oKernelSize, NppiPoint oAnchor, NppiBorderType eBorderType);

/**
 * Four channel 32-bit convolution filter with border control, ignoring alpha channel.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *        Coeffcients are expected to be stored in reverse order.
 * \param oKernelSize Width and Height of the rectangular kernel.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus
nppiFilterBorder32f_32s_AC4R(const Npp32s * pSrc, int nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp32s * pDst, int nDstStep, NppiSize oSizeROI, 
                             const Npp32f * pKernel, NppiSize oKernelSize, NppiPoint oAnchor, NppiBorderType eBorderType);


/**
 * Single channel 8-bit unsigned to 16-bit signed convolution filter with border control.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *        Coeffcients are expected to be stored in reverse order.
 * \param oKernelSize Width and Height of the rectangular kernel.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus
nppiFilterBorder32f_8u16s_C1R(const Npp8u * pSrc, int nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp16s * pDst, int nDstStep, NppiSize oSizeROI, 
                              const Npp32f * pKernel, NppiSize oKernelSize, NppiPoint oAnchor, NppiBorderType eBorderType);

/**
 * Three channel 8-bit unsigned to 16-bit signed convolution filter with border control.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *        Coeffcients are expected to be stored in reverse order.
 * \param oKernelSize Width and Height of the rectangular kernel.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus
nppiFilterBorder32f_8u16s_C3R(const Npp8u * pSrc, int nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp16s * pDst, int nDstStep, NppiSize oSizeROI, 
                              const Npp32f * pKernel, NppiSize oKernelSize, NppiPoint oAnchor, NppiBorderType eBorderType);

/**
 * Four channel 8-bit unsigned to 16-bit signed convolution filter with border control.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *        Coeffcients are expected to be stored in reverse order.
 * \param oKernelSize Width and Height of the rectangular kernel.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus
nppiFilterBorder32f_8u16s_C4R(const Npp8u * pSrc, int nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp16s * pDst, int nDstStep, NppiSize oSizeROI, 
                             const Npp32f * pKernel, NppiSize oKernelSize, NppiPoint oAnchor, NppiBorderType eBorderType);

/**
 * Four channel 8-bit unsigned to 16-bit signed convolution filter with border control, ignoring alpha channel.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *        Coeffcients are expected to be stored in reverse order.
 * \param oKernelSize Width and Height of the rectangular kernel.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus
nppiFilterBorder32f_8u16s_AC4R(const Npp8u * pSrc, int nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp16s * pDst, int nDstStep, NppiSize oSizeROI, 
                               const Npp32f * pKernel, NppiSize oKernelSize, NppiPoint oAnchor, NppiBorderType eBorderType);

/**
 * Single channel 8-bit to 16-bit signed convolution filter with border control.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *        Coeffcients are expected to be stored in reverse order.
 * \param oKernelSize Width and Height of the rectangular kernel.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus
nppiFilterBorder32f_8s16s_C1R(const Npp8s * pSrc, int nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp16s * pDst, int nDstStep, NppiSize oSizeROI, 
                              const Npp32f * pKernel, NppiSize oKernelSize, NppiPoint oAnchor, NppiBorderType eBorderType);

/**
 * Three channel 8-bit to 16-bit signed convolution filter with border control.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *        Coeffcients are expected to be stored in reverse order.
 * \param oKernelSize Width and Height of the rectangular kernel.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus
nppiFilterBorder32f_8s16s_C3R(const Npp8s * pSrc, int nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp16s * pDst, int nDstStep, NppiSize oSizeROI, 
                              const Npp32f * pKernel, NppiSize oKernelSize, NppiPoint oAnchor, NppiBorderType eBorderType);

/**
 * Four channel 8-bit to 16-bit signed convolution filter with border control.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *        Coeffcients are expected to be stored in reverse order.
 * \param oKernelSize Width and Height of the rectangular kernel.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus
nppiFilterBorder32f_8s16s_C4R(const Npp8s * pSrc, int nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp16s * pDst, int nDstStep, NppiSize oSizeROI, 
                              const Npp32f * pKernel, NppiSize oKernelSize, NppiPoint oAnchor, NppiBorderType eBorderType);

/**
 * Four channel 8-bit to 16-bit signed convolution filter with border control, ignoring alpha channel.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pKernel Pointer to the start address of the kernel coefficient array.
 *        Coeffcients are expected to be stored in reverse order.
 * \param oKernelSize Width and Height of the rectangular kernel.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus
nppiFilterBorder32f_8s16s_AC4R(const Npp8s * pSrc, int nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp16s * pDst, int nDstStep, NppiSize oSizeROI, 
                               const Npp32f * pKernel, NppiSize oKernelSize, NppiPoint oAnchor, NppiBorderType eBorderType);


/** @} FilterBorder32f */

/** @} image_convolution */

/** @defgroup image_2D_fixed_linear_filters 2D Fixed Linear Filters
 *
 * @{
 *
 */

/** @name FilterBox
 *
 * Computes the average pixel values of the pixels under a rectangular mask.
 *
 * @{
 *
 */

/**
 * Single channel 8-bit unsigned box filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param oMaskSize Width and Height of the neighborhood region for the local
 *        Avg operation.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference relative to
 *        the source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterBox_8u_C1R(const Npp8u * pSrc, Npp32s nSrcStep, Npp8u * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                     NppiSize oMaskSize, NppiPoint oAnchor);

/**
 * Three channel 8-bit unsigned box filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param oMaskSize Width and Height of the neighborhood region for the local
 *        Avg operation.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference relative to
 *        the source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterBox_8u_C3R(const Npp8u * pSrc, Npp32s nSrcStep, Npp8u * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                     NppiSize oMaskSize, NppiPoint oAnchor);

/**
 * Four channel 8-bit unsigned box filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param oMaskSize Width and Height of the neighborhood region for the local
 *        Avg operation.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference relative to
 *        the source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterBox_8u_C4R(const Npp8u * pSrc, Npp32s nSrcStep, Npp8u * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                     NppiSize oMaskSize, NppiPoint oAnchor);

/**
 * Four channel 8-bit unsigned box filter, ignorting alpha channel.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param oMaskSize Width and Height of the neighborhood region for the local
 *        Avg operation.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference relative to
 *        the source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterBox_8u_AC4R(const Npp8u * pSrc, Npp32s nSrcStep, Npp8u * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                      NppiSize oMaskSize, NppiPoint oAnchor);

/**
 * Single channel 16-bit unsigned box filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param oMaskSize Width and Height of the neighborhood region for the local
 *        Avg operation.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference relative to
 *        the source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterBox_16u_C1R(const Npp16u * pSrc, Npp32s nSrcStep, Npp16u * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                      NppiSize oMaskSize, NppiPoint oAnchor);

/**
 * Three channel 16-bit unsigned box filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param oMaskSize Width and Height of the neighborhood region for the local
 *        Avg operation.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference relative to
 *        the source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterBox_16u_C3R(const Npp16u * pSrc, Npp32s nSrcStep, Npp16u * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                      NppiSize oMaskSize, NppiPoint oAnchor);

/**
 * Four channel 16-bit unsigned box filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param oMaskSize Width and Height of the neighborhood region for the local
 *        Avg operation.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference relative to
 *        the source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterBox_16u_C4R(const Npp16u * pSrc, Npp32s nSrcStep, Npp16u * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                      NppiSize oMaskSize, NppiPoint oAnchor);

/**
 * Four channel 16-bit unsigned box filter, ignorting alpha channel.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param oMaskSize Width and Height of the neighborhood region for the local
 *        Avg operation.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference relative to
 *        the source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterBox_16u_AC4R(const Npp16u * pSrc, Npp32s nSrcStep, Npp16u * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                       NppiSize oMaskSize, NppiPoint oAnchor);

/**
 * Single channel 16-bit box filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param oMaskSize Width and Height of the neighborhood region for the local
 *        Avg operation.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference relative to
 *        the source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterBox_16s_C1R(const Npp16s * pSrc, Npp32s nSrcStep, Npp16s * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                      NppiSize oMaskSize, NppiPoint oAnchor);

/**
 * Three channel 16-bit box filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param oMaskSize Width and Height of the neighborhood region for the local
 *        Avg operation.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference relative to
 *        the source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterBox_16s_C3R(const Npp16s * pSrc, Npp32s nSrcStep, Npp16s * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                      NppiSize oMaskSize, NppiPoint oAnchor);

/**
 * Four channel 16-bit box filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param oMaskSize Width and Height of the neighborhood region for the local
 *        Avg operation.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference relative to
 *        the source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterBox_16s_C4R(const Npp16s * pSrc, Npp32s nSrcStep, Npp16s * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                      NppiSize oMaskSize, NppiPoint oAnchor);

/**
 * Four channel 16-bit box filter, ignorting alpha channel.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param oMaskSize Width and Height of the neighborhood region for the local
 *        Avg operation.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference relative to
 *        the source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterBox_16s_AC4R(const Npp16s * pSrc, Npp32s nSrcStep, Npp16s * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                       NppiSize oMaskSize, NppiPoint oAnchor);

/**
 * Single channel 32-bit floating-point box filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param oMaskSize Width and Height of the neighborhood region for the local
 *        Avg operation.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference relative to
 *        the source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterBox_32f_C1R(const Npp32f * pSrc, Npp32s nSrcStep, Npp32f * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                      NppiSize oMaskSize, NppiPoint oAnchor);

/**
 * Three channel 32-bit floating-point box filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param oMaskSize Width and Height of the neighborhood region for the local
 *        Avg operation.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference relative to
 *        the source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterBox_32f_C3R(const Npp32f * pSrc, Npp32s nSrcStep, Npp32f * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                      NppiSize oMaskSize, NppiPoint oAnchor);

/**
 * Four channel 32-bit floating-point box filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param oMaskSize Width and Height of the neighborhood region for the local
 *        Avg operation.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference relative to
 *        the source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterBox_32f_C4R(const Npp32f * pSrc, Npp32s nSrcStep, Npp32f * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                      NppiSize oMaskSize, NppiPoint oAnchor);

/**
 * Four channel 32-bit floating-point box filter, ignorting alpha channel.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param oMaskSize Width and Height of the neighborhood region for the local
 *        Avg operation.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference relative to
 *        the source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterBox_32f_AC4R(const Npp32f * pSrc, Npp32s nSrcStep, Npp32f * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                       NppiSize oMaskSize, NppiPoint oAnchor);

/**
 * Single channel 64-bit floating-point box filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param oMaskSize Width and Height of the neighborhood region for the local
 *        Avg operation.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference relative to
 *        the source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterBox_64f_C1R(const Npp64f * pSrc, Npp32s nSrcStep, Npp64f * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                      NppiSize oMaskSize, NppiPoint oAnchor);

/** @} FilterBox */

/** @name FilterBoxBorder
 *
 * Computes the average pixel values of the pixels under a rectangular mask with border control.
 * If any portion of the mask overlaps the source image boundary the requested 
 * border type operation is applied to all mask pixels which fall outside of the source image.
 *
 * Currently only the NPP_BORDER_REPLICATE border type operation is supported. *
 *
 * @{
 *
 */

/**
 * Single channel 8-bit unsigned box filter with border control.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param oMaskSize Width and Height of the neighborhood region for the local
 *        Avg operation.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference relative to
 *        the source pixel.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterBoxBorder_8u_C1R(const Npp8u * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp8u * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                           NppiSize oMaskSize, NppiPoint oAnchor, NppiBorderType eBorderType);

/**
 * Three channel 8-bit unsigned box filter with border control.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param oMaskSize Width and Height of the neighborhood region for the local
 *        Avg operation.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference relative to
 *        the source pixel.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterBoxBorder_8u_C3R(const Npp8u * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp8u * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                           NppiSize oMaskSize, NppiPoint oAnchor, NppiBorderType eBorderType);

/**
 * Four channel 8-bit unsigned box filter with border control.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param oMaskSize Width and Height of the neighborhood region for the local
 *        Avg operation.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference relative to
 *        the source pixel.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterBoxBorder_8u_C4R(const Npp8u * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp8u * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                           NppiSize oMaskSize, NppiPoint oAnchor, NppiBorderType eBorderType);

/**
 * Four channel 8-bit unsigned box filter with border control, ignoring alpha channel.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param oMaskSize Width and Height of the neighborhood region for the local
 *        Avg operation.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference relative to
 *        the source pixel.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterBoxBorder_8u_AC4R(const Npp8u * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp8u * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                            NppiSize oMaskSize, NppiPoint oAnchor, NppiBorderType eBorderType);

/**
 * Single channel 16-bit unsigned box filter with border control.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param oMaskSize Width and Height of the neighborhood region for the local
 *        Avg operation.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference relative to
 *        the source pixel.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterBoxBorder_16u_C1R(const Npp16u * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp16u * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                            NppiSize oMaskSize, NppiPoint oAnchor, NppiBorderType eBorderType);

/**
 * Three channel 16-bit unsigned box filter with border control.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param oMaskSize Width and Height of the neighborhood region for the local
 *        Avg operation.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference relative to
 *        the source pixel.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterBoxBorder_16u_C3R(const Npp16u * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp16u * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                            NppiSize oMaskSize, NppiPoint oAnchor, NppiBorderType eBorderType);

/**
 * Four channel 16-bit unsigned box filter with border control.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param oMaskSize Width and Height of the neighborhood region for the local
 *        Avg operation.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference relative to
 *        the source pixel.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterBoxBorder_16u_C4R(const Npp16u * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp16u * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                            NppiSize oMaskSize, NppiPoint oAnchor, NppiBorderType eBorderType);

/**
 * Four channel 16-bit unsigned box filter with border control, ignoring alpha channel.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param oMaskSize Width and Height of the neighborhood region for the local
 *        Avg operation.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference relative to
 *        the source pixel.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterBoxBorder_16u_AC4R(const Npp16u * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp16u * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                             NppiSize oMaskSize, NppiPoint oAnchor, NppiBorderType eBorderType);

/**
 * Single channel 16-bit box filter with border control.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param oMaskSize Width and Height of the neighborhood region for the local
 *        Avg operation.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference relative to
 *        the source pixel.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterBoxBorder_16s_C1R(const Npp16s * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp16s * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                            NppiSize oMaskSize, NppiPoint oAnchor, NppiBorderType eBorderType);

/**
 * Three channel 16-bit box filter with border control.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param oMaskSize Width and Height of the neighborhood region for the local
 *        Avg operation.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference relative to
 *        the source pixel.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterBoxBorder_16s_C3R(const Npp16s * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp16s * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                            NppiSize oMaskSize, NppiPoint oAnchor, NppiBorderType eBorderType);

/**
 * Four channel 16-bit box filter with border control.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param oMaskSize Width and Height of the neighborhood region for the local
 *        Avg operation.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference relative to
 *        the source pixel.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterBoxBorder_16s_C4R(const Npp16s * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp16s * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                            NppiSize oMaskSize, NppiPoint oAnchor, NppiBorderType eBorderType);

/**
 * Four channel 16-bit box filter with border control, ignoring alpha channel.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param oMaskSize Width and Height of the neighborhood region for the local
 *        Avg operation.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference relative to
 *        the source pixel.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterBoxBorder_16s_AC4R(const Npp16s * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp16s * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                             NppiSize oMaskSize, NppiPoint oAnchor, NppiBorderType eBorderType);

/**
 * Single channel 32-bit floating-point box filter with border control.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param oMaskSize Width and Height of the neighborhood region for the local
 *        Avg operation.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference relative to
 *        the source pixel.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterBoxBorder_32f_C1R(const Npp32f * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp32f * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                            NppiSize oMaskSize, NppiPoint oAnchor, NppiBorderType eBorderType);

/**
 * Three channel 32-bit floating-point box filter with border control.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param oMaskSize Width and Height of the neighborhood region for the local
 *        Avg operation.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference relative to
 *        the source pixel.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterBoxBorder_32f_C3R(const Npp32f * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp32f * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                            NppiSize oMaskSize, NppiPoint oAnchor, NppiBorderType eBorderType);

/**
 * Four channel 32-bit floating-point box filter with border control.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param oMaskSize Width and Height of the neighborhood region for the local
 *        Avg operation.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference relative to
 *        the source pixel.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterBoxBorder_32f_C4R(const Npp32f * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp32f * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                            NppiSize oMaskSize, NppiPoint oAnchor, NppiBorderType eBorderType);

/**
 * Four channel 32-bit floating-point box filter with border control, ignoring alpha channel.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param oMaskSize Width and Height of the neighborhood region for the local
 *        Avg operation.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference relative to
 *        the source pixel.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterBoxBorder_32f_AC4R(const Npp32f * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp32f * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                             NppiSize oMaskSize, NppiPoint oAnchor, NppiBorderType eBorderType);

/** @} FilterBoxBorder */

/** @} image_2D_fixed_linear_filters */

/** @defgroup image_rank_filters Rank Filters
 *
 * @{
 *
 */

/** @name ImageMax Filter
 *
 * Result pixel value is the maximum of pixel values under the rectangular
 * mask region.
 *
 * @{
 *
 */

/**
 * Single channel 8-bit unsigned maximum filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param oMaskSize Width and Height of the neighborhood region for the local
 *        Max operation.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterMax_8u_C1R(const Npp8u * pSrc, Npp32s nSrcStep, Npp8u * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                     NppiSize oMaskSize, NppiPoint oAnchor);

/**
 * Three channel 8-bit unsigned maximum filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param oMaskSize Width and Height of the neighborhood region for the local
 *        Max operation.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterMax_8u_C3R(const Npp8u * pSrc, Npp32s nSrcStep, Npp8u * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                     NppiSize oMaskSize, NppiPoint oAnchor);

/**
 * Four channel 8-bit unsigned maximum filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param oMaskSize Width and Height of the neighborhood region for the local
 *        Max operation.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus
nppiFilterMax_8u_C4R(const Npp8u * pSrc, Npp32s nSrcStep, Npp8u * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                     NppiSize oMaskSize, NppiPoint oAnchor);

/**
 * Four channel 8-bit unsigned maximum filter, ignoring alpha channel.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param oMaskSize Width and Height of the neighborhood region for the local
 *        Max operation.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus
nppiFilterMax_8u_AC4R(const Npp8u * pSrc, Npp32s nSrcStep, Npp8u * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                     NppiSize oMaskSize, NppiPoint oAnchor);

/**
 * Single channel 16-bit unsigned maximum filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param oMaskSize Width and Height of the neighborhood region for the local
 *        Max operation.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterMax_16u_C1R(const Npp16u * pSrc, Npp32s nSrcStep, Npp16u * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                      NppiSize oMaskSize, NppiPoint oAnchor);

/**
 * Three channel 16-bit unsigned maximum filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param oMaskSize Width and Height of the neighborhood region for the local
 *        Max operation.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterMax_16u_C3R(const Npp16u * pSrc, Npp32s nSrcStep, Npp16u * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                      NppiSize oMaskSize, NppiPoint oAnchor);

/**
 * Four channel 16-bit unsigned maximum filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param oMaskSize Width and Height of the neighborhood region for the local
 *        Max operation.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus
nppiFilterMax_16u_C4R(const Npp16u * pSrc, Npp32s nSrcStep, Npp16u * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                      NppiSize oMaskSize, NppiPoint oAnchor);

/**
 * Four channel 16-bit unsigned maximum filter, ignoring alpha channel.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param oMaskSize Width and Height of the neighborhood region for the local
 *        Max operation.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus
nppiFilterMax_16u_AC4R(const Npp16u * pSrc, Npp32s nSrcStep, Npp16u * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                       NppiSize oMaskSize, NppiPoint oAnchor);

/**
 * Single channel 16-bit signed maximum filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param oMaskSize Width and Height of the neighborhood region for the local
 *        Max operation.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterMax_16s_C1R(const Npp16s * pSrc, Npp32s nSrcStep, Npp16s * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                      NppiSize oMaskSize, NppiPoint oAnchor);

/**
 * Three channel 16-bit signed maximum filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param oMaskSize Width and Height of the neighborhood region for the local
 *        Max operation.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterMax_16s_C3R(const Npp16s * pSrc, Npp32s nSrcStep, Npp16s * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                      NppiSize oMaskSize, NppiPoint oAnchor);

/**
 * Four channel 16-bit signed maximum filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param oMaskSize Width and Height of the neighborhood region for the local
 *        Max operation.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus
nppiFilterMax_16s_C4R(const Npp16s * pSrc, Npp32s nSrcStep, Npp16s * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                      NppiSize oMaskSize, NppiPoint oAnchor);

/**
 * Four channel 16-bit signed maximum filter, ignoring alpha channel.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param oMaskSize Width and Height of the neighborhood region for the local
 *        Max operation.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus
nppiFilterMax_16s_AC4R(const Npp16s * pSrc, Npp32s nSrcStep, Npp16s * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                       NppiSize oMaskSize, NppiPoint oAnchor);

/**
 * Single channel 32-bit floating-point maximum filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param oMaskSize Width and Height of the neighborhood region for the local
 *        Max operation.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterMax_32f_C1R(const Npp32f * pSrc, Npp32s nSrcStep, Npp32f * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                      NppiSize oMaskSize, NppiPoint oAnchor);

/**
 * Three channel 32-bit floating-point maximum filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param oMaskSize Width and Height of the neighborhood region for the local
 *        Max operation.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterMax_32f_C3R(const Npp32f * pSrc, Npp32s nSrcStep, Npp32f * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                      NppiSize oMaskSize, NppiPoint oAnchor);

/**
 * Four channel 32-bit floating-point maximum filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param oMaskSize Width and Height of the neighborhood region for the local
 *        Max operation.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus
nppiFilterMax_32f_C4R(const Npp32f * pSrc, Npp32s nSrcStep, Npp32f * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                      NppiSize oMaskSize, NppiPoint oAnchor);

/**
 * Four channel 32-bit floating-point maximum filter, ignoring alpha channel.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param oMaskSize Width and Height of the neighborhood region for the local
 *        Max operation.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus
nppiFilterMax_32f_AC4R(const Npp32f * pSrc, Npp32s nSrcStep, Npp32f * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                       NppiSize oMaskSize, NppiPoint oAnchor);

/** @} FilterMax */


/** @name ImageMin Filter
 *
 * Result pixel value is the minimum of pixel values under the rectangular
 * mask region.
 *
 *
 * @{
 *
 */

/**
 * Single channel 8-bit unsigned minimum filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param oMaskSize Width and Height of the neighborhood region for the local
 *        Max operation.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterMin_8u_C1R(const Npp8u * pSrc, Npp32s nSrcStep, Npp8u * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                     NppiSize oMaskSize, NppiPoint oAnchor);

/**
 * Three channel 8-bit unsigned minimum filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param oMaskSize Width and Height of the neighborhood region for the local
 *        Max operation.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterMin_8u_C3R(const Npp8u * pSrc, Npp32s nSrcStep, Npp8u * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                     NppiSize oMaskSize, NppiPoint oAnchor);

/**
 * Four channel 8-bit unsigned minimum filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param oMaskSize Width and Height of the neighborhood region for the local
 *        Max operation.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterMin_8u_C4R(const Npp8u * pSrc, Npp32s nSrcStep, Npp8u * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                     NppiSize oMaskSize, NppiPoint oAnchor);

/**
 * Four channel 8-bit unsigned minimum filter, ignoring alpha channel.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param oMaskSize Width and Height of the neighborhood region for the local
 *        Max operation.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterMin_8u_AC4R(const Npp8u * pSrc, Npp32s nSrcStep, Npp8u * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                      NppiSize oMaskSize, NppiPoint oAnchor);

/**
 * Single channel 16-bit unsigned minimum filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param oMaskSize Width and Height of the neighborhood region for the local
 *        Max operation.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterMin_16u_C1R(const Npp16u * pSrc, Npp32s nSrcStep, Npp16u * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                      NppiSize oMaskSize, NppiPoint oAnchor);

/**
 * Three channel 16-bit unsigned minimum filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param oMaskSize Width and Height of the neighborhood region for the local
 *        Max operation.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterMin_16u_C3R(const Npp16u * pSrc, Npp32s nSrcStep, Npp16u * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                      NppiSize oMaskSize, NppiPoint oAnchor);

/**
 * Four channel 16-bit unsigned minimum filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param oMaskSize Width and Height of the neighborhood region for the local
 *        Max operation.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterMin_16u_C4R(const Npp16u * pSrc, Npp32s nSrcStep, Npp16u * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                      NppiSize oMaskSize, NppiPoint oAnchor);

/**
 * Four channel 16-bit unsigned minimum filter, ignoring alpha channel.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param oMaskSize Width and Height of the neighborhood region for the local
 *        Max operation.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterMin_16u_AC4R(const Npp16u * pSrc, Npp32s nSrcStep, Npp16u * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                       NppiSize oMaskSize, NppiPoint oAnchor);

/**
 * Single channel 16-bit signed minimum filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param oMaskSize Width and Height of the neighborhood region for the local
 *        Max operation.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterMin_16s_C1R(const Npp16s * pSrc, Npp32s nSrcStep, Npp16s * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                      NppiSize oMaskSize, NppiPoint oAnchor);

/**
 * Three channel 16-bit signed minimum filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param oMaskSize Width and Height of the neighborhood region for the local
 *        Max operation.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterMin_16s_C3R(const Npp16s * pSrc, Npp32s nSrcStep, Npp16s * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                      NppiSize oMaskSize, NppiPoint oAnchor);

/**
 * Four channel 16-bit signed minimum filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param oMaskSize Width and Height of the neighborhood region for the local
 *        Max operation.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterMin_16s_C4R(const Npp16s * pSrc, Npp32s nSrcStep, Npp16s * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                      NppiSize oMaskSize, NppiPoint oAnchor);

/**
 * Four channel 16-bit signed minimum filter, ignoring alpha channel.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param oMaskSize Width and Height of the neighborhood region for the local
 *        Max operation.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterMin_16s_AC4R(const Npp16s * pSrc, Npp32s nSrcStep, Npp16s * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                       NppiSize oMaskSize, NppiPoint oAnchor);


/**
 * Single channel 32-bit floating-point minimum filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param oMaskSize Width and Height of the neighborhood region for the local
 *        Max operation.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterMin_32f_C1R(const Npp32f * pSrc, Npp32s nSrcStep, Npp32f * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                      NppiSize oMaskSize, NppiPoint oAnchor);

/**
 * Three channel 32-bit floating-point minimum filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param oMaskSize Width and Height of the neighborhood region for the local
 *        Max operation.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterMin_32f_C3R(const Npp32f * pSrc, Npp32s nSrcStep, Npp32f * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                      NppiSize oMaskSize, NppiPoint oAnchor);

/**
 * Four channel 32-bit floating-point minimum filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param oMaskSize Width and Height of the neighborhood region for the local
 *        Max operation.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterMin_32f_C4R(const Npp32f * pSrc, Npp32s nSrcStep, Npp32f * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                      NppiSize oMaskSize, NppiPoint oAnchor);

/**
 * Four channel 32-bit floating-point minimum filter, ignoring alpha channel.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param oMaskSize Width and Height of the neighborhood region for the local
 *        Max operation.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterMin_32f_AC4R(const Npp32f * pSrc, Npp32s nSrcStep, Npp32f * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                       NppiSize oMaskSize, NppiPoint oAnchor);

/** @} FilterMin */


/** @name ImageMedian Filter
 *
 * Result pixel value is the median of pixel values under the rectangular
 * mask region.
 *
 *
 * @{
 *
 */

/**
 * Single channel 8-bit unsigned median filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param oMaskSize Width and Height of the neighborhood region for the local
 *        Median operation.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \param pBuffer Pointer to the user-allocated scratch buffer required for the Median operation.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterMedian_8u_C1R(const Npp8u * pSrc, Npp32s nSrcStep, Npp8u * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                     NppiSize oMaskSize, NppiPoint oAnchor, Npp8u * pBuffer);

/**
 * Three channel 8-bit unsigned median filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param oMaskSize Width and Height of the neighborhood region for the local
 *        Median operation.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \param pBuffer Pointer to the user-allocated scratch buffer required for the Median operation.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterMedian_8u_C3R(const Npp8u * pSrc, Npp32s nSrcStep, Npp8u * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                     NppiSize oMaskSize, NppiPoint oAnchor, Npp8u * pBuffer);

/**
 * Four channel 8-bit unsigned median filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param oMaskSize Width and Height of the neighborhood region for the local
 *        Median operation.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \param pBuffer Pointer to the user-allocated scratch buffer required for the Median operation.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterMedian_8u_C4R(const Npp8u * pSrc, Npp32s nSrcStep, Npp8u * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                     NppiSize oMaskSize, NppiPoint oAnchor, Npp8u * pBuffer);

/**
 * Four channel 8-bit unsigned median filter, ignoring alpha channel.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param oMaskSize Width and Height of the neighborhood region for the local
 *        Median operation.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \param pBuffer Pointer to the user-allocated scratch buffer required for the Median operation.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterMedian_8u_AC4R(const Npp8u * pSrc, Npp32s nSrcStep, Npp8u * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                      NppiSize oMaskSize, NppiPoint oAnchor, Npp8u * pBuffer);

/**
 * Single channel 16-bit unsigned median filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param oMaskSize Width and Height of the neighborhood region for the local
 *        Median operation.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \param pBuffer Pointer to the user-allocated scratch buffer required for the Median operation.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterMedian_16u_C1R(const Npp16u * pSrc, Npp32s nSrcStep, Npp16u * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                      NppiSize oMaskSize, NppiPoint oAnchor, Npp8u * pBuffer);

/**
 * Three channel 16-bit unsigned median filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param oMaskSize Width and Height of the neighborhood region for the local
 *        Median operation.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \param pBuffer Pointer to the user-allocated scratch buffer required for the Median operation.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterMedian_16u_C3R(const Npp16u * pSrc, Npp32s nSrcStep, Npp16u * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                      NppiSize oMaskSize, NppiPoint oAnchor, Npp8u * pBuffer);

/**
 * Four channel 16-bit unsigned median filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param oMaskSize Width and Height of the neighborhood region for the local
 *        Median operation.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \param pBuffer Pointer to the user-allocated scratch buffer required for the Median operation.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterMedian_16u_C4R(const Npp16u * pSrc, Npp32s nSrcStep, Npp16u * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                      NppiSize oMaskSize, NppiPoint oAnchor, Npp8u * pBuffer);

/**
 * Four channel 16-bit unsigned median filter, ignoring alpha channel.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param oMaskSize Width and Height of the neighborhood region for the local
 *        Median operation.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \param pBuffer Pointer to the user-allocated scratch buffer required for the Median operation.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterMedian_16u_AC4R(const Npp16u * pSrc, Npp32s nSrcStep, Npp16u * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                       NppiSize oMaskSize, NppiPoint oAnchor, Npp8u * pBuffer);

/**
 * Single channel 16-bit signed median filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param oMaskSize Width and Height of the neighborhood region for the local
 *        Median operation.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \param pBuffer Pointer to the user-allocated scratch buffer required for the Median operation.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterMedian_16s_C1R(const Npp16s * pSrc, Npp32s nSrcStep, Npp16s * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                      NppiSize oMaskSize, NppiPoint oAnchor, Npp8u * pBuffer);

/**
 * Three channel 16-bit signed median filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param oMaskSize Width and Height of the neighborhood region for the local
 *        Median operation.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \param pBuffer Pointer to the user-allocated scratch buffer required for the Median operation.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterMedian_16s_C3R(const Npp16s * pSrc, Npp32s nSrcStep, Npp16s * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                      NppiSize oMaskSize, NppiPoint oAnchor, Npp8u * pBuffer);

/**
 * Four channel 16-bit signed median filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param oMaskSize Width and Height of the neighborhood region for the local
 *        Median operation.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \param pBuffer Pointer to the user-allocated scratch buffer required for the Median operation.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterMedian_16s_C4R(const Npp16s * pSrc, Npp32s nSrcStep, Npp16s * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                      NppiSize oMaskSize, NppiPoint oAnchor, Npp8u * pBuffer);

/**
 * Four channel 16-bit signed median filter, ignoring alpha channel.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param oMaskSize Width and Height of the neighborhood region for the local
 *        Median operation.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \param pBuffer Pointer to the user-allocated scratch buffer required for the Median operation.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterMedian_16s_AC4R(const Npp16s * pSrc, Npp32s nSrcStep, Npp16s * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                       NppiSize oMaskSize, NppiPoint oAnchor, Npp8u * pBuffer);


/**
 * Single channel 32-bit floating-point median filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param oMaskSize Width and Height of the neighborhood region for the local
 *        Median operation.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \param pBuffer Pointer to the user-allocated scratch buffer required for the Median operation.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterMedian_32f_C1R(const Npp32f * pSrc, Npp32s nSrcStep, Npp32f * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                      NppiSize oMaskSize, NppiPoint oAnchor, Npp8u * pBuffer);

/**
 * Three channel 32-bit floating-point median filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param oMaskSize Width and Height of the neighborhood region for the local
 *        Median operation.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \param pBuffer Pointer to the user-allocated scratch buffer required for the Median operation.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterMedian_32f_C3R(const Npp32f * pSrc, Npp32s nSrcStep, Npp32f * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                      NppiSize oMaskSize, NppiPoint oAnchor, Npp8u * pBuffer);

/**
 * Four channel 32-bit floating-point median filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param oMaskSize Width and Height of the neighborhood region for the local
 *        Median operation.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \param pBuffer Pointer to the user-allocated scratch buffer required for the Median operation.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterMedian_32f_C4R(const Npp32f * pSrc, Npp32s nSrcStep, Npp32f * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                      NppiSize oMaskSize, NppiPoint oAnchor, Npp8u * pBuffer);

/**
 * Four channel 32-bit floating-point median filter, ignoring alpha channel.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param oMaskSize Width and Height of the neighborhood region for the local
 *        Median operation.
 * \param oAnchor X and Y offsets of the kernel origin frame of reference
 *        relative to the source pixel.
 * \param pBuffer Pointer to the user-allocated scratch buffer required for the Median operation.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterMedian_32f_AC4R(const Npp32f * pSrc, Npp32s nSrcStep, Npp32f * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                       NppiSize oMaskSize, NppiPoint oAnchor, Npp8u * pBuffer);
                       






/**
 * Single channel 8-bit unsigned median filter scratch memory size.
 * \param oSizeROI \ref roi_specification.
 * \param oMaskSize Width and Height of the neighborhood region for the local Median operation.
 * \param nBufferSize Pointer to the size of the scratch buffer required for the Median operation.
 * \return \ref image_data_error_codes
 */
NppStatus 
nppiFilterMedianGetBufferSize_8u_C1R(NppiSize oSizeROI, NppiSize oMaskSize, Npp32u * nBufferSize);

/**
 * Three channel 8-bit unsigned median filter scratch memory size.
 * \param oSizeROI \ref roi_specification.
 * \param oMaskSize Width and Height of the neighborhood region for the local Median operation.
 * \param nBufferSize Pointer to the size of the scratch buffer required for the Median operation.
 * \return \ref image_data_error_codes
 */
NppStatus 
nppiFilterMedianGetBufferSize_8u_C3R(NppiSize oSizeROI, NppiSize oMaskSize, Npp32u * nBufferSize);

/**
 * Four channel 8-bit unsigned median filter scratch memory size.
 * \param oSizeROI \ref roi_specification.
 * \param oMaskSize Width and Height of the neighborhood region for the local Median operation.
 * \param nBufferSize Pointer to the size of the scratch buffer required for the Median operation.
 * \return \ref image_data_error_codes
 */
NppStatus 
nppiFilterMedianGetBufferSize_8u_C4R(NppiSize oSizeROI, NppiSize oMaskSize, Npp32u * nBufferSize);

/**
 * Four channel 8-bit unsigned median filter, ignoring alpha channel.
 * \param oSizeROI \ref roi_specification.
 * \param oMaskSize Width and Height of the neighborhood region for the local Median operation.
 * \param nBufferSize Pointer to the size of the scratch buffer required for the Median operation.
 * \return \ref image_data_error_codes
 */
NppStatus 
nppiFilterMedianGetBufferSize_8u_AC4R(NppiSize oSizeROI, NppiSize oMaskSize, Npp32u * nBufferSize);

/**
 * Single channel 16-bit unsigned median filter scratch memory size.
 * \param oSizeROI \ref roi_specification.
 * \param oMaskSize Width and Height of the neighborhood region for the local Median operation.
 * \param nBufferSize Pointer to the size of the scratch buffer required for the Median operation.
 * \return \ref image_data_error_codes
 */
NppStatus 
nppiFilterMedianGetBufferSize_16u_C1R(NppiSize oSizeROI, NppiSize oMaskSize, Npp32u * nBufferSize);

/**
 * Three channel 16-bit unsigned median filter scratch memory size.
 * \param oSizeROI \ref roi_specification.
 * \param oMaskSize Width and Height of the neighborhood region for the local Median operation.
 * \param nBufferSize Pointer to the size of the scratch buffer required for the Median operation.
 * \return \ref image_data_error_codes
 */
NppStatus 
nppiFilterMedianGetBufferSize_16u_C3R(NppiSize oSizeROI, NppiSize oMaskSize, Npp32u * nBufferSize);

/**
 * Four channel 16-bit unsigned median filter scratch memory size.
 * \param oSizeROI \ref roi_specification.
 * \param oMaskSize Width and Height of the neighborhood region for the local Median operation.
 * \param nBufferSize Pointer to the size of the scratch buffer required for the Median operation.
 * \return \ref image_data_error_codes
 */
NppStatus 
nppiFilterMedianGetBufferSize_16u_C4R(NppiSize oSizeROI, NppiSize oMaskSize, Npp32u * nBufferSize);

/**
 * Four channel 16-bit unsigned median filter, ignoring alpha channel.
 * \param oSizeROI \ref roi_specification.
 * \param oMaskSize Width and Height of the neighborhood region for the local Median operation.
 * \param nBufferSize Pointer to the size of the scratch buffer required for the Median operation.
 * \return \ref image_data_error_codes
 */
NppStatus 
nppiFilterMedianGetBufferSize_16u_AC4R(NppiSize oSizeROI, NppiSize oMaskSize, Npp32u * nBufferSize);

/**
 * Single channel 16-bit signed median filter scratch memory size.
 * \param oSizeROI \ref roi_specification.
 * \param oMaskSize Width and Height of the neighborhood region for the local Median operation.
 * \param nBufferSize Pointer to the size of the scratch buffer required for the Median operation.
 * \return \ref image_data_error_codes
 */
NppStatus 
nppiFilterMedianGetBufferSize_16s_C1R(NppiSize oSizeROI, NppiSize oMaskSize, Npp32u * nBufferSize);

/**
 * Three channel 16-bit signed median filter scratch memory size.
 * \param oSizeROI \ref roi_specification.
 * \param oMaskSize Width and Height of the neighborhood region for the local Median operation.
 * \param nBufferSize Pointer to the size of the scratch buffer required for the Median operation.
 * \return \ref image_data_error_codes
 */
NppStatus 
nppiFilterMedianGetBufferSize_16s_C3R(NppiSize oSizeROI, NppiSize oMaskSize, Npp32u * nBufferSize);

/**
 * Four channel 16-bit signed median filter scratch memory size.
 * \param oSizeROI \ref roi_specification.
 * \param oMaskSize Width and Height of the neighborhood region for the local Median operation.
 * \param nBufferSize Pointer to the size of the scratch buffer required for the Median operation.
 * \return \ref image_data_error_codes
 */
NppStatus 
nppiFilterMedianGetBufferSize_16s_C4R(NppiSize oSizeROI, NppiSize oMaskSize, Npp32u * nBufferSize);

/**
 * Four channel 16-bit signed median filter, ignoring alpha channel.
 * \param oSizeROI \ref roi_specification.
 * \param oMaskSize Width and Height of the neighborhood region for the local Median operation.
 * \param nBufferSize Pointer to the size of the scratch buffer required for the Median operation.
 * \return \ref image_data_error_codes
 */
NppStatus 
nppiFilterMedianGetBufferSize_16s_AC4R(NppiSize oSizeROI, NppiSize oMaskSize, Npp32u * nBufferSize);


/**
 * Single channel 32-bit floating-point median filter scratch memory size.
 * \param oSizeROI \ref roi_specification.
 * \param oMaskSize Width and Height of the neighborhood region for the local Median operation.
 * \param nBufferSize Pointer to the size of the scratch buffer required for the Median operation.
 * \return \ref image_data_error_codes
 */
NppStatus 
nppiFilterMedianGetBufferSize_32f_C1R(NppiSize oSizeROI, NppiSize oMaskSize, Npp32u * nBufferSize);

/**
 * Three channel 32-bit floating-point median filter scratch memory size.
 * \param oSizeROI \ref roi_specification.
 * \param oMaskSize Width and Height of the neighborhood region for the local Median operation.
 * \param nBufferSize Pointer to the size of the scratch buffer required for the Median operation.
 * \return \ref image_data_error_codes
 */
NppStatus 
nppiFilterMedianGetBufferSize_32f_C3R(NppiSize oSizeROI, NppiSize oMaskSize, Npp32u * nBufferSize);

/**
 * Four channel 32-bit floating-point median filter scratch memory size.
 * \param oSizeROI \ref roi_specification.
 * \param oMaskSize Width and Height of the neighborhood region for the local Median operation.
 * \param nBufferSize Pointer to the size of the scratch buffer required for the Median operation.
 * \return \ref image_data_error_codes
 */
NppStatus 
nppiFilterMedianGetBufferSize_32f_C4R(NppiSize oSizeROI, NppiSize oMaskSize, Npp32u * nBufferSize);

/**
 * Four channel 32-bit floating-point median filter, ignoring alpha channel.
 * \param oSizeROI \ref roi_specification.
 * \param oMaskSize Width and Height of the neighborhood region for the local Median operation.
 * \param nBufferSize Pointer to the size of the scratch buffer required for the Median operation.
 * \return \ref image_data_error_codes
 */
NppStatus 
nppiFilterMedianGetBufferSize_32f_AC4R(NppiSize oSizeROI, NppiSize oMaskSize, Npp32u * nBufferSize);


/** @} FilterMedian */



/** @} image_rank_filters */

/** @defgroup fixed_filters Fixed Filters
 *
 * Fixed filters perform linear filtering operations (i.e. convolutions) with predefined kernels
 * of fixed sizes.
 * 
 * Some of the fixed filters have versions with border control.   For these functions, if any portion 
 * of the mask overlaps the source image boundary the requested border type operation is applied to 
 * all mask pixels which fall outside of the source image.
 *
 * Currently only the NPP_BORDER_REPLICATE border type operation is supported for these functions.
 *
 * @{
 *
 */

/** @name FilterPrewittHoriz 
 *
 * Filters the image using a horizontal Prewitt filter kernel:
 *
 * \f[
 *  \left( \begin{array}{rrr}
 *    1 &  1 &  1 \\
 *    0 &  0 &  0 \\
 *   -1 & -1 & -1 \\
 *  \end{array} \right)
 * \f]
 *
 * @{
 *
 */

/**
 * Single channel 8-bit unsigned horizontal Prewitt filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterPrewittHoriz_8u_C1R(const Npp8u * pSrc, Npp32s nSrcStep, Npp8u * pDst, Npp32s nDstStep, NppiSize oSizeROI);

/**
 * Three channel 8-bit unsigned horizontal Prewitt filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterPrewittHoriz_8u_C3R(const Npp8u * pSrc, Npp32s nSrcStep, Npp8u * pDst, Npp32s nDstStep, NppiSize oSizeROI);

/**
 * Four channel 8-bit unsigned horizontal Prewitt filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterPrewittHoriz_8u_C4R(const Npp8u * pSrc, Npp32s nSrcStep, Npp8u * pDst, Npp32s nDstStep, NppiSize oSizeROI);

/**
 * Four channel 8-bit unsigned horizontal Prewitt filter, ignoring alpha channel.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterPrewittHoriz_8u_AC4R(const Npp8u * pSrc, Npp32s nSrcStep, Npp8u * pDst, Npp32s nDstStep, NppiSize oSizeROI);

/**
 * Single channel 16-bit signed horizontal Prewitt filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterPrewittHoriz_16s_C1R(const Npp16s * pSrc, Npp32s nSrcStep, Npp16s * pDst, Npp32s nDstStep, NppiSize oSizeROI);

/**
 * Three channel 16-bit signed horizontal Prewitt filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterPrewittHoriz_16s_C3R(const Npp16s * pSrc, Npp32s nSrcStep, Npp16s * pDst, Npp32s nDstStep, NppiSize oSizeROI);

/**
 * Four channel 16-bit signed horizontal Prewitt filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterPrewittHoriz_16s_C4R(const Npp16s * pSrc, Npp32s nSrcStep, Npp16s * pDst, Npp32s nDstStep, NppiSize oSizeROI);

/**
 * Four channel 16-bit signed horizontal Prewitt filter, ignoring alpha channel.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterPrewittHoriz_16s_AC4R(const Npp16s * pSrc, Npp32s nSrcStep, Npp16s * pDst, Npp32s nDstStep, NppiSize oSizeROI);

/**
 * Single channel 32-bit floating-point horizontal Prewitt filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterPrewittHoriz_32f_C1R(const Npp32f * pSrc, Npp32s nSrcStep, Npp32f * pDst, Npp32s nDstStep, NppiSize oSizeROI);

/**
 * Three channel 32-bit floating-point horizontal Prewitt filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterPrewittHoriz_32f_C3R(const Npp32f * pSrc, Npp32s nSrcStep, Npp32f * pDst, Npp32s nDstStep, NppiSize oSizeROI);

/**
 * Four channel 32-bit floating-point horizontal Prewitt filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterPrewittHoriz_32f_C4R(const Npp32f * pSrc, Npp32s nSrcStep, Npp32f * pDst, Npp32s nDstStep, NppiSize oSizeROI);

/**
 * Four channel 32-bit floating-point horizontal Prewitt filter, ignoring alpha channel.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterPrewittHoriz_32f_AC4R(const Npp32f * pSrc, Npp32s nSrcStep, Npp32f * pDst, Npp32s nDstStep, NppiSize oSizeROI);

/** @} FilterPrewittHoriz */

/** @name FilterPrewittVert 
 *
 * Filters the image using a vertical Prewitt filter kernel:
 *
 * \f[
 *  \left( \begin{array}{rrr}
 *   -1 & 0 & 1 \\
 *   -1 & 0 & 1 \\
 *   -1 & 0 & 1 \\
 *  \end{array} \right)
 * \f]
 *
 * @{
 *
 */

/**
 * Single channel 8-bit unsigned vertical Prewitt filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterPrewittVert_8u_C1R(const Npp8u * pSrc, Npp32s nSrcStep, Npp8u * pDst, Npp32s nDstStep, NppiSize oSizeROI);

/**
 * Three channel 8-bit unsigned vertical Prewitt filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterPrewittVert_8u_C3R(const Npp8u * pSrc, Npp32s nSrcStep, Npp8u * pDst, Npp32s nDstStep, NppiSize oSizeROI);

/**
 * Four channel 8-bit unsigned vertical Prewitt filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterPrewittVert_8u_C4R(const Npp8u * pSrc, Npp32s nSrcStep, Npp8u * pDst, Npp32s nDstStep, NppiSize oSizeROI);

/**
 * Four channel 8-bit unsigned vertical Prewitt filter, ignoring alpha channel.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterPrewittVert_8u_AC4R(const Npp8u * pSrc, Npp32s nSrcStep, Npp8u * pDst, Npp32s nDstStep, NppiSize oSizeROI);

/**
 * Single channel 16-bit signed vertical Prewitt filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterPrewittVert_16s_C1R(const Npp16s * pSrc, Npp32s nSrcStep, Npp16s * pDst, Npp32s nDstStep, NppiSize oSizeROI);

/**
 * Three channel 16-bit signed vertical Prewitt filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterPrewittVert_16s_C3R(const Npp16s * pSrc, Npp32s nSrcStep, Npp16s * pDst, Npp32s nDstStep, NppiSize oSizeROI);

/**
 * Four channel 16-bit signed vertical Prewitt filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterPrewittVert_16s_C4R(const Npp16s * pSrc, Npp32s nSrcStep, Npp16s * pDst, Npp32s nDstStep, NppiSize oSizeROI);

/**
 * Four channel 16-bit signed vertical Prewitt filter, ignoring alpha channel.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterPrewittVert_16s_AC4R(const Npp16s * pSrc, Npp32s nSrcStep, Npp16s * pDst, Npp32s nDstStep, NppiSize oSizeROI);

/**
 * Single channel 32-bit floating-point vertical Prewitt filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterPrewittVert_32f_C1R(const Npp32f * pSrc, Npp32s nSrcStep, Npp32f * pDst, Npp32s nDstStep, NppiSize oSizeROI);

/**
 * Three channel 32-bit floating-point vertical Prewitt filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterPrewittVert_32f_C3R(const Npp32f * pSrc, Npp32s nSrcStep, Npp32f * pDst, Npp32s nDstStep, NppiSize oSizeROI);

/**
 * Four channel 32-bit floating-point vertical Prewitt filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterPrewittVert_32f_C4R(const Npp32f * pSrc, Npp32s nSrcStep, Npp32f * pDst, Npp32s nDstStep, NppiSize oSizeROI);

/**
 * Four channel 32-bit floating-point vertical Prewitt filter, ignoring alpha channel.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterPrewittVert_32f_AC4R(const Npp32f * pSrc, Npp32s nSrcStep, Npp32f * pDst, Npp32s nDstStep, NppiSize oSizeROI);

/** @} FilterPrewittVert */


/** @name FilterScharrHoriz 
 *
 * Filters the image using a horizontal Scharr filter kernel:
 *
 * \f[
 *  \left( \begin{array}{rrr}
 *    3 &  10 &  3 \\
 *    0 &   0 &  0 \\
 *   -3 & -10 & -3 \\
 *  \end{array} \right)
 * \f]
 *
 * @{
 *
 */

/**
 * Single channel 8-bit unsigned to 16-bit signed horizontal Scharr filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterScharrHoriz_8u16s_C1R(const Npp8u * pSrc, Npp32s nSrcStep, Npp16s * pDst, Npp32s nDstStep, NppiSize oSizeROI);

/**
 * Single channel 8-bit signed to 16-bit signed horizontal Scharr filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterScharrHoriz_8s16s_C1R(const Npp8s * pSrc, Npp32s nSrcStep, Npp16s * pDst, Npp32s nDstStep, NppiSize oSizeROI);

/**
 * Single channel 32-bit floating-point horizontal Scharr filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterScharrHoriz_32f_C1R(const Npp32f * pSrc, Npp32s nSrcStep, Npp32f * pDst, Npp32s nDstStep, NppiSize oSizeROI);

/** @} FilterScharrHoriz */

/** @name FilterScharrVert 
 *
 * Filters the image using a vertical Scharr filter kernel:
 *
 * \f[
 *  \left( \begin{array}{rrr}
 *     3 &   0 &  -3 \\
 *    10 &   0 & -10 \\
 *     3 &   0 &  -3 \\
 *  \end{array} \right)
 * \f]
 *
 * @{
 *
 */

/**
 * Single channel 8-bit unsigned to 16-bit signed vertical Scharr filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterScharrVert_8u16s_C1R(const Npp8u * pSrc, Npp32s nSrcStep, Npp16s * pDst, Npp32s nDstStep, NppiSize oSizeROI);

/**
 * Single channel 8-bit signed to 16-bit signed vertical Scharr filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterScharrVert_8s16s_C1R(const Npp8s * pSrc, Npp32s nSrcStep, Npp16s * pDst, Npp32s nDstStep, NppiSize oSizeROI);

/**
 * Single channel 32-bit floating-point vertical Scharr filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterScharrVert_32f_C1R(const Npp32f * pSrc, Npp32s nSrcStep, Npp32f * pDst, Npp32s nDstStep, NppiSize oSizeROI);

/** @} FilterScharrVert */

/** @name FilterScharrHorizBorder 
 *
 * Filters the image using a horizontal Scharr filter kernel with border control:
 *
 * \f[
 *  \left( \begin{array}{rrr}
 *    3 &  10 &  3 \\
 *    0 &   0 &  0 \\
 *   -3 & -10 & -3 \\
 *  \end{array} \right)
 * \f]
 *
 * @{
 *
 */

/**
 * Single channel 8-bit unsigned to 16-bit signed horizontal Scharr filter kernel with border control.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterScharrHorizBorder_8u16s_C1R(const Npp8u * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp16s * pDst, Npp32s nDstStep, NppiSize oSizeROI, NppiBorderType eBorderType);

/**
 * Single channel 8-bit signed to 16-bit signed horizontal Scharr filter kernel with border control.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterScharrHorizBorder_8s16s_C1R(const Npp8s * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp16s * pDst, Npp32s nDstStep, NppiSize oSizeROI, NppiBorderType eBorderType);

/**
 * Single channel 32-bit floating-point horizontal Scharr filter kernel with border control.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterScharrHorizBorder_32f_C1R(const Npp32f * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp32f * pDst, Npp32s nDstStep, NppiSize oSizeROI, NppiBorderType eBorderType);

/** @} FilterScharrHorizBorder */

/** @name FilterScharrVertBorder 
 *
 * Filters the image using a vertical Scharr filter kernel kernel with border control:
 *
 * \f[
 *  \left( \begin{array}{rrr}
 *     3 &   0 &  -3 \\
 *    10 &   0 & -10 \\
 *     3 &   0 &  -3 \\
 *  \end{array} \right)
 * \f]
 *
 * @{
 *
 */

/**
 * Single channel 8-bit unsigned to 16-bit signed vertical Scharr filter kernel with border control.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterScharrVertBorder_8u16s_C1R(const Npp8u * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp16s * pDst, Npp32s nDstStep, NppiSize oSizeROI, NppiBorderType eBorderType);

/**
 * Single channel 8-bit signed to 16-bit signed vertical Scharr filter kernel with border control.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterScharrVertBorder_8s16s_C1R(const Npp8s * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp16s * pDst, Npp32s nDstStep, NppiSize oSizeROI, NppiBorderType eBorderType);

/**
 * Single channel 32-bit floating-point vertical Scharr filter kernel with border control.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterScharrVertBorder_32f_C1R(const Npp32f * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp32f * pDst, Npp32s nDstStep, NppiSize oSizeROI, NppiBorderType eBorderType);

/** @} FilterScharrVertBorder */


/** @name FilterSobelHoriz 
 *              
 * Filters the image using a horizontal Sobel filter kernel:
 *
 * \f[
 *  \left( \begin{array}{rrr}
 *    1 &  2 &  1 \\
 *    0 &  0 &  0 \\
 *   -1 & -2 & -1 \\
 *  \end{array} \right)
 *  \left( \begin{array}{rrrrr}
 *    1  &  4 &   6 &  4 &  1 \\
 *    2  &  8 &  12 &  8 &  2 \\
 *    0  &  0 &   0 &  0 &  0 \\
 *    -2 & -8 & -12 & -8 & -2 \\
 *    -1 & -4 &  -6 & -4 & -1 \\
 *  \end{array} \right)
 * \f]
 *
 * @{
 *
 */

/**
 * Single channel 8-bit unsigned horizontal Sobel filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterSobelHoriz_8u_C1R(const Npp8u * pSrc, Npp32s nSrcStep, Npp8u * pDst, Npp32s nDstStep, NppiSize oSizeROI);

/**
 * Three channel 8-bit unsigned horizontal Sobel filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterSobelHoriz_8u_C3R(const Npp8u * pSrc, Npp32s nSrcStep, Npp8u * pDst, Npp32s nDstStep, NppiSize oSizeROI);

/**
 * Four channel 8-bit unsigned horizontal Sobel filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterSobelHoriz_8u_C4R(const Npp8u * pSrc, Npp32s nSrcStep, Npp8u * pDst, Npp32s nDstStep, NppiSize oSizeROI);

/**
 * Four channel 16-bit signed horizontal Sobel filter, ignoring alpha channel.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterSobelHoriz_8u_AC4R(const Npp8u * pSrc, Npp32s nSrcStep, Npp8u * pDst, Npp32s nDstStep, NppiSize oSizeROI);

/**
 * Single channel 16-bit signed horizontal Sobel filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterSobelHoriz_16s_C1R(const Npp16s * pSrc, Npp32s nSrcStep, Npp16s * pDst, Npp32s nDstStep, NppiSize oSizeROI);

/**
 * Three channel 16-bit signed horizontal Sobel filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterSobelHoriz_16s_C3R(const Npp16s * pSrc, Npp32s nSrcStep, Npp16s * pDst, Npp32s nDstStep, NppiSize oSizeROI);

/**
 * Four channel 16-bit signed horizontal Sobel filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterSobelHoriz_16s_C4R(const Npp16s * pSrc, Npp32s nSrcStep, Npp16s * pDst, Npp32s nDstStep, NppiSize oSizeROI);

/**
 * Four channel 8-bit unsigned horizontal Sobel filter, ignoring alpha channel.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterSobelHoriz_16s_AC4R(const Npp16s * pSrc, Npp32s nSrcStep, Npp16s * pDst, Npp32s nDstStep, NppiSize oSizeROI);

/**
 * Single channel 32-bit floating-point horizontal Sobel filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterSobelHoriz_32f_C1R(const Npp32f * pSrc, Npp32s nSrcStep, Npp32f * pDst, Npp32s nDstStep, NppiSize oSizeROI);

/**
 * Three channel 32-bit floating-point horizontal Sobel filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterSobelHoriz_32f_C3R(const Npp32f * pSrc, Npp32s nSrcStep, Npp32f * pDst, Npp32s nDstStep, NppiSize oSizeROI);

/**
 * Four channel 32-bit floating-point horizontal Sobel filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterSobelHoriz_32f_C4R(const Npp32f * pSrc, Npp32s nSrcStep, Npp32f * pDst, Npp32s nDstStep, NppiSize oSizeROI);

/**
 * Four channel 32-bit floating-point horizontal Sobel filter, ignoring alpha channel.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterSobelHoriz_32f_AC4R(const Npp32f * pSrc, Npp32s nSrcStep, Npp32f * pDst, Npp32s nDstStep, NppiSize oSizeROI);

/**
 * Single channel 8-bit unsigned to 16-bit signed horizontal Sobel filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eMaskSize Enumeration value specifying the mask size.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterSobelHoriz_8u16s_C1R(const Npp8u * pSrc, Npp32s nSrcStep, Npp16s * pDst, Npp32s nDstStep, NppiSize oSizeROI,
                               NppiMaskSize eMaskSize);

/**
 * Single channel 8-bit signed to 16-bit signed horizontal Sobel filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eMaskSize Enumeration value specifying the mask size.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterSobelHoriz_8s16s_C1R(const Npp8s * pSrc, Npp32s nSrcStep, Npp16s * pDst, Npp32s nDstStep, NppiSize oSizeROI,
                               NppiMaskSize eMaskSize);

/**
 * Single channel 32-bit floating-point horizontal Sobel filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eMaskSize Enumeration value specifying the mask size.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterSobelHorizMask_32f_C1R(const Npp32f * pSrc, Npp32s nSrcStep, Npp32f * pDst, Npp32s nDstStep, NppiSize oSizeROI,
                                 NppiMaskSize eMaskSize);


/** @} FilterSobelHoriz */

/** @name FilterSobelVert 
 *
 * Filters the image using a vertical Sobel filter kernel:
 *
 * \f[
 *  \left( \begin{array}{rrr}
 *    -1 & 0 & 1 \\
 *    -2 & 0 & 2 \\
 *    -1 & 0 & 1 \\
 *  \end{array} \right)
 *  \left( \begin{array}{rrrrr}
 *    -1 &  -2 & 0 &  2 & 1 \\
 *    -4 &  -8 & 0 &  8 & 4 \\
 *    -6 & -12 & 0 & 12 & 6 \\
 *    -4 &  -8 & 0 &  8 & 4 \\
 *    -1 &  -2 & 0 &  2 & 1 \\
 *  \end{array} \right)
 * \f]
 *
 * @{
 *
 */

/**
 * Single channel 8-bit unsigned vertical Sobel filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterSobelVert_8u_C1R(const Npp8u * pSrc, Npp32s nSrcStep, Npp8u * pDst, Npp32s nDstStep, NppiSize oSizeROI);

/**
 * Three channel 8-bit unsigned vertical Sobel filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterSobelVert_8u_C3R(const Npp8u * pSrc, Npp32s nSrcStep, Npp8u * pDst, Npp32s nDstStep, NppiSize oSizeROI);

/**
 * Four channel 8-bit unsigned vertical Sobel filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterSobelVert_8u_C4R(const Npp8u * pSrc, Npp32s nSrcStep, Npp8u * pDst, Npp32s nDstStep, NppiSize oSizeROI);

/**
 * Four channel 16-bit signed vertical Sobel filter, ignoring alpha channel.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterSobelVert_8u_AC4R(const Npp8u * pSrc, Npp32s nSrcStep, Npp8u * pDst, Npp32s nDstStep, NppiSize oSizeROI);

/**
 * Single channel 16-bit signed vertical Sobel filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterSobelVert_16s_C1R(const Npp16s * pSrc, Npp32s nSrcStep, Npp16s * pDst, Npp32s nDstStep, NppiSize oSizeROI);

/**
 * Three channel 16-bit signed vertical Sobel filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterSobelVert_16s_C3R(const Npp16s * pSrc, Npp32s nSrcStep, Npp16s * pDst, Npp32s nDstStep, NppiSize oSizeROI);

/**
 * Four channel 16-bit signed vertical Sobel filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterSobelVert_16s_C4R(const Npp16s * pSrc, Npp32s nSrcStep, Npp16s * pDst, Npp32s nDstStep, NppiSize oSizeROI);

/**
 * Four channel 8-bit unsigned vertical Sobel filter, ignoring alpha channel.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterSobelVert_16s_AC4R(const Npp16s * pSrc, Npp32s nSrcStep, Npp16s * pDst, Npp32s nDstStep, NppiSize oSizeROI);

/**
 * Single channel 32-bit floating-point vertical Sobel filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterSobelVert_32f_C1R(const Npp32f * pSrc, Npp32s nSrcStep, Npp32f * pDst, Npp32s nDstStep, NppiSize oSizeROI);

/**
 * Three channel 32-bit floating-point vertical Sobel filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterSobelVert_32f_C3R(const Npp32f * pSrc, Npp32s nSrcStep, Npp32f * pDst, Npp32s nDstStep, NppiSize oSizeROI);

/**
 * Four channel 32-bit floating-point vertical Sobel filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterSobelVert_32f_C4R(const Npp32f * pSrc, Npp32s nSrcStep, Npp32f * pDst, Npp32s nDstStep, NppiSize oSizeROI);

/**
 * Four channel 32-bit floating-point vertical Sobel filter, ignoring alpha channel.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterSobelVert_32f_AC4R(const Npp32f * pSrc, Npp32s nSrcStep, Npp32f * pDst, Npp32s nDstStep, NppiSize oSizeROI);

/**
 * Single channel 8-bit unsigned to 16-bit signed vertical Sobel filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eMaskSize Enumeration value specifying the mask size.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterSobelVert_8u16s_C1R(const Npp8u * pSrc, Npp32s nSrcStep, Npp16s * pDst, Npp32s nDstStep, NppiSize oSizeROI,
                               NppiMaskSize eMaskSize);

/**
 * Single channel 8-bit signed to 16-bit signed vertical Sobel filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eMaskSize Enumeration value specifying the mask size.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterSobelVert_8s16s_C1R(const Npp8s * pSrc, Npp32s nSrcStep, Npp16s * pDst, Npp32s nDstStep, NppiSize oSizeROI,
                              NppiMaskSize eMaskSize);

/**
 * Single channel 32-bit floating-point vertical Sobel filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eMaskSize Enumeration value specifying the mask size.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterSobelVertMask_32f_C1R(const Npp32f * pSrc, Npp32s nSrcStep, Npp32f * pDst, Npp32s nDstStep, NppiSize oSizeROI,
                                NppiMaskSize eMaskSize);


/** @} FilterSobelVert */

/** @name FilterSobelHorizSecond
 *
 * Filters the image using a second derivative, horizontal Sobel filter kernel:
 *
 * \f[
 *  \left( \begin{array}{rrr}
 *     1 &   2 &   1 \\
 *    -2 &  -4 &  -2 \\
 *     1 &   2 &   1 \\
 *  \end{array} \right)
 *  \left( \begin{array}{rrrrr}
 *     1  &  4 &   6 &  4 &  1 \\
 *     0  &  0 &   0 &  0 &  0 \\
 *    -2  & -8 & -12 & -8 & -2 \\
 *     0  &  0 &   0 &  0 &  0 \\
 *     1  &  4 &   6 &  4 &  1 \\
 *  \end{array} \right)
 * \f]
 *
 * @{
 *
 */

/**
 * Single channel 8-bit unsigned to 16-bit signed second derivative, horizontal Sobel filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eMaskSize Enumeration value specifying the mask size.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterSobelHorizSecond_8u16s_C1R(const Npp8u * pSrc, Npp32s nSrcStep, Npp16s * pDst, Npp32s nDstStep, NppiSize oSizeROI,
                                     NppiMaskSize eMaskSize);

/**
 * Single channel 8-bit signed to 16-bit signed second derivative, horizontal Sobel filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eMaskSize Enumeration value specifying the mask size.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterSobelHorizSecond_8s16s_C1R(const Npp8s * pSrc, Npp32s nSrcStep, Npp16s * pDst, Npp32s nDstStep, NppiSize oSizeROI,
                                     NppiMaskSize eMaskSize);
/**
 * Single channel 32-bit floating-point second derivative, horizontal Sobel filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eMaskSize Enumeration value specifying the mask size.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterSobelHorizSecond_32f_C1R(const Npp32f * pSrc, Npp32s nSrcStep, Npp32f * pDst, Npp32s nDstStep, NppiSize oSizeROI,
                                   NppiMaskSize eMaskSize);

/** @} FilterSobelVertSecond */

/** @} FilterSobelVert */

/** @name FilterSobelVertSecond
 *
 * Filters the image using a second derivative, vertical Sobel filter kernel:
 *
 * \f[
 *  \left( \begin{array}{rrr}
     *    1 & -2 & 1 \\
     *    2 & -4 & 2 \\
     *    1 & -2 & 1 \\
 *  \end{array} \right)
 *  \left( \begin{array}{rrrrr}
 *     1 & 0 &  -2 & 0 & 1 \\
 *     4 & 0 &  -8 & 0 & 4 \\
 *     6 & 0 & -12 & 0 & 6 \\
 *     4 & 0 &  -8 & 0 & 4 \\
 *     1 & 0 &  -2 & 0 & 1 \\
 *  \end{array} \right)
 * \f]
 *
 * @{
 *
 */

/**
 * Single channel 8-bit unsigned to 16-bit signed second derivative, vertical Sobel filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eMaskSize Enumeration value specifying the mask size.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterSobelVertSecond_8u16s_C1R(const Npp8u * pSrc, Npp32s nSrcStep, Npp16s * pDst, Npp32s nDstStep, NppiSize oSizeROI,
                                    NppiMaskSize eMaskSize);

/**
 * Single channel 8-bit signed to 16-bit signed second derivative, vertical Sobel filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eMaskSize Enumeration value specifying the mask size.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterSobelVertSecond_8s16s_C1R(const Npp8s * pSrc, Npp32s nSrcStep, Npp16s * pDst, Npp32s nDstStep, NppiSize oSizeROI,
                                    NppiMaskSize eMaskSize);
/**
 * Single channel 32-bit floating-point second derivative, vertical Sobel filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eMaskSize Enumeration value specifying the mask size.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterSobelVertSecond_32f_C1R(const Npp32f * pSrc, Npp32s nSrcStep, Npp32f * pDst, Npp32s nDstStep, NppiSize oSizeROI,
                                  NppiMaskSize eMaskSize);

/** @} FilterSobelVertSecond */

/** @name FilterSobelCross
 *
 * Filters the image using a second cross derivative Sobel filter kernel:
 *
 * \f[
 *  \left( \begin{array}{rrr}
 *    -1 & 0 &  1 \\
 *     0 & 0 &  0 \\
 *     1 & 0 & -1 \\
 *  \end{array} \right)
 *  \left( \begin{array}{rrrrr}
 *    -1 & -2 & 0 &  2 &  1 \\
 *    -2 & -4 & 0 &  4 &  2 \\
 *     0 &  0 & 0 &  0 &  0 \\
 *     2 &  4 & 0 & -4 & -2 \\
 *     1 &  2 & 0 & -2 & -1 \\
 *  \end{array} \right)
 * \f]
 *
 * @{
 *
 */

/**
 * Single channel 8-bit unsigned to 16-bit signed second cross derivative Sobel filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eMaskSize Enumeration value specifying the mask size.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterSobelCross_8u16s_C1R(const Npp8u * pSrc, Npp32s nSrcStep, Npp16s * pDst, Npp32s nDstStep, NppiSize oSizeROI,
                                    NppiMaskSize eMaskSize);

/**
 * Single channel 8-bit signed to 16-bit signed second cross derivative Sobel filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eMaskSize Enumeration value specifying the mask size.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterSobelCross_8s16s_C1R(const Npp8s * pSrc, Npp32s nSrcStep, Npp16s * pDst, Npp32s nDstStep, NppiSize oSizeROI,
                                    NppiMaskSize eMaskSize);
/**
 * Single channel 32-bit floating-point second cross derivative Sobel filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eMaskSize Enumeration value specifying the mask size.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterSobelCross_32f_C1R(const Npp32f * pSrc, Npp32s nSrcStep, Npp32f * pDst, Npp32s nDstStep, NppiSize oSizeROI,
                                  NppiMaskSize eMaskSize);

/** @} FilterSobelCross */


/** @name FilterSobelHorizBorder 
 *
 * Filters the image using a horizontal Sobel filter kernel with border control:
 *
 * \f[
 *  \left( \begin{array}{rrr}
 *    1 &  2 &  1 \\
 *    0 &  0 &  0 \\
 *   -1 & -2 & -1 \\
 *  \end{array} \right)
 *  \left( \begin{array}{rrrrr}
 *    1  &  4 &   6 &  4 &  1 \\
 *    2  &  8 &  12 &  8 &  2 \\
 *    0  &  0 &   0 &  0 &  0 \\
 *    -2 & -8 & -12 & -8 & -2 \\
 *    -1 & -4 &  -6 & -4 & -1 \\
 *  \end{array} \right)
 * \f]
 *
 * @{
 *
 */

/**
 * Single channel 8-bit unsigned horizontal Sobel filter with border control.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterSobelHorizBorder_8u_C1R(const Npp8u * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp8u * pDst, Npp32s nDstStep, NppiSize oSizeROI, NppiBorderType eBorderType);

/**
 * Three channel 8-bit unsigned horizontal Sobel filter with border control.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterSobelHorizBorder_8u_C3R(const Npp8u * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp8u * pDst, Npp32s nDstStep, NppiSize oSizeROI, NppiBorderType eBorderType);

/**
 * Four channel 8-bit unsigned horizontal Sobel filter with border control.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterSobelHorizBorder_8u_C4R(const Npp8u * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp8u * pDst, Npp32s nDstStep, NppiSize oSizeROI, NppiBorderType eBorderType);

/**
 * Four channel 16-bit signed horizontal Sobel filter with border control, ignoring alpha channel.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterSobelHorizBorder_8u_AC4R(const Npp8u * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp8u * pDst, Npp32s nDstStep, NppiSize oSizeROI, NppiBorderType eBorderType);

/**
 * Single channel 16-bit signed horizontal Sobel filter with border control.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterSobelHorizBorder_16s_C1R(const Npp16s * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp16s * pDst, Npp32s nDstStep, NppiSize oSizeROI, NppiBorderType eBorderType);

/**
 * Three channel 16-bit signed horizontal Sobel filter with border control.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterSobelHorizBorder_16s_C3R(const Npp16s * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp16s * pDst, Npp32s nDstStep, NppiSize oSizeROI, NppiBorderType eBorderType);

/**
 * Four channel 16-bit signed horizontal Sobel filter with border control.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterSobelHorizBorder_16s_C4R(const Npp16s * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp16s * pDst, Npp32s nDstStep, NppiSize oSizeROI, NppiBorderType eBorderType);

/**
 * Four channel 8-bit unsigned horizontal Sobel filter with border control, ignoring alpha channel.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterSobelHorizBorder_16s_AC4R(const Npp16s * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp16s * pDst, Npp32s nDstStep, NppiSize oSizeROI, NppiBorderType eBorderType);

/**
 * Single channel 32-bit floating-point horizontal Sobel filter with border control.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterSobelHorizBorder_32f_C1R(const Npp32f * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp32f * pDst, Npp32s nDstStep, NppiSize oSizeROI, NppiBorderType eBorderType);

/**
 * Three channel 32-bit floating-point horizontal Sobel filter with border control.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterSobelHorizBorder_32f_C3R(const Npp32f * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp32f * pDst, Npp32s nDstStep, NppiSize oSizeROI, NppiBorderType eBorderType);

/**
 * Four channel 32-bit floating-point horizontal Sobel filter with border control.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterSobelHorizBorder_32f_C4R(const Npp32f * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp32f * pDst, Npp32s nDstStep, NppiSize oSizeROI, NppiBorderType eBorderType);

/**
 * Four channel 32-bit floating-point horizontal Sobel filter with border control, ignoring alpha channel.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterSobelHorizBorder_32f_AC4R(const Npp32f * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp32f * pDst, Npp32s nDstStep, NppiSize oSizeROI, NppiBorderType eBorderType);

/**
 * Single channel 8-bit unsigned to 16-bit signed horizontal Sobel filter with border control.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eMaskSize Enumeration value specifying the mask size.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterSobelHorizBorder_8u16s_C1R(const Npp8u * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp16s * pDst, Npp32s nDstStep, NppiSize oSizeROI,
                                     NppiMaskSize eMaskSize, NppiBorderType eBorderType);

/**
 * Single channel 8-bit signed to 16-bit signed horizontal Sobel filter with border control.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eMaskSize Enumeration value specifying the mask size.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterSobelHorizBorder_8s16s_C1R(const Npp8s * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp16s * pDst, Npp32s nDstStep, NppiSize oSizeROI,
                                     NppiMaskSize eMaskSize, NppiBorderType eBorderType);

/**
 * Single channel 32-bit floating-point horizontal Sobel filter with border control.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eMaskSize Enumeration value specifying the mask size.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterSobelHorizMaskBorder_32f_C1R(const Npp32f * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp32f * pDst, Npp32s nDstStep, NppiSize oSizeROI,
                                       NppiMaskSize eMaskSize, NppiBorderType eBorderType);


/** @} FilterSobelHorizBorder */

/** @name FilterSobelVertBorder 
 *
 * Filters the image using a vertical Sobel filter kernel with border control:
 *
 * \f[
 *  \left( \begin{array}{rrr}
 *    -1 & 0 & 1 \\
 *    -2 & 0 & 2 \\
 *    -1 & 0 & 1 \\
 *  \end{array} \right)
 *  \left( \begin{array}{rrrrr}
 *    -1 &  -2 & 0 &  2 & 1 \\
 *    -4 &  -8 & 0 &  8 & 4 \\
 *    -6 & -12 & 0 & 12 & 6 \\
 *    -4 &  -8 & 0 &  8 & 4 \\
 *    -1 &  -2 & 0 &  2 & 1 \\
 *  \end{array} \right)
 * \f]
 *
 * @{
 *
 */

/**
 * Single channel 8-bit unsigned vertical Sobel filter with border control.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterSobelVertBorder_8u_C1R(const Npp8u * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp8u * pDst, Npp32s nDstStep, NppiSize oSizeROI, NppiBorderType eBorderType);

/**
 * Three channel 8-bit unsigned vertical Sobel filter with border control.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterSobelVertBorder_8u_C3R(const Npp8u * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp8u * pDst, Npp32s nDstStep, NppiSize oSizeROI, NppiBorderType eBorderType);

/**
 * Four channel 8-bit unsigned vertical Sobel filter with border control.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterSobelVertBorder_8u_C4R(const Npp8u * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp8u * pDst, Npp32s nDstStep, NppiSize oSizeROI, NppiBorderType eBorderType);

/**
 * Four channel 16-bit signed vertical Sobel filter with border control, ignoring alpha channel.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterSobelVertBorder_8u_AC4R(const Npp8u * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp8u * pDst, Npp32s nDstStep, NppiSize oSizeROI, NppiBorderType eBorderType);

/**
 * Single channel 16-bit signed vertical Sobel filter with border control.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterSobelVertBorder_16s_C1R(const Npp16s * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp16s * pDst, Npp32s nDstStep, NppiSize oSizeROI, NppiBorderType eBorderType);

/**
 * Three channel 16-bit signed vertical Sobel filter with border control.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterSobelVertBorder_16s_C3R(const Npp16s * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp16s * pDst, Npp32s nDstStep, NppiSize oSizeROI, NppiBorderType eBorderType);

/**
 * Four channel 16-bit signed vertical Sobel filter with border control.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterSobelVertBorder_16s_C4R(const Npp16s * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp16s * pDst, Npp32s nDstStep, NppiSize oSizeROI, NppiBorderType eBorderType);

/**
 * Four channel 8-bit unsigned vertical Sobel filter with border control, ignoring alpha channel.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterSobelVertBorder_16s_AC4R(const Npp16s * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp16s * pDst, Npp32s nDstStep, NppiSize oSizeROI, NppiBorderType eBorderType);

/**
 * Single channel 32-bit floating-point vertical Sobel filter with border control.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterSobelVertBorder_32f_C1R(const Npp32f * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp32f * pDst, Npp32s nDstStep, NppiSize oSizeROI, NppiBorderType eBorderType);

/**
 * Three channel 32-bit floating-point vertical Sobel filter with border control.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterSobelVertBorder_32f_C3R(const Npp32f * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp32f * pDst, Npp32s nDstStep, NppiSize oSizeROI, NppiBorderType eBorderType);

/**
 * Four channel 32-bit floating-point vertical Sobel filter with border control.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterSobelVertBorder_32f_C4R(const Npp32f * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp32f * pDst, Npp32s nDstStep, NppiSize oSizeROI, NppiBorderType eBorderType);

/**
 * Four channel 32-bit floating-point vertical Sobel filter with border control, ignoring alpha channel.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterSobelVertBorder_32f_AC4R(const Npp32f * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp32f * pDst, Npp32s nDstStep, NppiSize oSizeROI, NppiBorderType eBorderType);

/**
 * Single channel 8-bit unsigned to 16-bit signed vertical Sobel filter with border control.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eMaskSize Enumeration value specifying the mask size.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterSobelVertBorder_8u16s_C1R(const Npp8u * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp16s * pDst, Npp32s nDstStep, NppiSize oSizeROI,
                                    NppiMaskSize eMaskSize, NppiBorderType eBorderType);

/**
 * Single channel 8-bit signed to 16-bit signed vertical Sobel filter with border control.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eMaskSize Enumeration value specifying the mask size.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterSobelVertBorder_8s16s_C1R(const Npp8s * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp16s * pDst, Npp32s nDstStep, NppiSize oSizeROI,
                                    NppiMaskSize eMaskSize, NppiBorderType eBorderType);

/**
 * Single channel 32-bit floating-point vertical Sobel filter with border control.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eMaskSize Enumeration value specifying the mask size.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterSobelVertMaskBorder_32f_C1R(const Npp32f * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp32f * pDst, Npp32s nDstStep, NppiSize oSizeROI,
                                      NppiMaskSize eMaskSize, NppiBorderType eBorderType);


/** @} FilterSobelVertBorder */

/** @name FilterSobelHorizSecondBorder
 *
 * Filters the image using a second derivative, horizontal Sobel filter kernel with border control:
 *
 * \f[
 *  \left( \begin{array}{rrr}
 *     1 &   2 &   1 \\
 *    -2 &  -4 &  -2 \\
 *     1 &   2 &   1 \\
 *  \end{array} \right)
 *  \left( \begin{array}{rrrrr}
 *     1  &  4 &   6 &  4 &  1 \\
 *     0  &  0 &   0 &  0 &  0 \\
 *    -2  & -8 & -12 & -8 & -2 \\
 *     0  &  0 &   0 &  0 &  0 \\
 *     1  &  4 &   6 &  4 &  1 \\
 *  \end{array} \right)
 * \f]
 *
 * @{
 *
 */

/**
 * Single channel 8-bit unsigned to 16-bit signed second derivative, horizontal Sobel filter with border control.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eMaskSize Enumeration value specifying the mask size.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterSobelHorizSecondBorder_8u16s_C1R(const Npp8u * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp16s * pDst, Npp32s nDstStep, NppiSize oSizeROI,
                                           NppiMaskSize eMaskSize, NppiBorderType eBorderType);

/**
 * Single channel 8-bit signed to 16-bit signed second derivative, horizontal Sobel filter with border control.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eMaskSize Enumeration value specifying the mask size.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterSobelHorizSecondBorder_8s16s_C1R(const Npp8s * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp16s * pDst, Npp32s nDstStep, NppiSize oSizeROI,
                                           NppiMaskSize eMaskSize, NppiBorderType eBorderType);
/**
 * Single channel 32-bit floating-point second derivative, horizontal Sobel filter with border control.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eMaskSize Enumeration value specifying the mask size.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterSobelHorizSecondBorder_32f_C1R(const Npp32f * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp32f * pDst, Npp32s nDstStep, NppiSize oSizeROI,
                                         NppiMaskSize eMaskSize, NppiBorderType eBorderType);

/** @} FilterSobelVertSecondBorder */

/** @} FilterSobelVertBorder */

/** @name FilterSobelVertSecondBorder
 *
 * Filters the image using a second derivative, vertical Sobel filter kernel with border control:
 *
 * \f[
 *  \left( \begin{array}{rrr}
     *    1 & -2 & 1 \\
     *    2 & -4 & 2 \\
     *    1 & -2 & 1 \\
 *  \end{array} \right)
 *  \left( \begin{array}{rrrrr}
 *     1 & 0 &  -2 & 0 & 1 \\
 *     4 & 0 &  -8 & 0 & 4 \\
 *     6 & 0 & -12 & 0 & 6 \\
 *     4 & 0 &  -8 & 0 & 4 \\
 *     1 & 0 &  -2 & 0 & 1 \\
 *  \end{array} \right)
 * \f]
 *
 * @{
 *
 */

/**
 * Single channel 8-bit unsigned to 16-bit signed second derivative, vertical Sobel filter with border control.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eMaskSize Enumeration value specifying the mask size.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterSobelVertSecondBorder_8u16s_C1R(const Npp8u * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp16s * pDst, Npp32s nDstStep, NppiSize oSizeROI,
                                          NppiMaskSize eMaskSize, NppiBorderType eBorderType);

/**
 * Single channel 8-bit signed to 16-bit signed second derivative, vertical Sobel filter with border control.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eMaskSize Enumeration value specifying the mask size.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterSobelVertSecondBorder_8s16s_C1R(const Npp8s * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp16s * pDst, Npp32s nDstStep, NppiSize oSizeROI,
                                          NppiMaskSize eMaskSize, NppiBorderType eBorderType);
/**
 * Single channel 32-bit floating-point second derivative, vertical Sobel filter with border control.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eMaskSize Enumeration value specifying the mask size.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterSobelVertSecondBorder_32f_C1R(const Npp32f * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp32f * pDst, Npp32s nDstStep, NppiSize oSizeROI,
                                        NppiMaskSize eMaskSize, NppiBorderType eBorderType);

/** @} FilterSobelVertSecondBorder */

/** @name FilterSobelCrossBorder
 *
 * Filters the image using a second cross derivative Sobel filter kernel with border control:
 *
 * \f[
 *  \left( \begin{array}{rrr}
 *    -1 & 0 &  1 \\
 *     0 & 0 &  0 \\
 *     1 & 0 & -1 \\
 *  \end{array} \right)
 *  \left( \begin{array}{rrrrr}
 *    -1 & -2 & 0 &  2 &  1 \\
 *    -2 & -4 & 0 &  4 &  2 \\
 *     0 &  0 & 0 &  0 &  0 \\
 *     2 &  4 & 0 & -4 & -2 \\
 *     1 &  2 & 0 & -2 & -1 \\
 *  \end{array} \right)
 * \f]
 *
 * @{
 *
 */

/**
 * Single channel 8-bit unsigned to 16-bit signed second cross derivative Sobel filter with border control.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eMaskSize Enumeration value specifying the mask size.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterSobelCrossBorder_8u16s_C1R(const Npp8u * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp16s * pDst, Npp32s nDstStep, NppiSize oSizeROI,
                                     NppiMaskSize eMaskSize, NppiBorderType eBorderType);

/**
 * Single channel 8-bit signed to 16-bit signed second cross derivative Sobel filter with border control.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eMaskSize Enumeration value specifying the mask size.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterSobelCrossBorder_8s16s_C1R(const Npp8s * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp16s * pDst, Npp32s nDstStep, NppiSize oSizeROI,
                                     NppiMaskSize eMaskSize, NppiBorderType eBorderType);
/**
 * Single channel 32-bit floating-point second cross derivative Sobel filter with border control.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eMaskSize Enumeration value specifying the mask size.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterSobelCrossBorder_32f_C1R(const Npp32f * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp32f * pDst, Npp32s nDstStep, NppiSize oSizeROI,
                                   NppiMaskSize eMaskSize, NppiBorderType eBorderType);

/** @} FilterSobelCrossBorder */

/** @name FilterRobertsDown
 *
 * Filters the image using a horizontal Roberts filter kernel:
 *
 * \f[
 *  \left( \begin{array}{rrr}
 *   0 & 0 &  0 \\
 *   0 & 1 &  0 \\
 *   0 & 0 & -1 \\
 *  \end{array} \right)
 * \f]
 *
 * @{
 *
 */

/**
 * Single channel 8-bit unsigned horizontal Roberts filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterRobertsDown_8u_C1R(const Npp8u * pSrc, Npp32s nSrcStep, Npp8u * pDst, Npp32s nDstStep, NppiSize oSizeROI);

/**
 * Three channel 8-bit unsigned horizontal Roberts filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterRobertsDown_8u_C3R(const Npp8u * pSrc, Npp32s nSrcStep, Npp8u * pDst, Npp32s nDstStep, NppiSize oSizeROI);

/**
 * Four channel 8-bit unsigned horizontal Roberts filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterRobertsDown_8u_C4R(const Npp8u * pSrc, Npp32s nSrcStep, Npp8u * pDst, Npp32s nDstStep, NppiSize oSizeROI);

/**
 * Four channel 8-bit unsigned horizontal Roberts filter, ignoring alpha-channel.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterRobertsDown_8u_AC4R(const Npp8u * pSrc, Npp32s nSrcStep, Npp8u * pDst, Npp32s nDstStep, NppiSize oSizeROI);

/**
 * Single channel 16-bit signed horizontal Roberts filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterRobertsDown_16s_C1R(const Npp16s * pSrc, Npp32s nSrcStep, Npp16s * pDst, Npp32s nDstStep, NppiSize oSizeROI);

/**
 * Three channel 16-bit signed horizontal Roberts filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterRobertsDown_16s_C3R(const Npp16s * pSrc, Npp32s nSrcStep, Npp16s * pDst, Npp32s nDstStep, NppiSize oSizeROI);

/**
 * Four channel 16-bit signed horizontal Roberts filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterRobertsDown_16s_C4R(const Npp16s * pSrc, Npp32s nSrcStep, Npp16s * pDst, Npp32s nDstStep, NppiSize oSizeROI);

/**
 * Four channel 16-bit signed horizontal Roberts filter, ignoring alpha-channel.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterRobertsDown_16s_AC4R(const Npp16s * pSrc, Npp32s nSrcStep, Npp16s * pDst, Npp32s nDstStep, NppiSize oSizeROI);

/**
 * Single channel 32-bit floating-point horizontal Roberts filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterRobertsDown_32f_C1R(const Npp32f * pSrc, Npp32s nSrcStep, Npp32f * pDst, Npp32s nDstStep, NppiSize oSizeROI);

/**
 * Three channel 32-bit floating-point horizontal Roberts filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterRobertsDown_32f_C3R(const Npp32f * pSrc, Npp32s nSrcStep, Npp32f * pDst, Npp32s nDstStep, NppiSize oSizeROI);

/**
 * Four channel 32-bit floating-point horizontal Roberts filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterRobertsDown_32f_C4R(const Npp32f * pSrc, Npp32s nSrcStep, Npp32f * pDst, Npp32s nDstStep, NppiSize oSizeROI);

/**
 * Four channel 32-bit floating-point horizontal Roberts filter, ignoring alpha-channel.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterRobertsDown_32f_AC4R(const Npp32f * pSrc, Npp32s nSrcStep, Npp32f * pDst, Npp32s nDstStep, NppiSize oSizeROI);

/** @} FilterRobertsDown */

/** @name FilterRobertsUp
 *
 * Filters the image using a vertical Roberts filter kernel:
 *
 * \f[
 *  \left( \begin{array}{rrr}
 *   0 & 0 &  0 \\
 *   0 & 1 &  0 \\
 *   0 & 0 & -1 \\
 *  \end{array} \right)
 * \f]
 *
 * @{
 *
 */

/**
 * Single channel 8-bit unsigned vertical Roberts filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterRobertsUp_8u_C1R(const Npp8u * pSrc, Npp32s nSrcStep, Npp8u * pDst, Npp32s nDstStep, NppiSize oSizeROI);

/**
 * Three channel 8-bit unsigned vertical Roberts filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterRobertsUp_8u_C3R(const Npp8u * pSrc, Npp32s nSrcStep, Npp8u * pDst, Npp32s nDstStep, NppiSize oSizeROI);

/**
 * Four channel 8-bit unsigned vertical Roberts filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterRobertsUp_8u_C4R(const Npp8u * pSrc, Npp32s nSrcStep, Npp8u * pDst, Npp32s nDstStep, NppiSize oSizeROI);

/**
 * Four channel 8-bit unsigned vertical Roberts filter, ignoring alpha-channel.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterRobertsUp_8u_AC4R(const Npp8u * pSrc, Npp32s nSrcStep, Npp8u * pDst, Npp32s nDstStep, NppiSize oSizeROI);

/**
 * Single channel 16-bit signed vertical Roberts filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterRobertsUp_16s_C1R(const Npp16s * pSrc, Npp32s nSrcStep, Npp16s * pDst, Npp32s nDstStep, NppiSize oSizeROI);

/**
 * Three channel 16-bit signed vertical Roberts filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterRobertsUp_16s_C3R(const Npp16s * pSrc, Npp32s nSrcStep, Npp16s * pDst, Npp32s nDstStep, NppiSize oSizeROI);

/**
 * Four channel 16-bit signed vertical Roberts filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterRobertsUp_16s_C4R(const Npp16s * pSrc, Npp32s nSrcStep, Npp16s * pDst, Npp32s nDstStep, NppiSize oSizeROI);

/**
 * Four channel 16-bit signed vertical Roberts filter, ignoring alpha-channel.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterRobertsUp_16s_AC4R(const Npp16s * pSrc, Npp32s nSrcStep, Npp16s * pDst, Npp32s nDstStep, NppiSize oSizeROI);

/**
 * Single channel 32-bit floating-point vertical Roberts filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterRobertsUp_32f_C1R(const Npp32f * pSrc, Npp32s nSrcStep, Npp32f * pDst, Npp32s nDstStep, NppiSize oSizeROI);

/**
 * Three channel 32-bit floating-point vertical Roberts filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterRobertsUp_32f_C3R(const Npp32f * pSrc, Npp32s nSrcStep, Npp32f * pDst, Npp32s nDstStep, NppiSize oSizeROI);

/**
 * Four channel 32-bit floating-point vertical Roberts filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterRobertsUp_32f_C4R(const Npp32f * pSrc, Npp32s nSrcStep, Npp32f * pDst, Npp32s nDstStep, NppiSize oSizeROI);

/**
 * Four channel 32-bit floating-point vertical Roberts filter, ignoring alpha-channel.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterRobertsUp_32f_AC4R(const Npp32f * pSrc, Npp32s nSrcStep, Npp32f * pDst, Npp32s nDstStep, NppiSize oSizeROI);

/** @} FilterRobertsUp */


/** @name FilterLaplace
 *
 * Filters the image using a Laplacian filter kernel:
 *
 * \f[
 *  \left( \begin{array}{rrr}
 *   -1 & -1 & -1 \\
 *   -1 &  8 & -1 \\
 *   -1 & -1 & -1 \\
 *  \end{array} \right)
  *  \left( \begin{array}{rrrrr}
 *   -1 & -3 & -4 & -3 & -1 \\
 *   -3 &  0 &  6 &  0 & -3 \\
 *   -4 &  6 & 20 &  6 & -4 \\
 *   -3 &  0 &  6 &  0 & -3 \\
 *   -1 & -3 & -4 & -3 & -1 \\
 *  \end{array} \right)

 * \f]
 *
 * @{
 *
 */

/**
 * Single channel 8-bit unsigned Laplace filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eMaskSize Enumeration value specifying the mask size.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterLaplace_8u_C1R(const Npp8u * pSrc, Npp32s nSrcStep, Npp8u * pDst, Npp32s nDstStep, NppiSize oSizeROI,
                         NppiMaskSize eMaskSize);

/**
 * Three channel 8-bit unsigned Laplace filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eMaskSize Enumeration value specifying the mask size.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterLaplace_8u_C3R(const Npp8u * pSrc, Npp32s nSrcStep, Npp8u * pDst, Npp32s nDstStep, NppiSize oSizeROI,
                         NppiMaskSize eMaskSize);

/**
 * Four channel 8-bit unsigned Laplace filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eMaskSize Enumeration value specifying the mask size.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterLaplace_8u_C4R(const Npp8u * pSrc, Npp32s nSrcStep, Npp8u * pDst, Npp32s nDstStep, NppiSize oSizeROI,
                         NppiMaskSize eMaskSize);

/**
 * Four channel 8-bit unsigned Laplace filter, ignoring alpha channel.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eMaskSize Enumeration value specifying the mask size.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterLaplace_8u_AC4R(const Npp8u * pSrc, Npp32s nSrcStep, Npp8u * pDst, Npp32s nDstStep, NppiSize oSizeROI,
                          NppiMaskSize eMaskSize);

/**
 * Single channel 16-bit signed Laplace filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eMaskSize Enumeration value specifying the mask size.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterLaplace_16s_C1R(const Npp16s * pSrc, Npp32s nSrcStep, Npp16s * pDst, Npp32s nDstStep, NppiSize oSizeROI,
                          NppiMaskSize eMaskSize);

/**
 * Three channel 16-bit signed Laplace filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eMaskSize Enumeration value specifying the mask size.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterLaplace_16s_C3R(const Npp16s * pSrc, Npp32s nSrcStep, Npp16s * pDst, Npp32s nDstStep, NppiSize oSizeROI,
                          NppiMaskSize eMaskSize);

/**
 * Four channel 16-bit signed Laplace filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eMaskSize Enumeration value specifying the mask size.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterLaplace_16s_C4R(const Npp16s * pSrc, Npp32s nSrcStep, Npp16s * pDst, Npp32s nDstStep, NppiSize oSizeROI,
                          NppiMaskSize eMaskSize);

/**
 * Four channel 16-bit signed Laplace filter, ignoring alpha channel.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eMaskSize Enumeration value specifying the mask size.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterLaplace_16s_AC4R(const Npp16s * pSrc, Npp32s nSrcStep, Npp16s * pDst, Npp32s nDstStep, NppiSize oSizeROI,
                           NppiMaskSize eMaskSize);

/**
 * Single channel 32-bit floating-point Laplace filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eMaskSize Enumeration value specifying the mask size.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterLaplace_32f_C1R(const Npp32f * pSrc, Npp32s nSrcStep, Npp32f * pDst, Npp32s nDstStep, NppiSize oSizeROI,
                          NppiMaskSize eMaskSize);

/**
 * Three channel 32-bit floating-point Laplace filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eMaskSize Enumeration value specifying the mask size.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterLaplace_32f_C3R(const Npp32f * pSrc, Npp32s nSrcStep, Npp32f * pDst, Npp32s nDstStep, NppiSize oSizeROI,
                          NppiMaskSize eMaskSize);

/**
 * Four channel 32-bit floating-point Laplace filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eMaskSize Enumeration value specifying the mask size.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterLaplace_32f_C4R(const Npp32f * pSrc, Npp32s nSrcStep, Npp32f * pDst, Npp32s nDstStep, NppiSize oSizeROI,
                          NppiMaskSize eMaskSize);

/**
 * Four channel 32-bit floating-point Laplace filter, ignoring alpha channel.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eMaskSize Enumeration value specifying the mask size.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterLaplace_32f_AC4R(const Npp32f * pSrc, Npp32s nSrcStep, Npp32f * pDst, Npp32s nDstStep, NppiSize oSizeROI,
                           NppiMaskSize eMaskSize);

/**
 * Single channel 8-bit unsigned to 16-bit signed Laplace filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eMaskSize Enumeration value specifying the mask size.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterLaplace_8u16s_C1R(const Npp8u * pSrc, Npp32s nSrcStep, Npp16s * pDst, Npp32s nDstStep, NppiSize oSizeROI,
                            NppiMaskSize eMaskSize);

/**
 * Single channel 8-bit signed to 16-bit signed Laplace filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eMaskSize Enumeration value specifying the mask size.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterLaplace_8s16s_C1R(const Npp8s * pSrc, Npp32s nSrcStep, Npp16s * pDst, Npp32s nDstStep, NppiSize oSizeROI,
                            NppiMaskSize eMaskSize);

/** @} FilterLaplace */

/** @name FilterGauss
 *
 * Filters the image using a Gaussian filter kernel:
 *
 * Note that all FilterGauss functions currently support mask sizes up to 15x15.
 *
 * \f[
 *  \left( \begin{array}{rrr}
 *      1/16  & 2/16  & 1/16 \\
 *      2/16  & 4/16  & 2/16 \\
 *      1/16  & 2/16  & 1/16 \\
 *  \end{array} \right)
 *  \left( \begin{array}{rrrrr}
 *       2/571 &  7/571 &  12/571 &  7/571 &  2/571 \\
 *       7/571 & 31/571 &  52/571 & 31/571 &  7/571 \\
 *      12/571 & 52/571 & 127/571 & 52/571 & 12/571 \\
 *       7/571 & 31/571 &  52/571 & 31/571 &  7/571 \\
 *       2/571 &  7/571 &  12/571 &  7/571 &  2/571 \\
 *  \end{array} \right)
 * \f]
 *
 * @{
 *
 */

/**
 * Single channel 8-bit unsigned Gauss filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eMaskSize Enumeration value specifying the mask size.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterGauss_8u_C1R(const Npp8u * pSrc, Npp32s nSrcStep, Npp8u * pDst, Npp32s nDstStep, NppiSize oSizeROI,
                       NppiMaskSize eMaskSize);

/**
 * Three channel 8-bit unsigned Gauss filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eMaskSize Enumeration value specifying the mask size.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterGauss_8u_C3R(const Npp8u * pSrc, Npp32s nSrcStep, Npp8u * pDst, Npp32s nDstStep, NppiSize oSizeROI,
                       NppiMaskSize eMaskSize);

/**
 * Four channel 8-bit unsigned Gauss filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eMaskSize Enumeration value specifying the mask size.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterGauss_8u_C4R(const Npp8u * pSrc, Npp32s nSrcStep, Npp8u * pDst, Npp32s nDstStep, NppiSize oSizeROI,
                       NppiMaskSize eMaskSize);

/**
 * Four channel 8-bit unsigned Gauss filter, ignoring alpha channel.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eMaskSize Enumeration value specifying the mask size.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterGauss_8u_AC4R(const Npp8u * pSrc, Npp32s nSrcStep, Npp8u * pDst, Npp32s nDstStep, NppiSize oSizeROI,
                        NppiMaskSize eMaskSize);

/**
 * Single channel 16-bit unsigned Gauss filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eMaskSize Enumeration value specifying the mask size.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterGauss_16u_C1R(const Npp16u * pSrc, Npp32s nSrcStep, Npp16u * pDst, Npp32s nDstStep, NppiSize oSizeROI,
                        NppiMaskSize eMaskSize);

/**
 * Three channel 16-bit unsigned Gauss filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eMaskSize Enumeration value specifying the mask size.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterGauss_16u_C3R(const Npp16u * pSrc, Npp32s nSrcStep, Npp16u * pDst, Npp32s nDstStep, NppiSize oSizeROI,
                        NppiMaskSize eMaskSize);

/**
 * Four channel 16-bit unsigned Gauss filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eMaskSize Enumeration value specifying the mask size.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterGauss_16u_C4R(const Npp16u * pSrc, Npp32s nSrcStep, Npp16u * pDst, Npp32s nDstStep, NppiSize oSizeROI,
                        NppiMaskSize eMaskSize);

/**
 * Four channel 16-bit unsigned Gauss filter, ignoring alpha channel.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eMaskSize Enumeration value specifying the mask size.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterGauss_16u_AC4R(const Npp16u * pSrc, Npp32s nSrcStep, Npp16u * pDst, Npp32s nDstStep, NppiSize oSizeROI,
                         NppiMaskSize eMaskSize);

/**
 * Single channel 16-bit signed Gauss filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eMaskSize Enumeration value specifying the mask size.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterGauss_16s_C1R(const Npp16s * pSrc, Npp32s nSrcStep, Npp16s * pDst, Npp32s nDstStep, NppiSize oSizeROI,
                        NppiMaskSize eMaskSize);

/**
 * Three channel 16-bit signed Gauss filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eMaskSize Enumeration value specifying the mask size.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterGauss_16s_C3R(const Npp16s * pSrc, Npp32s nSrcStep, Npp16s * pDst, Npp32s nDstStep, NppiSize oSizeROI,
                        NppiMaskSize eMaskSize);

/**
 * Four channel 16-bit signed Gauss filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eMaskSize Enumeration value specifying the mask size.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterGauss_16s_C4R(const Npp16s * pSrc, Npp32s nSrcStep, Npp16s * pDst, Npp32s nDstStep, NppiSize oSizeROI,
                        NppiMaskSize eMaskSize);

/**
 * Four channel 16-bit signed Gauss filter, ignoring alpha channel.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eMaskSize Enumeration value specifying the mask size.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterGauss_16s_AC4R(const Npp16s * pSrc, Npp32s nSrcStep, Npp16s * pDst, Npp32s nDstStep, NppiSize oSizeROI,
                         NppiMaskSize eMaskSize);

/**
 * Single channel 32-bit floating-point Gauss filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eMaskSize Enumeration value specifying the mask size.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterGauss_32f_C1R(const Npp32f * pSrc, Npp32s nSrcStep, Npp32f * pDst, Npp32s nDstStep, NppiSize oSizeROI,
                        NppiMaskSize eMaskSize);

/**
 * Three channel 32-bit floating-point Gauss filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eMaskSize Enumeration value specifying the mask size.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterGauss_32f_C3R(const Npp32f * pSrc, Npp32s nSrcStep, Npp32f * pDst, Npp32s nDstStep, NppiSize oSizeROI,
                        NppiMaskSize eMaskSize);

/**
 * Four channel 32-bit floating-point Gauss filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eMaskSize Enumeration value specifying the mask size.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterGauss_32f_C4R(const Npp32f * pSrc, Npp32s nSrcStep, Npp32f * pDst, Npp32s nDstStep, NppiSize oSizeROI,
                        NppiMaskSize eMaskSize);

/**
 * Four channel 32-bit floating-point Gauss filter, ignoring alpha channel.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eMaskSize Enumeration value specifying the mask size.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterGauss_32f_AC4R(const Npp32f * pSrc, Npp32s nSrcStep, Npp32f * pDst, Npp32s nDstStep, NppiSize oSizeROI,
                         NppiMaskSize eMaskSize);

/** @} FilterGauss */

/** @name FilterGaussBorder
 *
 * If any portion of the mask overlaps the source
 * image boundary the requested border type operation is applied to all mask pixels
 * which fall outside of the source image.
 *
 * Currently only the NPP_BORDER_REPLICATE border type operation is supported.
 *
 * Note that all FilterGaussBorder functions currently support mask sizes up to 15x15.
 *
 * Filters the image using a Gaussian filter kernel:
 *
 * \f[
 *  \left( \begin{array}{rrr}
 *      1/16  & 2/16  & 1/16 \\
 *      2/16  & 4/16  & 2/16 \\
 *      1/16  & 2/16  & 1/16 \\
 *  \end{array} \right)
 *  \left( \begin{array}{rrrrr}
 *       2/571 &  7/571 &  12/571 &  7/571 &  2/571 \\
 *       7/571 & 31/571 &  52/571 & 31/571 &  7/571 \\
 *      12/571 & 52/571 & 127/571 & 52/571 & 12/571 \\
 *       7/571 & 31/571 &  52/571 & 31/571 &  7/571 \\
 *       2/571 &  7/571 &  12/571 &  7/571 &  2/571 \\
 *  \end{array} \right)
 * \f]
 *
 * @{
 *
 */

/**
 * Single channel 8-bit unsigned Gauss filter with border control.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eMaskSize Enumeration value specifying the mask size.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterGaussBorder_8u_C1R(const Npp8u * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp8u * pDst, Npp32s nDstStep, NppiSize oSizeROI,
                             NppiMaskSize eMaskSize, NppiBorderType eBorderType);

/**
 * Three channel 8-bit unsigned Gauss filter with border control.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eMaskSize Enumeration value specifying the mask size.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterGaussBorder_8u_C3R(const Npp8u * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp8u * pDst, Npp32s nDstStep, NppiSize oSizeROI,
                             NppiMaskSize eMaskSize, NppiBorderType eBorderType);

/**
 * Four channel 8-bit unsigned Gauss filter with border control.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eMaskSize Enumeration value specifying the mask size.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterGaussBorder_8u_C4R(const Npp8u * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp8u * pDst, Npp32s nDstStep, NppiSize oSizeROI,
                             NppiMaskSize eMaskSize, NppiBorderType eBorderType);

/**
 * Four channel 8-bit unsigned Gauss filter with border control, ignoring alpha channel.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eMaskSize Enumeration value specifying the mask size.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterGaussBorder_8u_AC4R(const Npp8u * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp8u * pDst, Npp32s nDstStep, NppiSize oSizeROI,
                              NppiMaskSize eMaskSize, NppiBorderType eBorderType);

/**
 * Single channel 16-bit unsigned Gauss filter with border control.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eMaskSize Enumeration value specifying the mask size.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterGaussBorder_16u_C1R(const Npp16u * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp16u * pDst, Npp32s nDstStep, NppiSize oSizeROI,
                              NppiMaskSize eMaskSize, NppiBorderType eBorderType);

/**
 * Three channel 16-bit unsigned Gauss filter with border control.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eMaskSize Enumeration value specifying the mask size.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterGaussBorder_16u_C3R(const Npp16u * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp16u * pDst, Npp32s nDstStep, NppiSize oSizeROI,
                              NppiMaskSize eMaskSize, NppiBorderType eBorderType);

/**
 * Four channel 16-bit unsigned Gauss filter with border control.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eMaskSize Enumeration value specifying the mask size.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterGaussBorder_16u_C4R(const Npp16u * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp16u * pDst, Npp32s nDstStep, NppiSize oSizeROI,
                              NppiMaskSize eMaskSize, NppiBorderType eBorderType);

/**
 * Four channel 16-bit unsigned Gauss filter with border control, ignoring alpha channel.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eMaskSize Enumeration value specifying the mask size.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterGaussBorder_16u_AC4R(const Npp16u * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp16u * pDst, Npp32s nDstStep, NppiSize oSizeROI,
                               NppiMaskSize eMaskSize, NppiBorderType eBorderType);

/**
 * Single channel 16-bit signed Gauss filter with border control.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eMaskSize Enumeration value specifying the mask size.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterGaussBorder_16s_C1R(const Npp16s * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp16s * pDst, Npp32s nDstStep, NppiSize oSizeROI,
                              NppiMaskSize eMaskSize, NppiBorderType eBorderType);

/**
 * Three channel 16-bit signed Gauss filter with border control.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eMaskSize Enumeration value specifying the mask size.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterGaussBorder_16s_C3R(const Npp16s * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp16s * pDst, Npp32s nDstStep, NppiSize oSizeROI,
                              NppiMaskSize eMaskSize, NppiBorderType eBorderType);

/**
 * Four channel 16-bit signed Gauss filter with border control.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eMaskSize Enumeration value specifying the mask size.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterGaussBorder_16s_C4R(const Npp16s * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp16s * pDst, Npp32s nDstStep, NppiSize oSizeROI,
                              NppiMaskSize eMaskSize, NppiBorderType eBorderType);

/**
 * Four channel 16-bit signed Gauss filter with border control, ignoring alpha channel.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eMaskSize Enumeration value specifying the mask size.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterGaussBorder_16s_AC4R(const Npp16s * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp16s * pDst, Npp32s nDstStep, NppiSize oSizeROI,
                               NppiMaskSize eMaskSize, NppiBorderType eBorderType);

/**
 * Single channel 32-bit floating-point Gauss filter with border control.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eMaskSize Enumeration value specifying the mask size.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterGaussBorder_32f_C1R(const Npp32f * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp32f * pDst, Npp32s nDstStep, NppiSize oSizeROI,
                              NppiMaskSize eMaskSize, NppiBorderType eBorderType);

/**
 * Three channel 32-bit floating-point Gauss filter with border control.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eMaskSize Enumeration value specifying the mask size.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterGaussBorder_32f_C3R(const Npp32f * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp32f * pDst, Npp32s nDstStep, NppiSize oSizeROI,
                              NppiMaskSize eMaskSize, NppiBorderType eBorderType);

/**
 * Four channel 32-bit floating-point Gauss filter with border control.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eMaskSize Enumeration value specifying the mask size.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterGaussBorder_32f_C4R(const Npp32f * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp32f * pDst, Npp32s nDstStep, NppiSize oSizeROI,
                              NppiMaskSize eMaskSize, NppiBorderType eBorderType);

/**
 * Four channel 32-bit floating-point Gauss filter with border control, ignoring alpha channel.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eMaskSize Enumeration value specifying the mask size.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterGaussBorder_32f_AC4R(const Npp32f * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp32f * pDst, Npp32s nDstStep, NppiSize oSizeROI,
                               NppiMaskSize eMaskSize, NppiBorderType eBorderType);

/** @} FilterGaussBorder */

/** @name FilterHighPass
 *
 * Filters the image using a high-pass filter kernel:
 *
 * \f[
 *  \left( \begin{array}{rrr}
 *      -1 & -1 & -1 \\
 *      -1 &  8 & -1 \\
 *      -1 & -1 & -1 \\
 *  \end{array} \right)
 *  \left( \begin{array}{rrrrr}
 *      -1 & -1 & -1 & -1 & -1 \\
 *      -1 & -1 & -1 & -1 & -1 \\
 *      -1 & -1 & 24 & -1 & -1 \\
 *      -1 & -1 & -1 & -1 & -1 \\
 *      -1 & -1 & -1 & -1 & -1 \\
 *  \end{array} \right)
 * \f]
 *
 * @{
 *
 */

/**
 * Single channel 8-bit unsigned high-pass filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eMaskSize Enumeration value specifying the mask size.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterHighPass_8u_C1R(const Npp8u * pSrc, Npp32s nSrcStep, Npp8u * pDst, Npp32s nDstStep, NppiSize oSizeROI,
                          NppiMaskSize eMaskSize);

/**
 * Three channel 8-bit unsigned high-pass filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eMaskSize Enumeration value specifying the mask size.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterHighPass_8u_C3R(const Npp8u * pSrc, Npp32s nSrcStep, Npp8u * pDst, Npp32s nDstStep, NppiSize oSizeROI,
                          NppiMaskSize eMaskSize);

/**
 * Four channel 8-bit unsigned high-pass filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eMaskSize Enumeration value specifying the mask size.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterHighPass_8u_C4R(const Npp8u * pSrc, Npp32s nSrcStep, Npp8u * pDst, Npp32s nDstStep, NppiSize oSizeROI,
                          NppiMaskSize eMaskSize);

/**
 * Four channel 8-bit unsigned high-pass filter, ignoring alpha channel.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eMaskSize Enumeration value specifying the mask size.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterHighPass_8u_AC4R(const Npp8u * pSrc, Npp32s nSrcStep, Npp8u * pDst, Npp32s nDstStep, NppiSize oSizeROI,
                           NppiMaskSize eMaskSize);

/**
 * Single channel 16-bit unsigned high-pass filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eMaskSize Enumeration value specifying the mask size.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterHighPass_16u_C1R(const Npp16u * pSrc, Npp32s nSrcStep, Npp16u * pDst, Npp32s nDstStep, NppiSize oSizeROI,
                           NppiMaskSize eMaskSize);

/**
 * Three channel 16-bit unsigned high-pass filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eMaskSize Enumeration value specifying the mask size.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterHighPass_16u_C3R(const Npp16u * pSrc, Npp32s nSrcStep, Npp16u * pDst, Npp32s nDstStep, NppiSize oSizeROI,
                           NppiMaskSize eMaskSize);

/**
 * Four channel 16-bit unsigned high-pass filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eMaskSize Enumeration value specifying the mask size.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterHighPass_16u_C4R(const Npp16u * pSrc, Npp32s nSrcStep, Npp16u * pDst, Npp32s nDstStep, NppiSize oSizeROI,
                           NppiMaskSize eMaskSize);

/**
 * Four channel 16-bit unsigned high-pass filter, ignoring alpha channel.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eMaskSize Enumeration value specifying the mask size.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterHighPass_16u_AC4R(const Npp16u * pSrc, Npp32s nSrcStep, Npp16u * pDst, Npp32s nDstStep, NppiSize oSizeROI,
                            NppiMaskSize eMaskSize);

/**
 * Single channel 16-bit signed high-pass filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eMaskSize Enumeration value specifying the mask size.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterHighPass_16s_C1R(const Npp16s * pSrc, Npp32s nSrcStep, Npp16s * pDst, Npp32s nDstStep, NppiSize oSizeROI,
                           NppiMaskSize eMaskSize);

/**
 * Three channel 16-bit signed high-pass filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eMaskSize Enumeration value specifying the mask size.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterHighPass_16s_C3R(const Npp16s * pSrc, Npp32s nSrcStep, Npp16s * pDst, Npp32s nDstStep, NppiSize oSizeROI,
                           NppiMaskSize eMaskSize);

/**
 * Four channel 16-bit signed high-pass filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eMaskSize Enumeration value specifying the mask size.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterHighPass_16s_C4R(const Npp16s * pSrc, Npp32s nSrcStep, Npp16s * pDst, Npp32s nDstStep, NppiSize oSizeROI,
                           NppiMaskSize eMaskSize);

/**
 * Four channel 16-bit signed high-pass filter, ignoring alpha channel.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eMaskSize Enumeration value specifying the mask size.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterHighPass_16s_AC4R(const Npp16s * pSrc, Npp32s nSrcStep, Npp16s * pDst, Npp32s nDstStep, NppiSize oSizeROI,
                            NppiMaskSize eMaskSize);

/**
 * Single channel 32-bit floating-point high-pass filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eMaskSize Enumeration value specifying the mask size.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterHighPass_32f_C1R(const Npp32f * pSrc, Npp32s nSrcStep, Npp32f * pDst, Npp32s nDstStep, NppiSize oSizeROI,
                           NppiMaskSize eMaskSize);

/**
 * Three channel 32-bit floating-point high-pass filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eMaskSize Enumeration value specifying the mask size.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterHighPass_32f_C3R(const Npp32f * pSrc, Npp32s nSrcStep, Npp32f * pDst, Npp32s nDstStep, NppiSize oSizeROI,
                           NppiMaskSize eMaskSize);

/**
 * Four channel 32-bit floating-point high-pass filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eMaskSize Enumeration value specifying the mask size.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterHighPass_32f_C4R(const Npp32f * pSrc, Npp32s nSrcStep, Npp32f * pDst, Npp32s nDstStep, NppiSize oSizeROI,
                           NppiMaskSize eMaskSize);

/**
 * Four channel 32-bit floating-point high-pass filter, ignoring alpha channel.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eMaskSize Enumeration value specifying the mask size.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterHighPass_32f_AC4R(const Npp32f * pSrc, Npp32s nSrcStep, Npp32f * pDst, Npp32s nDstStep, NppiSize oSizeROI,
                            NppiMaskSize eMaskSize);

/** @} FilterHighPass */

/** @name FilterLowPass
 *
 * Filters the image using a low-pass filter kernel:
 *
 * \f[
 *  \left( \begin{array}{rrr}
 *      1/9 & 1/9 & 1/9 \\
 *      1/9 & 1/9 & 1/9 \\
 *      1/9 & 1/9 & 1/9 \\
 *  \end{array} \right)
 *  \left( \begin{array}{rrrrr}
 *      1/25 & 1/25 & 1/25 & 1/25 & 1/25 \\
 *      1/25 & 1/25 & 1/25 & 1/25 & 1/25 \\
 *      1/25 & 1/25 & 1/25 & 1/25 & 1/25 \\
 *      1/25 & 1/25 & 1/25 & 1/25 & 1/25 \\
 *      1/25 & 1/25 & 1/25 & 1/25 & 1/25 \\
 *  \end{array} \right)
 * \f]
 *
 * @{
 *
 */

/**
 * Single channel 8-bit unsigned low-pass filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eMaskSize Enumeration value specifying the mask size.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterLowPass_8u_C1R(const Npp8u * pSrc, Npp32s nSrcStep, Npp8u * pDst, Npp32s nDstStep, NppiSize oSizeROI,
                         NppiMaskSize eMaskSize);

/**
 * Three channel 8-bit unsigned low-pass filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eMaskSize Enumeration value specifying the mask size.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterLowPass_8u_C3R(const Npp8u * pSrc, Npp32s nSrcStep, Npp8u * pDst, Npp32s nDstStep, NppiSize oSizeROI,
                         NppiMaskSize eMaskSize);

/**
 * Four channel 8-bit unsigned low-pass filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eMaskSize Enumeration value specifying the mask size.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterLowPass_8u_C4R(const Npp8u * pSrc, Npp32s nSrcStep, Npp8u * pDst, Npp32s nDstStep, NppiSize oSizeROI,
                         NppiMaskSize eMaskSize);

/**
 * Four channel 8-bit unsigned low-pass filter, ignoring alpha channel.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eMaskSize Enumeration value specifying the mask size.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterLowPass_8u_AC4R(const Npp8u * pSrc, Npp32s nSrcStep, Npp8u * pDst, Npp32s nDstStep, NppiSize oSizeROI,
                          NppiMaskSize eMaskSize);

/**
 * Single channel 16-bit unsigned low-pass filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eMaskSize Enumeration value specifying the mask size.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterLowPass_16u_C1R(const Npp16u * pSrc, Npp32s nSrcStep, Npp16u * pDst, Npp32s nDstStep, NppiSize oSizeROI,
                          NppiMaskSize eMaskSize);

/**
 * Three channel 16-bit unsigned low-pass filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eMaskSize Enumeration value specifying the mask size.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterLowPass_16u_C3R(const Npp16u * pSrc, Npp32s nSrcStep, Npp16u * pDst, Npp32s nDstStep, NppiSize oSizeROI,
                          NppiMaskSize eMaskSize);

/**
 * Four channel 16-bit unsigned low-pass filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eMaskSize Enumeration value specifying the mask size.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterLowPass_16u_C4R(const Npp16u * pSrc, Npp32s nSrcStep, Npp16u * pDst, Npp32s nDstStep, NppiSize oSizeROI,
                          NppiMaskSize eMaskSize);

/**
 * Four channel 16-bit unsigned low-pass filter, ignoring alpha channel.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eMaskSize Enumeration value specifying the mask size.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterLowPass_16u_AC4R(const Npp16u * pSrc, Npp32s nSrcStep, Npp16u * pDst, Npp32s nDstStep, NppiSize oSizeROI,
                           NppiMaskSize eMaskSize);

/**
 * Single channel 16-bit signed low-pass filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eMaskSize Enumeration value specifying the mask size.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterLowPass_16s_C1R(const Npp16s * pSrc, Npp32s nSrcStep, Npp16s * pDst, Npp32s nDstStep, NppiSize oSizeROI,
                          NppiMaskSize eMaskSize);

/**
 * Three channel 16-bit signed low-pass filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eMaskSize Enumeration value specifying the mask size.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterLowPass_16s_C3R(const Npp16s * pSrc, Npp32s nSrcStep, Npp16s * pDst, Npp32s nDstStep, NppiSize oSizeROI,
                          NppiMaskSize eMaskSize);

/**
 * Four channel 16-bit signed low-pass filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eMaskSize Enumeration value specifying the mask size.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterLowPass_16s_C4R(const Npp16s * pSrc, Npp32s nSrcStep, Npp16s * pDst, Npp32s nDstStep, NppiSize oSizeROI,
                          NppiMaskSize eMaskSize);

/**
 * Four channel 16-bit signed low-pass filter, ignoring alpha channel.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eMaskSize Enumeration value specifying the mask size.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterLowPass_16s_AC4R(const Npp16s * pSrc, Npp32s nSrcStep, Npp16s * pDst, Npp32s nDstStep, NppiSize oSizeROI,
                           NppiMaskSize eMaskSize);

/**
 * Single channel 32-bit floating-point low-pass filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eMaskSize Enumeration value specifying the mask size.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterLowPass_32f_C1R(const Npp32f * pSrc, Npp32s nSrcStep, Npp32f * pDst, Npp32s nDstStep, NppiSize oSizeROI,
                          NppiMaskSize eMaskSize);

/**
 * Three channel 32-bit floating-point low-pass filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eMaskSize Enumeration value specifying the mask size.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterLowPass_32f_C3R(const Npp32f * pSrc, Npp32s nSrcStep, Npp32f * pDst, Npp32s nDstStep, NppiSize oSizeROI,
                          NppiMaskSize eMaskSize);

/**
 * Four channel 32-bit floating-point low-pass filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eMaskSize Enumeration value specifying the mask size.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterLowPass_32f_C4R(const Npp32f * pSrc, Npp32s nSrcStep, Npp32f * pDst, Npp32s nDstStep, NppiSize oSizeROI,
                          NppiMaskSize eMaskSize);

/**
 * Four channel 32-bit floating-point high-pass filter, ignoring alpha channel.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param eMaskSize Enumeration value specifying the mask size.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterLowPass_32f_AC4R(const Npp32f * pSrc, Npp32s nSrcStep, Npp32f * pDst, Npp32s nDstStep, NppiSize oSizeROI,
                           NppiMaskSize eMaskSize);

/** @} FilterHighPass */

/** @name FilterSharpen
 *
 * Filters the image using a sharpening filter kernel:
 *
 * \f[
 *  \left( \begin{array}{rrr}
 *      -1/8 & -1/8 & -1/8 \\
 *      -1/8 & 16/8 & -1/8 \\
 *      -1/8 & -1/8 & -1/8 \\
 *  \end{array} \right)
 * \f]
 *
 * @{
 *
 */

/**
 * Single channel 8-bit unsigned sharpening filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterSharpen_8u_C1R(const Npp8u * pSrc, Npp32s nSrcStep, Npp8u * pDst, Npp32s nDstStep, NppiSize oSizeROI);

/**
 * Three channel 8-bit unsigned sharpening filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterSharpen_8u_C3R(const Npp8u * pSrc, Npp32s nSrcStep, Npp8u * pDst, Npp32s nDstStep, NppiSize oSizeROI);

/**
 * Four channel 8-bit unsigned sharpening filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterSharpen_8u_C4R(const Npp8u * pSrc, Npp32s nSrcStep, Npp8u * pDst, Npp32s nDstStep, NppiSize oSizeROI);

/**
 * Four channel 8-bit unsigned sharpening filter, ignoring alpha channel.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterSharpen_8u_AC4R(const Npp8u * pSrc, Npp32s nSrcStep, Npp8u * pDst, Npp32s nDstStep, NppiSize oSizeROI);

/**
 * Single channel 16-bit unsigned sharpening filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterSharpen_16u_C1R(const Npp16u * pSrc, Npp32s nSrcStep, Npp16u * pDst, Npp32s nDstStep, NppiSize oSizeROI);

/**
 * Three channel 16-bit unsigned sharpening filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterSharpen_16u_C3R(const Npp16u * pSrc, Npp32s nSrcStep, Npp16u * pDst, Npp32s nDstStep, NppiSize oSizeROI);

/**
 * Four channel 16-bit unsigned sharpening filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterSharpen_16u_C4R(const Npp16u * pSrc, Npp32s nSrcStep, Npp16u * pDst, Npp32s nDstStep, NppiSize oSizeROI);

/**
 * Four channel 16-bit unsigned sharpening filter, ignoring alpha channel.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterSharpen_16u_AC4R(const Npp16u * pSrc, Npp32s nSrcStep, Npp16u * pDst, Npp32s nDstStep, NppiSize oSizeROI);

/**
 * Single channel 16-bit signed sharpening filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterSharpen_16s_C1R(const Npp16s * pSrc, Npp32s nSrcStep, Npp16s * pDst, Npp32s nDstStep, NppiSize oSizeROI);

/**
 * Three channel 16-bit signed sharpening filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterSharpen_16s_C3R(const Npp16s * pSrc, Npp32s nSrcStep, Npp16s * pDst, Npp32s nDstStep, NppiSize oSizeROI);

/**
 * Four channel 16-bit signed sharpening filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterSharpen_16s_C4R(const Npp16s * pSrc, Npp32s nSrcStep, Npp16s * pDst, Npp32s nDstStep, NppiSize oSizeROI);

/**
 * Four channel 16-bit signed sharpening filter, ignoring alpha channel.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterSharpen_16s_AC4R(const Npp16s * pSrc, Npp32s nSrcStep, Npp16s * pDst, Npp32s nDstStep, NppiSize oSizeROI);

/**
 * Single channel 32-bit floating-point sharpening filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterSharpen_32f_C1R(const Npp32f * pSrc, Npp32s nSrcStep, Npp32f * pDst, Npp32s nDstStep, NppiSize oSizeROI);

/**
 * Three channel 32-bit floating-point sharpening filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterSharpen_32f_C3R(const Npp32f * pSrc, Npp32s nSrcStep, Npp32f * pDst, Npp32s nDstStep, NppiSize oSizeROI);

/**
 * Four channel 32-bit floating-point sharpening filter.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterSharpen_32f_C4R(const Npp32f * pSrc, Npp32s nSrcStep, Npp32f * pDst, Npp32s nDstStep, NppiSize oSizeROI);

/**
 * Four channel 32-bit floating-point sharpening filter, ignoring alpha channel.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiFilterSharpen_32f_AC4R(const Npp32f * pSrc, Npp32s nSrcStep, Npp32f * pDst, Npp32s nDstStep, NppiSize oSizeROI);

/** @} FilterSharpen */


/** @} fixed_filters */


/** @} image_filtering_functions */

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* NV_NPPI_FILTERING_FUNCTIONS_H */
