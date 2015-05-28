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
#ifndef NV_NPPI_MORPHOLOGICAL_OPERATIONS_H
#define NV_NPPI_MORPHOLOGICAL_OPERATIONS_H
 
/**
 * \file nppi_morphological_operations.h
 * NPP Image Processing Functionality.
 */
 
#include "nppdefs.h"


#ifdef __cplusplus
extern "C" {
#endif

/** @defgroup image_morphological_operations Morphological Operations
 *  @ingroup nppi
 *
 * Morphological image operations. 
 *
 * Morphological operations are classified as \ref neighborhood_operations. 
 *
 * @{
 *
 */

/** @defgroup image_dilate Dilation
 *
 * Dilation computes the output pixel as the maximum pixel value of the pixels
 * under the mask. Pixels who's corresponding mask values are zero do not 
 * participate in the maximum search.
 *
 * It is the user's responsibility to avoid \ref sampling_beyond_image_boundaries.
 *
 * @{
 *
 */

/**
 * Single-channel 8-bit unsigned integer dilation.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pMask Pointer to the start address of the mask array
 * \param oMaskSize Width and Height mask array.
 * \param oAnchor X and Y offsets of the mask origin frame of reference
 *        w.r.t the source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiDilate_8u_C1R(const Npp8u * pSrc, Npp32s nSrcStep, Npp8u * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                  const Npp8u * pMask, NppiSize oMaskSize, NppiPoint oAnchor);

/**
 * Three-channel 8-bit unsigned integer dilation.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pMask Pointer to the start address of the mask array
 * \param oMaskSize Width and Height mask array.
 * \param oAnchor X and Y offsets of the mask origin frame of reference
 *        w.r.t the source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiDilate_8u_C3R(const Npp8u * pSrc, Npp32s nSrcStep, Npp8u * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                  const Npp8u * pMask, NppiSize oMaskSize, NppiPoint oAnchor);

/**
 * Four-channel 8-bit unsigned integer dilation.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pMask Pointer to the start address of the mask array
 * \param oMaskSize Width and Height mask array.
 * \param oAnchor X and Y offsets of the mask origin frame of reference
 *        w.r.t the source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus
nppiDilate_8u_C4R(const Npp8u * pSrc, int nSrcStep, Npp8u * pDst, int nDstStep, NppiSize oSizeROI,
                  const Npp8u * pMask, NppiSize oMaskSize, NppiPoint oAnchor);

/**
 * Four-channel 8-bit unsigned integer dilation, ignoring alpha-channel.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pMask Pointer to the start address of the mask array
 * \param oMaskSize Width and Height mask array.
 * \param oAnchor X and Y offsets of the mask origin frame of reference
 *        w.r.t the source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus
nppiDilate_8u_AC4R(const Npp8u * pSrc, int nSrcStep, Npp8u * pDst, int nDstStep, NppiSize oSizeROI,
                   const Npp8u * pMask, NppiSize oMaskSize, NppiPoint oAnchor);


/**
 * Single-channel 16-bit unsigned integer dilation.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pMask Pointer to the start address of the mask array
 * \param oMaskSize Width and Height mask array.
 * \param oAnchor X and Y offsets of the mask origin frame of reference
 *        w.r.t the source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiDilate_16u_C1R(const Npp16u * pSrc, Npp32s nSrcStep, Npp16u * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                   const Npp8u * pMask, NppiSize oMaskSize, NppiPoint oAnchor);

/**
 * Three-channel 16-bit unsigned integer dilation.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pMask Pointer to the start address of the mask array
 * \param oMaskSize Width and Height mask array.
 * \param oAnchor X and Y offsets of the mask origin frame of reference
 *        w.r.t the source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiDilate_16u_C3R(const Npp16u * pSrc, Npp32s nSrcStep, Npp16u * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                   const Npp8u * pMask, NppiSize oMaskSize, NppiPoint oAnchor);

/**
 * Four-channel 16-bit unsigned integer dilation.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pMask Pointer to the start address of the mask array
 * \param oMaskSize Width and Height mask array.
 * \param oAnchor X and Y offsets of the mask origin frame of reference
 *        w.r.t the source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus
nppiDilate_16u_C4R(const Npp16u * pSrc, int nSrcStep, Npp16u * pDst, int nDstStep, NppiSize oSizeROI,
                   const Npp8u * pMask, NppiSize oMaskSize, NppiPoint oAnchor);

/**
 * Four-channel 16-bit unsigned integer dilation, ignoring alpha-channel.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pMask Pointer to the start address of the mask array
 * \param oMaskSize Width and Height mask array.
 * \param oAnchor X and Y offsets of the mask origin frame of reference
 *        w.r.t the source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus
nppiDilate_16u_AC4R(const Npp16u * pSrc, int nSrcStep, Npp16u * pDst, int nDstStep, NppiSize oSizeROI,
                    const Npp8u * pMask, NppiSize oMaskSize, NppiPoint oAnchor);


/**
 * Single-channel 32-bit floating-point dilation.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pMask Pointer to the start address of the mask array
 * \param oMaskSize Width and Height mask array.
 * \param oAnchor X and Y offsets of the mask origin frame of reference
 *        w.r.t the source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiDilate_32f_C1R(const Npp32f * pSrc, Npp32s nSrcStep, Npp32f * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                   const Npp8u * pMask, NppiSize oMaskSize, NppiPoint oAnchor);

/**
 * Three-channel 32-bit floating-point dilation.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pMask Pointer to the start address of the mask array
 * \param oMaskSize Width and Height mask array.
 * \param oAnchor X and Y offsets of the mask origin frame of reference
 *        w.r.t the source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiDilate_32f_C3R(const Npp32f * pSrc, Npp32s nSrcStep, Npp32f * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                   const Npp8u * pMask, NppiSize oMaskSize, NppiPoint oAnchor);

/**
 * Four-channel 32-bit floating-point dilation.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pMask Pointer to the start address of the mask array
 * \param oMaskSize Width and Height mask array.
 * \param oAnchor X and Y offsets of the mask origin frame of reference
 *        w.r.t the source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus
nppiDilate_32f_C4R(const Npp32f * pSrc, int nSrcStep, Npp32f * pDst, int nDstStep, NppiSize oSizeROI,
                   const Npp8u * pMask, NppiSize oMaskSize, NppiPoint oAnchor);

/**
 * Four-channel 32-bit floating-point dilation, ignoring alpha-channel.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pMask Pointer to the start address of the mask array
 * \param oMaskSize Width and Height mask array.
 * \param oAnchor X and Y offsets of the mask origin frame of reference
 *        w.r.t the source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus
nppiDilate_32f_AC4R(const Npp32f * pSrc, int nSrcStep, Npp32f * pDst, int nDstStep, NppiSize oSizeROI,
                    const Npp8u * pMask, NppiSize oMaskSize, NppiPoint oAnchor);


/** @} image_dilate */

/** @defgroup image_dilate_border Dilation with border control
 *
 * Dilation computes the output pixel as the maximum pixel value of the pixels
 * under the mask. Pixels who's corresponding mask values are zero do not 
 * participate in the maximum search.
 *
 * If any portion of the mask overlaps the source image boundary the requested border type 
 * operation is applied to all mask pixels which fall outside of the source image.
 *
 * Currently only the NPP_BORDER_REPLICATE border type operation is supported.
 *
 * @{
 *
 */

/**
 * Single-channel 8-bit unsigned integer dilation with border control.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pMask Pointer to the start address of the mask array
 * \param oMaskSize Width and Height mask array.
 * \param oAnchor X and Y offsets of the mask origin frame of reference
 *        w.r.t the source pixel.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiDilateBorder_8u_C1R(const Npp8u * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp8u * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                        const Npp8u * pMask, NppiSize oMaskSize, NppiPoint oAnchor, NppiBorderType eBorderType);

/**
 * Three-channel 8-bit unsigned integer dilation with border control.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pMask Pointer to the start address of the mask array
 * \param oMaskSize Width and Height mask array.
 * \param oAnchor X and Y offsets of the mask origin frame of reference
 *        w.r.t the source pixel.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiDilateBorder_8u_C3R(const Npp8u * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp8u * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                        const Npp8u * pMask, NppiSize oMaskSize, NppiPoint oAnchor, NppiBorderType eBorderType);

/**
 * Four-channel 8-bit unsigned integer dilation with border control.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pMask Pointer to the start address of the mask array
 * \param oMaskSize Width and Height mask array.
 * \param oAnchor X and Y offsets of the mask origin frame of reference
 *        w.r.t the source pixel.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus
nppiDilateBorder_8u_C4R(const Npp8u * pSrc, int nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp8u * pDst, int nDstStep, NppiSize oSizeROI,
                        const Npp8u * pMask, NppiSize oMaskSize, NppiPoint oAnchor, NppiBorderType eBorderType);

/**
 * Four-channel 8-bit unsigned integer dilation with border control, ignoring alpha-channel.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pMask Pointer to the start address of the mask array
 * \param oMaskSize Width and Height mask array.
 * \param oAnchor X and Y offsets of the mask origin frame of reference
 *        w.r.t the source pixel.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus
nppiDilateBorder_8u_AC4R(const Npp8u * pSrc, int nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp8u * pDst, int nDstStep, NppiSize oSizeROI,
                         const Npp8u * pMask, NppiSize oMaskSize, NppiPoint oAnchor, NppiBorderType eBorderType);


/**
 * Single-channel 16-bit unsigned integer dilation with border control.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pMask Pointer to the start address of the mask array
 * \param oMaskSize Width and Height mask array.
 * \param oAnchor X and Y offsets of the mask origin frame of reference
 *        w.r.t the source pixel.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiDilateBorder_16u_C1R(const Npp16u * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp16u * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                         const Npp8u * pMask, NppiSize oMaskSize, NppiPoint oAnchor, NppiBorderType eBorderType);

/**
 * Three-channel 16-bit unsigned integer dilation with border control.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pMask Pointer to the start address of the mask array
 * \param oMaskSize Width and Height mask array.
 * \param oAnchor X and Y offsets of the mask origin frame of reference
 *        w.r.t the source pixel.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiDilateBorder_16u_C3R(const Npp16u * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp16u * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                         const Npp8u * pMask, NppiSize oMaskSize, NppiPoint oAnchor, NppiBorderType eBorderType);

/**
 * Four-channel 16-bit unsigned integer dilation with border control.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pMask Pointer to the start address of the mask array
 * \param oMaskSize Width and Height mask array.
 * \param oAnchor X and Y offsets of the mask origin frame of reference
 *        w.r.t the source pixel.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus
nppiDilateBorder_16u_C4R(const Npp16u * pSrc, int nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp16u * pDst, int nDstStep, NppiSize oSizeROI,
                         const Npp8u * pMask, NppiSize oMaskSize, NppiPoint oAnchor, NppiBorderType eBorderType);

/**
 * Four-channel 16-bit unsigned integer dilation with border control, ignoring alpha-channel.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pMask Pointer to the start address of the mask array
 * \param oMaskSize Width and Height mask array.
 * \param oAnchor X and Y offsets of the mask origin frame of reference
 *        w.r.t the source pixel.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus
nppiDilateBorder_16u_AC4R(const Npp16u * pSrc, int nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp16u * pDst, int nDstStep, NppiSize oSizeROI,
                          const Npp8u * pMask, NppiSize oMaskSize, NppiPoint oAnchor, NppiBorderType eBorderType);


/**
 * Single-channel 32-bit floating-point dilation with border control.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pMask Pointer to the start address of the mask array
 * \param oMaskSize Width and Height mask array.
 * \param oAnchor X and Y offsets of the mask origin frame of reference
 *        w.r.t the source pixel.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiDilateBorder_32f_C1R(const Npp32f * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp32f * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                         const Npp8u * pMask, NppiSize oMaskSize, NppiPoint oAnchor, NppiBorderType eBorderType);

/**
 * Three-channel 32-bit floating-point dilation with border control.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pMask Pointer to the start address of the mask array
 * \param oMaskSize Width and Height mask array.
 * \param oAnchor X and Y offsets of the mask origin frame of reference
 *        w.r.t the source pixel.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiDilateBorder_32f_C3R(const Npp32f * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp32f * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                         const Npp8u * pMask, NppiSize oMaskSize, NppiPoint oAnchor, NppiBorderType eBorderType);

/**
 * Four-channel 32-bit floating-point dilation with border control.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pMask Pointer to the start address of the mask array
 * \param oMaskSize Width and Height mask array.
 * \param oAnchor X and Y offsets of the mask origin frame of reference
 *        w.r.t the source pixel.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus
nppiDilateBorder_32f_C4R(const Npp32f * pSrc, int nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp32f * pDst, int nDstStep, NppiSize oSizeROI,
                         const Npp8u * pMask, NppiSize oMaskSize, NppiPoint oAnchor, NppiBorderType eBorderType);

/**
 * Four-channel 32-bit floating-point dilation with border control, ignoring alpha-channel.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pMask Pointer to the start address of the mask array
 * \param oMaskSize Width and Height mask array.
 * \param oAnchor X and Y offsets of the mask origin frame of reference
 *        w.r.t the source pixel.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus
nppiDilateBorder_32f_AC4R(const Npp32f * pSrc, int nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp32f * pDst, int nDstStep, NppiSize oSizeROI,
                          const Npp8u * pMask, NppiSize oMaskSize, NppiPoint oAnchor, NppiBorderType eBorderType);


/** @} image_dilate_border */

/** @defgroup image_dilate_3x3 Dilate3x3
 *
 * Dilation using a 3x3 mask with the anchor at its center pixel.
 *
 * It is the user's responsibility to avoid \ref sampling_beyond_image_boundaries.
 *
 * @{
 *
 */

/**
 * Single-channel 8-bit unsigned integer 3x3 dilation.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiDilate3x3_8u_C1R(const Npp8u * pSrc, Npp32s nSrcStep, Npp8u * pDst, Npp32s nDstStep, NppiSize oSizeROI);

/**
 * Three-channel 8-bit unsigned integer 3x3 dilation.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiDilate3x3_8u_C3R(const Npp8u * pSrc, Npp32s nSrcStep, Npp8u * pDst, Npp32s nDstStep, NppiSize oSizeROI);

/**
 * Four-channel 8-bit unsigned integer 3x3 dilation.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus
nppiDilate3x3_8u_C4R(const Npp8u * pSrc, int nSrcStep, Npp8u * pDst, int nDstStep, NppiSize oSizeROI);

/**
 * Four-channel 8-bit unsigned integer 3x3 dilation, ignoring alpha-channel.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus
nppiDilate3x3_8u_AC4R(const Npp8u * pSrc, int nSrcStep, Npp8u * pDst, int nDstStep, NppiSize oSizeROI);


/**
 * Single-channel 16-bit unsigned integer 3x3 dilation.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiDilate3x3_16u_C1R(const Npp16u * pSrc, Npp32s nSrcStep, Npp16u * pDst, Npp32s nDstStep, NppiSize oSizeROI);

/**
 * Three-channel 16-bit unsigned integer 3x3 dilation.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiDilate3x3_16u_C3R(const Npp16u * pSrc, Npp32s nSrcStep, Npp16u * pDst, Npp32s nDstStep, NppiSize oSizeROI);

/**
 * Four-channel 16-bit unsigned integer 3x3 dilation.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus
nppiDilate3x3_16u_C4R(const Npp16u * pSrc, int nSrcStep, Npp16u * pDst, int nDstStep, NppiSize oSizeROI);

/**
 * Four-channel 16-bit unsigned integer 3x3 dilation, ignoring alpha-channel.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus
nppiDilate3x3_16u_AC4R(const Npp16u * pSrc, int nSrcStep, Npp16u * pDst, int nDstStep, NppiSize oSizeROI);


/**
 * Single-channel 32-bit floating-point 3x3 dilation.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiDilate3x3_32f_C1R(const Npp32f * pSrc, Npp32s nSrcStep, Npp32f * pDst, Npp32s nDstStep, NppiSize oSizeROI);

/**
 * Three-channel 32-bit floating-point 3x3 dilation.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiDilate3x3_32f_C3R(const Npp32f * pSrc, Npp32s nSrcStep, Npp32f * pDst, Npp32s nDstStep, NppiSize oSizeROI);

/**
 * Four-channel 32-bit floating-point 3x3 dilation.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus
nppiDilate3x3_32f_C4R(const Npp32f * pSrc, int nSrcStep, Npp32f * pDst, int nDstStep, NppiSize oSizeROI);

/**
 * Four-channel 32-bit floating-point 3x3 dilation, ignoring alpha-channel.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus
nppiDilate3x3_32f_AC4R(const Npp32f * pSrc, int nSrcStep, Npp32f * pDst, int nDstStep, NppiSize oSizeROI);

/**
 * Single-channel 64-bit floating-point 3x3 dilation.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiDilate3x3_64f_C1R(const Npp64f * pSrc, Npp32s nSrcStep, Npp64f * pDst, Npp32s nDstStep, NppiSize oSizeROI);

/** @} image_dilate_3x3 */

/** @defgroup image_dilate_3x3_border Dilate3x3Border
 *
 * Dilation using a 3x3 mask with the anchor at its center pixel with border control.
 *
 * If any portion of the mask overlaps the source image boundary the requested border type 
 * operation is applied to all mask pixels which fall outside of the source image.
 *
 * Currently only the NPP_BORDER_REPLICATE border type operation is supported.
 *
 * @{
 *
 */

/**
 * Single-channel 8-bit unsigned integer 3x3 dilation with border control.
 * 
 * \param pSrc  \ref source_image_pointer.
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
nppiDilate3x3Border_8u_C1R(const Npp8u * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp8u * pDst, Npp32s nDstStep, NppiSize oSizeROI, NppiBorderType eBorderType);

/**
 * Three-channel 8-bit unsigned integer 3x3 dilation with border control.
 * 
 * \param pSrc  \ref source_image_pointer.
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
nppiDilate3x3Border_8u_C3R(const Npp8u * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp8u * pDst, Npp32s nDstStep, NppiSize oSizeROI, NppiBorderType eBorderType);

/**
 * Four-channel 8-bit unsigned integer 3x3 dilation with border control.
 * 
 * \param pSrc  \ref source_image_pointer.
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
nppiDilate3x3Border_8u_C4R(const Npp8u * pSrc, int nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp8u * pDst, int nDstStep, NppiSize oSizeROI, NppiBorderType eBorderType);

/**
 * Four-channel 8-bit unsigned integer 3x3 dilation with border control, ignoring alpha-channel.
 * 
 * \param pSrc  \ref source_image_pointer.
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
nppiDilate3x3Border_8u_AC4R(const Npp8u * pSrc, int nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp8u * pDst, int nDstStep, NppiSize oSizeROI, NppiBorderType eBorderType);


/**
 * Single-channel 16-bit unsigned integer 3x3 dilation with border control.
 * 
 * \param pSrc  \ref source_image_pointer.
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
nppiDilate3x3Border_16u_C1R(const Npp16u * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp16u * pDst, Npp32s nDstStep, NppiSize oSizeROI, NppiBorderType eBorderType);

/**
 * Three-channel 16-bit unsigned integer 3x3 dilation with border control.
 * 
 * \param pSrc  \ref source_image_pointer.
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
nppiDilate3x3Border_16u_C3R(const Npp16u * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp16u * pDst, Npp32s nDstStep, NppiSize oSizeROI, NppiBorderType eBorderType);

/**
 * Four-channel 16-bit unsigned integer 3x3 dilation with border control.
 * 
 * \param pSrc  \ref source_image_pointer.
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
nppiDilate3x3Border_16u_C4R(const Npp16u * pSrc, int nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp16u * pDst, int nDstStep, NppiSize oSizeROI, NppiBorderType eBorderType);

/**
 * Four-channel 16-bit unsigned integer 3x3 dilation with border control, ignoring alpha-channel.
 * 
 * \param pSrc  \ref source_image_pointer.
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
nppiDilate3x3Border_16u_AC4R(const Npp16u * pSrc, int nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp16u * pDst, int nDstStep, NppiSize oSizeROI, NppiBorderType eBorderType);


/**
 * Single-channel 32-bit floating-point 3x3 dilation with border control.
 * 
 * \param pSrc  \ref source_image_pointer.
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
nppiDilate3x3Border_32f_C1R(const Npp32f * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp32f * pDst, Npp32s nDstStep, NppiSize oSizeROI, NppiBorderType eBorderType);

/**
 * Three-channel 32-bit floating-point 3x3 dilation with border control.
 * 
 * \param pSrc  \ref source_image_pointer.
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
nppiDilate3x3Border_32f_C3R(const Npp32f * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp32f * pDst, Npp32s nDstStep, NppiSize oSizeROI, NppiBorderType eBorderType);

/**
 * Four-channel 32-bit floating-point 3x3 dilation with border control.
 * 
 * \param pSrc  \ref source_image_pointer.
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
nppiDilate3x3Border_32f_C4R(const Npp32f * pSrc, int nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp32f * pDst, int nDstStep, NppiSize oSizeROI, NppiBorderType eBorderType);

/**
 * Four-channel 32-bit floating-point 3x3 dilation with border control, ignoring alpha-channel.
 * 
 * \param pSrc  \ref source_image_pointer.
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
nppiDilate3x3Border_32f_AC4R(const Npp32f * pSrc, int nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp32f * pDst, int nDstStep, NppiSize oSizeROI, NppiBorderType eBorderType);

/** @} image_dilate_3x3_border */

/** @defgroup image_erode Erode
 *
 * Erosion computes the output pixel as the minimum pixel value of the pixels
 * under the mask. Pixels who's corresponding mask values are zero do not 
 * participate in the maximum search.
 *
 * It is the user's responsibility to avoid \ref sampling_beyond_image_boundaries.
 *
 * @{
 *
 */


/**
 * Single-channel 8-bit unsigned integer erosion.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pMask Pointer to the start address of the mask array
 * \param oMaskSize Width and Height mask array.
 * \param oAnchor X and Y offsets of the mask origin frame of reference
 *        w.r.t the source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiErode_8u_C1R(const Npp8u * pSrc, Npp32s nSrcStep, Npp8u * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                  const Npp8u * pMask, NppiSize oMaskSize, NppiPoint oAnchor);

/**
 * Three-channel 8-bit unsigned integer erosion.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pMask Pointer to the start address of the mask array
 * \param oMaskSize Width and Height mask array.
 * \param oAnchor X and Y offsets of the mask origin frame of reference
 *        w.r.t the source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiErode_8u_C3R(const Npp8u * pSrc, Npp32s nSrcStep, Npp8u * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                  const Npp8u * pMask, NppiSize oMaskSize, NppiPoint oAnchor);

/**
 * Four-channel 8-bit unsigned integer erosion.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pMask Pointer to the start address of the mask array
 * \param oMaskSize Width and Height mask array.
 * \param oAnchor X and Y offsets of the mask origin frame of reference
 *        w.r.t the source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus
nppiErode_8u_C4R(const Npp8u * pSrc, int nSrcStep, Npp8u * pDst, int nDstStep, NppiSize oSizeROI,
                  const Npp8u * pMask, NppiSize oMaskSize, NppiPoint oAnchor);

/**
 * Four-channel 8-bit unsigned integer erosion, ignoring alpha-channel.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pMask Pointer to the start address of the mask array
 * \param oMaskSize Width and Height mask array.
 * \param oAnchor X and Y offsets of the mask origin frame of reference
 *        w.r.t the source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus
nppiErode_8u_AC4R(const Npp8u * pSrc, int nSrcStep, Npp8u * pDst, int nDstStep, NppiSize oSizeROI,
                   const Npp8u * pMask, NppiSize oMaskSize, NppiPoint oAnchor);


/**
 * Single-channel 16-bit unsigned integer erosion.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pMask Pointer to the start address of the mask array
 * \param oMaskSize Width and Height mask array.
 * \param oAnchor X and Y offsets of the mask origin frame of reference
 *        w.r.t the source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiErode_16u_C1R(const Npp16u * pSrc, Npp32s nSrcStep, Npp16u * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                   const Npp8u * pMask, NppiSize oMaskSize, NppiPoint oAnchor);

/**
 * Three-channel 16-bit unsigned integer erosion.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pMask Pointer to the start address of the mask array
 * \param oMaskSize Width and Height mask array.
 * \param oAnchor X and Y offsets of the mask origin frame of reference
 *        w.r.t the source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiErode_16u_C3R(const Npp16u * pSrc, Npp32s nSrcStep, Npp16u * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                   const Npp8u * pMask, NppiSize oMaskSize, NppiPoint oAnchor);

/**
 * Four-channel 16-bit unsigned integer erosion.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pMask Pointer to the start address of the mask array
 * \param oMaskSize Width and Height mask array.
 * \param oAnchor X and Y offsets of the mask origin frame of reference
 *        w.r.t the source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus
nppiErode_16u_C4R(const Npp16u * pSrc, int nSrcStep, Npp16u * pDst, int nDstStep, NppiSize oSizeROI,
                   const Npp8u * pMask, NppiSize oMaskSize, NppiPoint oAnchor);

/**
 * Four-channel 16-bit unsigned integer erosion, ignoring alpha-channel.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pMask Pointer to the start address of the mask array
 * \param oMaskSize Width and Height mask array.
 * \param oAnchor X and Y offsets of the mask origin frame of reference
 *        w.r.t the source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus
nppiErode_16u_AC4R(const Npp16u * pSrc, int nSrcStep, Npp16u * pDst, int nDstStep, NppiSize oSizeROI,
                    const Npp8u * pMask, NppiSize oMaskSize, NppiPoint oAnchor);


/**
 * Single-channel 32-bit floating-point erosion.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pMask Pointer to the start address of the mask array
 * \param oMaskSize Width and Height mask array.
 * \param oAnchor X and Y offsets of the mask origin frame of reference
 *        w.r.t the source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiErode_32f_C1R(const Npp32f * pSrc, Npp32s nSrcStep, Npp32f * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                   const Npp8u * pMask, NppiSize oMaskSize, NppiPoint oAnchor);

/**
 * Three-channel 32-bit floating-point erosion.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pMask Pointer to the start address of the mask array
 * \param oMaskSize Width and Height mask array.
 * \param oAnchor X and Y offsets of the mask origin frame of reference
 *        w.r.t the source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiErode_32f_C3R(const Npp32f * pSrc, Npp32s nSrcStep, Npp32f * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                   const Npp8u * pMask, NppiSize oMaskSize, NppiPoint oAnchor);

/**
 * Four-channel 32-bit floating-point erosion.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pMask Pointer to the start address of the mask array
 * \param oMaskSize Width and Height mask array.
 * \param oAnchor X and Y offsets of the mask origin frame of reference
 *        w.r.t the source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus
nppiErode_32f_C4R(const Npp32f * pSrc, int nSrcStep, Npp32f * pDst, int nDstStep, NppiSize oSizeROI,
                   const Npp8u * pMask, NppiSize oMaskSize, NppiPoint oAnchor);

/**
 * Four-channel 32-bit floating-point erosion, ignoring alpha-channel.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pMask Pointer to the start address of the mask array
 * \param oMaskSize Width and Height mask array.
 * \param oAnchor X and Y offsets of the mask origin frame of reference
 *        w.r.t the source pixel.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus
nppiErode_32f_AC4R(const Npp32f * pSrc, int nSrcStep, Npp32f * pDst, int nDstStep, NppiSize oSizeROI,
                    const Npp8u * pMask, NppiSize oMaskSize, NppiPoint oAnchor);


/** @} image_erode */

/** @defgroup image_erode_border Erosion with border control
 *
 * Erosion computes the output pixel as the minimum pixel value of the pixels
 * under the mask. Pixels who's corresponding mask values are zero do not 
 * participate in the minimum search.
 *
 * If any portion of the mask overlaps the source image boundary the requested border type 
 * operation is applied to all mask pixels which fall outside of the source image.
 *
 * Currently only the NPP_BORDER_REPLICATE border type operation is supported.
 *
 * @{
 *
 */

/**
 * Single-channel 8-bit unsigned integer erosion with border control.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pMask Pointer to the start address of the mask array
 * \param oMaskSize Width and Height mask array.
 * \param oAnchor X and Y offsets of the mask origin frame of reference
 *        w.r.t the source pixel.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiErodeBorder_8u_C1R(const Npp8u * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp8u * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                       const Npp8u * pMask, NppiSize oMaskSize, NppiPoint oAnchor, NppiBorderType eBorderType);

/**
 * Three-channel 8-bit unsigned integer erosion with border control.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pMask Pointer to the start address of the mask array
 * \param oMaskSize Width and Height mask array.
 * \param oAnchor X and Y offsets of the mask origin frame of reference
 *        w.r.t the source pixel.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiErodeBorder_8u_C3R(const Npp8u * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp8u * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                       const Npp8u * pMask, NppiSize oMaskSize, NppiPoint oAnchor, NppiBorderType eBorderType);

/**
 * Four-channel 8-bit unsigned integer erosion with border control.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pMask Pointer to the start address of the mask array
 * \param oMaskSize Width and Height mask array.
 * \param oAnchor X and Y offsets of the mask origin frame of reference
 *        w.r.t the source pixel.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus
nppiErodeBorder_8u_C4R(const Npp8u * pSrc, int nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp8u * pDst, int nDstStep, NppiSize oSizeROI,
                       const Npp8u * pMask, NppiSize oMaskSize, NppiPoint oAnchor, NppiBorderType eBorderType);

/**
 * Four-channel 8-bit unsigned integer erosion with border control, ignoring alpha-channel.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pMask Pointer to the start address of the mask array
 * \param oMaskSize Width and Height mask array.
 * \param oAnchor X and Y offsets of the mask origin frame of reference
 *        w.r.t the source pixel.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus
nppiErodeBorder_8u_AC4R(const Npp8u * pSrc, int nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp8u * pDst, int nDstStep, NppiSize oSizeROI,
                        const Npp8u * pMask, NppiSize oMaskSize, NppiPoint oAnchor, NppiBorderType eBorderType);


/**
 * Single-channel 16-bit unsigned integer erosion with border control.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pMask Pointer to the start address of the mask array
 * \param oMaskSize Width and Height mask array.
 * \param oAnchor X and Y offsets of the mask origin frame of reference
 *        w.r.t the source pixel.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiErodeBorder_16u_C1R(const Npp16u * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp16u * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                        const Npp8u * pMask, NppiSize oMaskSize, NppiPoint oAnchor, NppiBorderType eBorderType);

/**
 * Three-channel 16-bit unsigned integer erosion with border control.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pMask Pointer to the start address of the mask array
 * \param oMaskSize Width and Height mask array.
 * \param oAnchor X and Y offsets of the mask origin frame of reference
 *        w.r.t the source pixel.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiErodeBorder_16u_C3R(const Npp16u * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp16u * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                        const Npp8u * pMask, NppiSize oMaskSize, NppiPoint oAnchor, NppiBorderType eBorderType);

/**
 * Four-channel 16-bit unsigned integer erosion with border control.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pMask Pointer to the start address of the mask array
 * \param oMaskSize Width and Height mask array.
 * \param oAnchor X and Y offsets of the mask origin frame of reference
 *        w.r.t the source pixel.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus
nppiErodeBorder_16u_C4R(const Npp16u * pSrc, int nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp16u * pDst, int nDstStep, NppiSize oSizeROI,
                        const Npp8u * pMask, NppiSize oMaskSize, NppiPoint oAnchor, NppiBorderType eBorderType);

/**
 * Four-channel 16-bit unsigned integer erosion with border control, ignoring alpha-channel.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pMask Pointer to the start address of the mask array
 * \param oMaskSize Width and Height mask array.
 * \param oAnchor X and Y offsets of the mask origin frame of reference
 *        w.r.t the source pixel.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus
nppiErodeBorder_16u_AC4R(const Npp16u * pSrc, int nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp16u * pDst, int nDstStep, NppiSize oSizeROI,
                         const Npp8u * pMask, NppiSize oMaskSize, NppiPoint oAnchor, NppiBorderType eBorderType);


/**
 * Single-channel 32-bit floating-point erosion with border control.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pMask Pointer to the start address of the mask array
 * \param oMaskSize Width and Height mask array.
 * \param oAnchor X and Y offsets of the mask origin frame of reference
 *        w.r.t the source pixel.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiErodeBorder_32f_C1R(const Npp32f * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp32f * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                        const Npp8u * pMask, NppiSize oMaskSize, NppiPoint oAnchor, NppiBorderType eBorderType);

/**
 * Three-channel 32-bit floating-point erosion with border control.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pMask Pointer to the start address of the mask array
 * \param oMaskSize Width and Height mask array.
 * \param oAnchor X and Y offsets of the mask origin frame of reference
 *        w.r.t the source pixel.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiErodeBorder_32f_C3R(const Npp32f * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp32f * pDst, Npp32s nDstStep, NppiSize oSizeROI, 
                        const Npp8u * pMask, NppiSize oMaskSize, NppiPoint oAnchor, NppiBorderType eBorderType);

/**
 * Four-channel 32-bit floating-point erosion with border control.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pMask Pointer to the start address of the mask array
 * \param oMaskSize Width and Height mask array.
 * \param oAnchor X and Y offsets of the mask origin frame of reference
 *        w.r.t the source pixel.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus
nppiErodeBorder_32f_C4R(const Npp32f * pSrc, int nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp32f * pDst, int nDstStep, NppiSize oSizeROI,
                        const Npp8u * pMask, NppiSize oMaskSize, NppiPoint oAnchor, NppiBorderType eBorderType);

/**
 * Four-channel 32-bit floating-point erosion with border control, ignoring alpha-channel.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Source image width and height in pixels relative to pSrc.
 * \param oSrcOffset Source image starting point relative to pSrc. 
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \param pMask Pointer to the start address of the mask array
 * \param oMaskSize Width and Height mask array.
 * \param oAnchor X and Y offsets of the mask origin frame of reference
 *        w.r.t the source pixel.
 * \param eBorderType The border type operation to be applied at source image border boundaries.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus
nppiErodeBorder_32f_AC4R(const Npp32f * pSrc, int nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, Npp32f * pDst, int nDstStep, NppiSize oSizeROI,
                         const Npp8u * pMask, NppiSize oMaskSize, NppiPoint oAnchor, NppiBorderType eBorderType);


/** @} image_erode_border */

/** @defgroup image_erode_3x3 Erode3x3
 *
 * Erosion using a 3x3 mask with the anchor at its center pixel.
 *
 * It is the user's responsibility to avoid \ref sampling_beyond_image_boundaries.
 *
 * @{
 *
 */


/**
 * Single-channel 8-bit unsigned integer 3x3 erosion.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiErode3x3_8u_C1R(const Npp8u * pSrc, Npp32s nSrcStep, Npp8u * pDst, Npp32s nDstStep, NppiSize oSizeROI);

/**
 * Three-channel 8-bit unsigned integer 3x3 erosion.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiErode3x3_8u_C3R(const Npp8u * pSrc, Npp32s nSrcStep, Npp8u * pDst, Npp32s nDstStep, NppiSize oSizeROI);

/**
 * Four-channel 8-bit unsigned integer 3x3 erosion.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus
nppiErode3x3_8u_C4R(const Npp8u * pSrc, int nSrcStep, Npp8u * pDst, int nDstStep, NppiSize oSizeROI);

/**
 * Four-channel 8-bit unsigned integer 3x3 erosion, ignoring alpha-channel.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus
nppiErode3x3_8u_AC4R(const Npp8u * pSrc, int nSrcStep, Npp8u * pDst, int nDstStep, NppiSize oSizeROI);


/**
 * Single-channel 16-bit unsigned integer 3x3 erosion.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiErode3x3_16u_C1R(const Npp16u * pSrc, Npp32s nSrcStep, Npp16u * pDst, Npp32s nDstStep, NppiSize oSizeROI);

/**
 * Three-channel 16-bit unsigned integer 3x3 erosion.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiErode3x3_16u_C3R(const Npp16u * pSrc, Npp32s nSrcStep, Npp16u * pDst, Npp32s nDstStep, NppiSize oSizeROI);

/**
 * Four-channel 16-bit unsigned integer 3x3 erosion.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus
nppiErode3x3_16u_C4R(const Npp16u * pSrc, int nSrcStep, Npp16u * pDst, int nDstStep, NppiSize oSizeROI);

/**
 * Four-channel 16-bit unsigned integer 3x3 erosion, ignoring alpha-channel.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus
nppiErode3x3_16u_AC4R(const Npp16u * pSrc, int nSrcStep, Npp16u * pDst, int nDstStep, NppiSize oSizeROI);


/**
 * Single-channel 32-bit floating-point 3x3 erosion.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiErode3x3_32f_C1R(const Npp32f * pSrc, Npp32s nSrcStep, Npp32f * pDst, Npp32s nDstStep, NppiSize oSizeROI);

/**
 * Three-channel 32-bit floating-point 3x3 erosion.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiErode3x3_32f_C3R(const Npp32f * pSrc, Npp32s nSrcStep, Npp32f * pDst, Npp32s nDstStep, NppiSize oSizeROI);

/**
 * Four-channel 32-bit floating-point 3x3 erosion.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus
nppiErode3x3_32f_C4R(const Npp32f * pSrc, int nSrcStep, Npp32f * pDst, int nDstStep, NppiSize oSizeROI);

/**
 * Four-channel 32-bit floating-point 3x3 erosion, ignoring alpha-channel.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus
nppiErode3x3_32f_AC4R(const Npp32f * pSrc, int nSrcStep, Npp32f * pDst, int nDstStep, NppiSize oSizeROI);

/**
 * Single-channel 64-bit floating-point 3x3 erosion.
 * 
 * \param pSrc  \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oSizeROI \ref roi_specification.
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus 
nppiErode3x3_64f_C1R(const Npp64f * pSrc, Npp32s nSrcStep, Npp64f * pDst, Npp32s nDstStep, NppiSize oSizeROI);

/** @} image_erode */

/** @defgroup image_erode_3x3_border Erode3x3Border
 *
 * Erosion using a 3x3 mask with the anchor at its center pixel with border control.
 *
 * If any portion of the mask overlaps the source image boundary the requested border type 
 * operation is applied to all mask pixels which fall outside of the source image.
 *
 * Currently only the NPP_BORDER_REPLICATE border type operation is supported.
 *
 * @{
 *
 */

/**
 * Single-channel 8-bit unsigned integer 3x3 erosion with border control.
 * 
 * \param pSrc  \ref source_image_pointer.
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
nppiErode3x3Border_8u_C1R(const Npp8u * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, 
                                Npp8u * pDst, Npp32s nDstStep, NppiSize oSizeROI, NppiBorderType eBorderType);

/**
 * Three-channel 8-bit unsigned integer 3x3 erosion with border control.
 * 
 * \param pSrc  \ref source_image_pointer.
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
nppiErode3x3Border_8u_C3R(const Npp8u * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, 
                                Npp8u * pDst, Npp32s nDstStep, NppiSize oSizeROI, NppiBorderType eBorderType);

/**
 * Four-channel 8-bit unsigned integer 3x3 erosion with border control.
 * 
 * \param pSrc  \ref source_image_pointer.
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
nppiErode3x3Border_8u_C4R(const Npp8u * pSrc, int nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, 
                                Npp8u * pDst, int nDstStep, NppiSize oSizeROI, NppiBorderType eBorderType);

/**
 * Four-channel 8-bit unsigned integer 3x3 erosion with border control, ignoring alpha-channel.
 * 
 * \param pSrc  \ref source_image_pointer.
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
nppiErode3x3Border_8u_AC4R(const Npp8u * pSrc, int nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, 
                                 Npp8u * pDst, int nDstStep, NppiSize oSizeROI, NppiBorderType eBorderType);


/**
 * Single-channel 16-bit unsigned integer 3x3 erosion with border control.
 * 
 * \param pSrc  \ref source_image_pointer.
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
nppiErode3x3Border_16u_C1R(const Npp16u * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, 
                                 Npp16u * pDst, Npp32s nDstStep, NppiSize oSizeROI, NppiBorderType eBorderType);

/**
 * Three-channel 16-bit unsigned integer 3x3 erosion with border control.
 * 
 * \param pSrc  \ref source_image_pointer.
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
nppiErode3x3Border_16u_C3R(const Npp16u * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, 
                                 Npp16u * pDst, Npp32s nDstStep, NppiSize oSizeROI, NppiBorderType eBorderType);

/**
 * Four-channel 16-bit unsigned integer 3x3 erosion with border control.
 * 
 * \param pSrc  \ref source_image_pointer.
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
nppiErode3x3Border_16u_C4R(const Npp16u * pSrc, int nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, 
                                 Npp16u * pDst, int nDstStep, NppiSize oSizeROI, NppiBorderType eBorderType);

/**
 * Four-channel 16-bit unsigned integer 3x3 erosion with border control, ignoring alpha-channel.
 * 
 * \param pSrc  \ref source_image_pointer.
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
nppiErode3x3Border_16u_AC4R(const Npp16u * pSrc, int nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, 
                                  Npp16u * pDst, int nDstStep, NppiSize oSizeROI, NppiBorderType eBorderType);


/**
 * Single-channel 32-bit floating-point 3x3 erosion with border control.
 * 
 * \param pSrc  \ref source_image_pointer.
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
nppiErode3x3Border_32f_C1R(const Npp32f * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, 
                                 Npp32f * pDst, Npp32s nDstStep, NppiSize oSizeROI, NppiBorderType eBorderType);

/**
 * Three-channel 32-bit floating-point 3x3 erosion with border control.
 * 
 * \param pSrc  \ref source_image_pointer.
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
nppiErode3x3Border_32f_C3R(const Npp32f * pSrc, Npp32s nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, 
                                 Npp32f * pDst, Npp32s nDstStep, NppiSize oSizeROI, NppiBorderType eBorderType);

/**
 * Four-channel 32-bit floating-point 3x3 erosion with border control.
 * 
 * \param pSrc  \ref source_image_pointer.
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
nppiErode3x3Border_32f_C4R(const Npp32f * pSrc, int nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, 
                                 Npp32f * pDst, int nDstStep, NppiSize oSizeROI, NppiBorderType eBorderType);

/**
 * Four-channel 32-bit floating-point 3x3 erosion with border control, ignoring alpha-channel.
 * 
 * \param pSrc  \ref source_image_pointer.
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
nppiErode3x3Border_32f_AC4R(const Npp32f * pSrc, int nSrcStep, NppiSize oSrcSize, NppiPoint oSrcOffset, 
                                  Npp32f * pDst, int nDstStep, NppiSize oSizeROI, NppiBorderType eBorderType);

/** @} image_erode_3x3_border */

/** @} image_morphological_operations */

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* NV_NPPI_MORPHOLOGICAL_OPERATIONS_H */
