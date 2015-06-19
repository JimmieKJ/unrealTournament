
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
#ifndef NV_NPPI_GEOMETRY_TRANSFORMS_H
#define NV_NPPI_GEOMETRY_TRANSFORMS_H
 
/**
 * \file nppi_geometry_transforms.h
 * Image Geometry Transform Primitives.
 */
 
#include "nppdefs.h"


#ifdef __cplusplus
extern "C" {
#endif

/** @defgroup image_geometry_transforms Geometry Transforms
 *  @ingroup nppi
 *
 * Routines manipulating an image's geometry.
 *
 * \section geometric_transform_api Geometric Transform API Specifics
 *
 * This section covers some of the unique API features common to the
 * geometric transform primitives.
 *
 * \subsection geometric_transform_roi Geometric Transforms and ROIs
 *
 * Geometric transforms operate on source and destination ROIs. The way
 * these ROIs affect the processing of pixels differs from other (non
 * geometric) image-processing primitives: Only pixels in the intersection
 * of the destination ROI and the transformed source ROI are being
 * processed.
 *
 * The typical processing proceedes as follows:
 * -# Transform the rectangular source ROI (given in source image coordinates)
 *		into the destination image space. This yields a quadrilateral.
 * -# Write only pixels in the intersection of the transformed source ROI and
 *		the destination ROI.
 *
 * \subsection geometric_transforms_interpolation Pixel Interpolation
 *
 * The majority of image geometry transform operation need to perform a 
 * resampling of the source image as source and destination pixels are not
 * coincident.
 *
 * NPP supports the following pixel inerpolation modes (in order from fastest to 
 * slowest and lowest to highest quality):
 * - nearest neighbor
 * - linear interpolation
 * - cubic convolution
 * - supersampling
 * - interpolation using Lanczos window function
 *
 * @{
 *
 */

/** @defgroup image_resize_square_pixel ResizeSqrPixel
 *
 * ResizeSqrPixel supports the following interpolation modes:
 *
 * \code
 *   NPPI_INTER_NN
 *   NPPI_INTER_LINEAR
 *   NPPI_INTER_CUBIC
 *   NPPI_INTER_CUBIC2P_BSPLINE
 *   NPPI_INTER_CUBIC2P_CATMULLROM
 *   NPPI_INTER_CUBIC2P_B05C03
 *   NPPI_INTER_SUPER
 *   NPPI_INTER_LANCZOS
 * \endcode
 *
 * ResizeSqrPixel attempts to choose source pixels that would approximately represent the center of the destination pixels.
 * It does so by using the following scaling formula to select source pixels for interpolation:
 *
 * \code
 *   nAdjustedXFactor = 1.0 / nXFactor;
 *   nAdjustedYFactor = 1.0 / nYFactor;
 *   nAdjustedXShift = nXShift * nAdjustedXFactor + ((1.0 - nAdjustedXFactor) * 0.5);
 *   nAdjustedYShift = nYShift * nAdjustedYFactor + ((1.0 - nAdjustedYFactor) * 0.5);
 *   nSrcX = nAdjustedXFactor * nDstX - nAdjustedXShift;
 *   nSrcY = nAdjustedYFactor * nDstY - nAdjustedYShift;
 * \endcode
 *
 * In the ResizeSqrPixel functions below source image clip checking is handled as follows:
 *
 * If the source pixel fractional x and y coordinates are greater than or equal to oSizeROI.x and less than oSizeROI.x + oSizeROI.width and
 * greater than or equal to oSizeROI.y and less than oSizeROI.y + oSizeROI.height then the source pixel is considered to be within
 * the source image clip rectangle and the source image is sampled.  Otherwise the source image is not sampled and a destination pixel is not
 * written to the destination image. 
 *
 * \section resize_error_codes Error Codes
 * The resize primitives return the following error codes:
 *
 *         - ::NPP_WRONG_INTERSECTION_ROI_ERROR indicates an error condition if
 *           srcROIRect has no intersection with the source image.
 *         - ::NPP_RESIZE_NO_OPERATION_ERROR if either destination ROI width or
 *           height is less than 1 pixel.
 *         - ::NPP_RESIZE_FACTOR_ERROR Indicates an error condition if either nXFactor or
 *           nYFactor is less than or equal to zero.
 *         - ::NPP_INTERPOLATION_ERROR if eInterpolation has an illegal value.
 *         - ::NPP_SIZE_ERROR if source size width or height is less than 2 pixels.
 *
 * @{
 *
 */

/** @name GetResizeRect
 * Returns NppiRect which represents the offset and size of the destination rectangle that would be generated by
 * resizing the source NppiRect by the requested scale factors and shifts.
 *                                    
 * @{
 *
 */

/**
 * \param oSrcROI Region of interest in the source image.
 * \param pDstRect User supplied host memory pointer to an NppiRect structure that will be filled in by this function with the region of interest in the destination image.
 * \param nXFactor Factor by which x dimension is changed. 
 * \param nYFactor Factor by which y dimension is changed. 
 * \param nXShift Source pixel shift in x-direction.
 * \param nYShift Source pixel shift in y-direction.
 * \param eInterpolation The type of eInterpolation to perform resampling.
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref resize_error_codes
 */
NppStatus
nppiGetResizeRect(NppiRect oSrcROI, NppiRect *pDstRect, 
                  double nXFactor, double nYFactor, double nXShift, double nYShift, int eInterpolation);

/** @} */

/** @name ResizeSqrPixel
 * Resizes images.
 *                                    
 * @{
 *
 */

/**
 * 1 channel 8-bit unsigned image resize.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Size in pixels of the source image.
 * \param oSrcROI Region of interest in the source image.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Region of interest in the destination image.
 * \param nXFactor Factor by which x dimension is changed. 
 * \param nYFactor Factor by which y dimension is changed. 
 * \param nXShift Source pixel shift in x-direction.
 * \param nYShift Source pixel shift in y-direction.
 * \param eInterpolation The type of eInterpolation to perform resampling.
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref resize_error_codes
 */
NppStatus 
nppiResizeSqrPixel_8u_C1R(const Npp8u * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                                Npp8u * pDst, int nDstStep, NppiRect oDstROI,
                          double nXFactor, double nYFactor, double nXShift, double nYShift, int eInterpolation);

/**
 * 3 channel 8-bit unsigned image resize.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Size in pixels of the source image.
 * \param oSrcROI Region of interest in the source image.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Region of interest in the destination image.
 * \param nXFactor Factor by which x dimension is changed. 
 * \param nYFactor Factor by which y dimension is changed. 
 * \param nXShift Source pixel shift in x-direction.
 * \param nYShift Source pixel shift in y-direction.
 * \param eInterpolation The type of eInterpolation to perform resampling.
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref resize_error_codes
 */
NppStatus 
nppiResizeSqrPixel_8u_C3R(const Npp8u * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                                Npp8u * pDst, int nDstStep, NppiRect oDstROI,
                          double nXFactor, double nYFactor, double nXShift, double nYShift, int eInterpolation);

/**
 * 4 channel 8-bit unsigned image resize.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Size in pixels of the source image.
 * \param oSrcROI Region of interest in the source image.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Region of interest in the destination image.
 * \param nXFactor Factor by which x dimension is changed. 
 * \param nYFactor Factor by which y dimension is changed. 
 * \param nXShift Source pixel shift in x-direction.
 * \param nYShift Source pixel shift in y-direction.
 * \param eInterpolation The type of eInterpolation to perform resampling.
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref resize_error_codes
 */
NppStatus 
nppiResizeSqrPixel_8u_C4R(const Npp8u * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                                Npp8u * pDst, int nDstStep, NppiRect oDstROI,
                          double nXFactor, double nYFactor, double nXShift, double nYShift, int eInterpolation);

/**
 * 4 channel 8-bit unsigned image resize not affecting alpha.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Size in pixels of the source image.
 * \param oSrcROI Region of interest in the source image.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Region of interest in the destination image.
 * \param nXFactor Factor by which x dimension is changed. 
 * \param nYFactor Factor by which y dimension is changed. 
 * \param nXShift Source pixel shift in x-direction.
 * \param nYShift Source pixel shift in y-direction.
 * \param eInterpolation The type of interpolation to perform resampling.
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref resize_error_codes
 */
NppStatus 
nppiResizeSqrPixel_8u_AC4R(const Npp8u * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                                 Npp8u * pDst, int nDstStep, NppiRect oDstROI,
                           double nXFactor, double nYFactor, double nXShift, double nYShift, int eInterpolation);

/**
 * 3 channel 8-bit unsigned planar image resize.
 *
 * \param pSrc \ref source_planar_image_pointer_array (host memory array containing device memory image plane pointers).
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Size in pixels of the source image.
 * \param oSrcROI Region of interest in the source image.
 * \param pDst \ref destination_planar_image_pointer_array (host memory array containing device memory image plane pointers).
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Region of interest in the destination image.
 * \param nXFactor Factor by which x dimension is changed. 
 * \param nYFactor Factor by which y dimension is changed. 
 * \param nXShift Source pixel shift in x-direction.
 * \param nYShift Source pixel shift in y-direction.
 * \param eInterpolation The type of eInterpolation to perform resampling.
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref resize_error_codes
 */
NppStatus 
nppiResizeSqrPixel_8u_P3R(const Npp8u * const pSrc[3], NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                                Npp8u * pDst[3], int nDstStep, NppiRect oDstROI,
                          double nXFactor, double nYFactor, double nXShift, double nYShift, int eInterpolation);

/**
 * 4 channel 8-bit unsigned planar image resize.
 *
 * \param pSrc \ref source_planar_image_pointer_array (host memory array containing device memory image plane pointers).
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Size in pixels of the source image.
 * \param oSrcROI Region of interest in the source image.
 * \param pDst \ref destination_planar_image_pointer_array (host memory array containing device memory image plane pointers).
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Region of interest in the destination image.
 * \param nXFactor Factor by which x dimension is changed. 
 * \param nYFactor Factor by which y dimension is changed. 
 * \param nXShift Source pixel shift in x-direction.
 * \param nYShift Source pixel shift in y-direction.
 * \param eInterpolation The type of eInterpolation to perform resampling.
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref resize_error_codes
 */
NppStatus 
nppiResizeSqrPixel_8u_P4R(const Npp8u * const pSrc[4], NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                                Npp8u * pDst[4], int nDstStep, NppiRect oDstROI,
                          double nXFactor, double nYFactor, double nXShift, double nYShift, int eInterpolation);

/**
 * 1 channel 16-bit unsigned image resize.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Size in pixels of the source image.
 * \param oSrcROI Region of interest in the source image.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Region of interest in the destination image.
 * \param nXFactor Factor by which x dimension is changed. 
 * \param nYFactor Factor by which y dimension is changed. 
 * \param nXShift Source pixel shift in x-direction.
 * \param nYShift Source pixel shift in y-direction.
 * \param eInterpolation The type of eInterpolation to perform resampling.
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref resize_error_codes
 */
NppStatus 
nppiResizeSqrPixel_16u_C1R(const Npp16u * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                                 Npp16u * pDst, int nDstStep, NppiRect oDstROI,
                          double nXFactor, double nYFactor, double nXShift, double nYShift, int eInterpolation);

/**
 * 3 channel 16-bit unsigned image resize.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Size in pixels of the source image.
 * \param oSrcROI Region of interest in the source image.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Region of interest in the destination image.
 * \param nXFactor Factor by which x dimension is changed. 
 * \param nYFactor Factor by which y dimension is changed. 
 * \param nXShift Source pixel shift in x-direction.
 * \param nYShift Source pixel shift in y-direction.
 * \param eInterpolation The type of eInterpolation to perform resampling.
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref resize_error_codes
 */
NppStatus 
nppiResizeSqrPixel_16u_C3R(const Npp16u * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                                 Npp16u * pDst, int nDstStep, NppiRect oDstROI,
                           double nXFactor, double nYFactor, double nXShift, double nYShift, int eInterpolation);

/**
 * 4 channel 16-bit unsigned image resize.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Size in pixels of the source image.
 * \param oSrcROI Region of interest in the source image.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Region of interest in the destination image.
 * \param nXFactor Factor by which x dimension is changed. 
 * \param nYFactor Factor by which y dimension is changed. 
 * \param nXShift Source pixel shift in x-direction.
 * \param nYShift Source pixel shift in y-direction.
 * \param eInterpolation The type of eInterpolation to perform resampling.
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref resize_error_codes
 */
NppStatus 
nppiResizeSqrPixel_16u_C4R(const Npp16u * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                                 Npp16u * pDst, int nDstStep, NppiRect oDstROI,
                           double nXFactor, double nYFactor, double nXShift, double nYShift, int eInterpolation);

/**
 * 4 channel 16-bit unsigned image resize not affecting alpha.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Size in pixels of the source image.
 * \param oSrcROI Region of interest in the source image.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Region of interest in the destination image.
 * \param nXFactor Factor by which x dimension is changed. 
 * \param nYFactor Factor by which y dimension is changed. 
 * \param nXShift Source pixel shift in x-direction.
 * \param nYShift Source pixel shift in y-direction.
 * \param eInterpolation The type of interpolation to perform resampling.
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref resize_error_codes
 */
NppStatus 
nppiResizeSqrPixel_16u_AC4R(const Npp16u * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                                  Npp16u * pDst, int nDstStep, NppiRect oDstROI,
                            double nXFactor, double nYFactor, double nXShift, double nYShift, int eInterpolation);

/**
 * 3 channel 16-bit unsigned planar image resize.
 *
 * \param pSrc \ref source_planar_image_pointer_array (host memory array containing device memory image plane pointers).
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Size in pixels of the source image.
 * \param oSrcROI Region of interest in the source image.
 * \param pDst \ref destination_planar_image_pointer_array (host memory array containing device memory image plane pointers).
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Region of interest in the destination image.
 * \param nXFactor Factor by which x dimension is changed. 
 * \param nYFactor Factor by which y dimension is changed. 
 * \param nXShift Source pixel shift in x-direction.
 * \param nYShift Source pixel shift in y-direction.
 * \param eInterpolation The type of eInterpolation to perform resampling.
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref resize_error_codes
 */
NppStatus 
nppiResizeSqrPixel_16u_P3R(const Npp16u * const pSrc[3], NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                                 Npp16u * pDst[3], int nDstStep, NppiRect oDstROI,
                           double nXFactor, double nYFactor, double nXShift, double nYShift, int eInterpolation);

/**
 * 4 channel 16-bit unsigned planar image resize.
 *
 * \param pSrc \ref source_planar_image_pointer_array (host memory array containing device memory image plane pointers).
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Size in pixels of the source image.
 * \param oSrcROI Region of interest in the source image.
 * \param pDst \ref destination_planar_image_pointer_array (host memory array containing device memory image plane pointers).
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Region of interest in the destination image.
 * \param nXFactor Factor by which x dimension is changed. 
 * \param nYFactor Factor by which y dimension is changed. 
 * \param nXShift Source pixel shift in x-direction.
 * \param nYShift Source pixel shift in y-direction.
 * \param eInterpolation The type of eInterpolation to perform resampling.
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref resize_error_codes
 */
NppStatus 
nppiResizeSqrPixel_16u_P4R(const Npp16u * const pSrc[4], NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                                 Npp16u * pDst[4], int nDstStep, NppiRect oDstROI,
                           double nXFactor, double nYFactor, double nXShift, double nYShift, int eInterpolation);

/**
 * 1 channel 16-bit signed image resize.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Size in pixels of the source image.
 * \param oSrcROI Region of interest in the source image.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Region of interest in the destination image.
 * \param nXFactor Factor by which x dimension is changed. 
 * \param nYFactor Factor by which y dimension is changed. 
 * \param nXShift Source pixel shift in x-direction.
 * \param nYShift Source pixel shift in y-direction.
 * \param eInterpolation The type of eInterpolation to perform resampling.
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref resize_error_codes
 */
NppStatus 
nppiResizeSqrPixel_16s_C1R(const Npp16s * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                                 Npp16s * pDst, int nDstStep, NppiRect oDstROI,
                          double nXFactor, double nYFactor, double nXShift, double nYShift, int eInterpolation);

/**
 * 3 channel 16-bit signed image resize.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Size in pixels of the source image.
 * \param oSrcROI Region of interest in the source image.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Region of interest in the destination image.
 * \param nXFactor Factor by which x dimension is changed. 
 * \param nYFactor Factor by which y dimension is changed. 
 * \param nXShift Source pixel shift in x-direction.
 * \param nYShift Source pixel shift in y-direction.
 * \param eInterpolation The type of eInterpolation to perform resampling.
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref resize_error_codes
 */
NppStatus 
nppiResizeSqrPixel_16s_C3R(const Npp16s * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                                 Npp16s * pDst, int nDstStep, NppiRect oDstROI,
                           double nXFactor, double nYFactor, double nXShift, double nYShift, int eInterpolation);

/**
 * 4 channel 16-bit signed image resize.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Size in pixels of the source image.
 * \param oSrcROI Region of interest in the source image.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Region of interest in the destination image.
 * \param nXFactor Factor by which x dimension is changed. 
 * \param nYFactor Factor by which y dimension is changed. 
 * \param nXShift Source pixel shift in x-direction.
 * \param nYShift Source pixel shift in y-direction.
 * \param eInterpolation The type of eInterpolation to perform resampling.
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref resize_error_codes
 */
NppStatus 
nppiResizeSqrPixel_16s_C4R(const Npp16s * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                                 Npp16s * pDst, int nDstStep, NppiRect oDstROI,
                           double nXFactor, double nYFactor, double nXShift, double nYShift, int eInterpolation);

/**
 * 4 channel 16-bit signed image resize not affecting alpha.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Size in pixels of the source image.
 * \param oSrcROI Region of interest in the source image.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Region of interest in the destination image.
 * \param nXFactor Factor by which x dimension is changed. 
 * \param nYFactor Factor by which y dimension is changed. 
 * \param nXShift Source pixel shift in x-direction.
 * \param nYShift Source pixel shift in y-direction.
 * \param eInterpolation The type of interpolation to perform resampling.
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref resize_error_codes
 */
NppStatus 
nppiResizeSqrPixel_16s_AC4R(const Npp16s * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                                  Npp16s * pDst, int nDstStep, NppiRect oDstROI,
                            double nXFactor, double nYFactor, double nXShift, double nYShift, int eInterpolation);

/**
 * 3 channel 16-bit signed planar image resize.
 *
 * \param pSrc \ref source_planar_image_pointer_array (host memory array containing device memory image plane pointers).
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Size in pixels of the source image.
 * \param oSrcROI Region of interest in the source image.
 * \param pDst \ref destination_planar_image_pointer_array (host memory array containing device memory image plane pointers).
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Region of interest in the destination image.
 * \param nXFactor Factor by which x dimension is changed. 
 * \param nYFactor Factor by which y dimension is changed. 
 * \param nXShift Source pixel shift in x-direction.
 * \param nYShift Source pixel shift in y-direction.
 * \param eInterpolation The type of eInterpolation to perform resampling.
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref resize_error_codes
 */
NppStatus 
nppiResizeSqrPixel_16s_P3R(const Npp16s * const pSrc[3], NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                                 Npp16s * pDst[3], int nDstStep, NppiRect oDstROI,
                           double nXFactor, double nYFactor, double nXShift, double nYShift, int eInterpolation);

/**
 * 4 channel 16-bit signed planar image resize.
 *
 * \param pSrc \ref source_planar_image_pointer_array (host memory array containing device memory image plane pointers).
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Size in pixels of the source image.
 * \param oSrcROI Region of interest in the source image.
 * \param pDst \ref destination_planar_image_pointer_array (host memory array containing device memory image plane pointers).
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Region of interest in the destination image.
 * \param nXFactor Factor by which x dimension is changed. 
 * \param nYFactor Factor by which y dimension is changed. 
 * \param nXShift Source pixel shift in x-direction.
 * \param nYShift Source pixel shift in y-direction.
 * \param eInterpolation The type of eInterpolation to perform resampling.
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref resize_error_codes
 */
NppStatus 
nppiResizeSqrPixel_16s_P4R(const Npp16s * const pSrc[4], NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                                 Npp16s * pDst[4], int nDstStep, NppiRect oDstROI,
                           double nXFactor, double nYFactor, double nXShift, double nYShift, int eInterpolation);

/**
 * 1 channel 32-bit floating point image resize.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Size in pixels of the source image.
 * \param oSrcROI Region of interest in the source image.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Region of interest in the destination image.
 * \param nXFactor Factor by which x dimension is changed. 
 * \param nYFactor Factor by which y dimension is changed. 
 * \param nXShift Source pixel shift in x-direction.
 * \param nYShift Source pixel shift in y-direction.
 * \param eInterpolation The type of eInterpolation to perform resampling.
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref resize_error_codes
 */
NppStatus 
nppiResizeSqrPixel_32f_C1R(const Npp32f * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                                 Npp32f * pDst, int nDstStep, NppiRect oDstROI,
                          double nXFactor, double nYFactor, double nXShift, double nYShift, int eInterpolation);

/**
 * 3 channel 32-bit floating point image resize.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Size in pixels of the source image.
 * \param oSrcROI Region of interest in the source image.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Region of interest in the destination image.
 * \param nXFactor Factor by which x dimension is changed. 
 * \param nYFactor Factor by which y dimension is changed. 
 * \param nXShift Source pixel shift in x-direction.
 * \param nYShift Source pixel shift in y-direction.
 * \param eInterpolation The type of eInterpolation to perform resampling.
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref resize_error_codes
 */
NppStatus 
nppiResizeSqrPixel_32f_C3R(const Npp32f * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                                 Npp32f * pDst, int nDstStep, NppiRect oDstROI,
                           double nXFactor, double nYFactor, double nXShift, double nYShift, int eInterpolation);

/**
 * 4 channel 32-bit floating point image resize.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Size in pixels of the source image.
 * \param oSrcROI Region of interest in the source image.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Region of interest in the destination image.
 * \param nXFactor Factor by which x dimension is changed. 
 * \param nYFactor Factor by which y dimension is changed. 
 * \param nXShift Source pixel shift in x-direction.
 * \param nYShift Source pixel shift in y-direction.
 * \param eInterpolation The type of eInterpolation to perform resampling.
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref resize_error_codes
 */
NppStatus 
nppiResizeSqrPixel_32f_C4R(const Npp32f * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                                 Npp32f * pDst, int nDstStep, NppiRect oDstROI,
                           double nXFactor, double nYFactor, double nXShift, double nYShift, int eInterpolation);

/**
 * 4 channel 32-bit floating point image resize not affecting alpha.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Size in pixels of the source image.
 * \param oSrcROI Region of interest in the source image.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Region of interest in the destination image.
 * \param nXFactor Factor by which x dimension is changed. 
 * \param nYFactor Factor by which y dimension is changed. 
 * \param nXShift Source pixel shift in x-direction.
 * \param nYShift Source pixel shift in y-direction.
 * \param eInterpolation The type of interpolation to perform resampling.
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref resize_error_codes
 */
NppStatus 
nppiResizeSqrPixel_32f_AC4R(const Npp32f * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                                  Npp32f * pDst, int nDstStep, NppiRect oDstROI,
                            double nXFactor, double nYFactor, double nXShift, double nYShift, int eInterpolation);

/**
 * 3 channel 32-bit floating point planar image resize.
 *
 * \param pSrc \ref source_planar_image_pointer_array (host memory array containing device memory image plane pointers).
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Size in pixels of the source image.
 * \param oSrcROI Region of interest in the source image.
 * \param pDst \ref destination_planar_image_pointer_array (host memory array containing device memory image plane pointers).
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Region of interest in the destination image.
 * \param nXFactor Factor by which x dimension is changed. 
 * \param nYFactor Factor by which y dimension is changed. 
 * \param nXShift Source pixel shift in x-direction.
 * \param nYShift Source pixel shift in y-direction.
 * \param eInterpolation The type of eInterpolation to perform resampling.
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref resize_error_codes
 */
NppStatus 
nppiResizeSqrPixel_32f_P3R(const Npp32f * const pSrc[3], NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                                 Npp32f * pDst[3], int nDstStep, NppiRect oDstROI,
                           double nXFactor, double nYFactor, double nXShift, double nYShift, int eInterpolation);

/**
 * 4 channel 32-bit floating point planar image resize.
 *
 * \param pSrc \ref source_planar_image_pointer_array (host memory array containing device memory image plane pointers).
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Size in pixels of the source image.
 * \param oSrcROI Region of interest in the source image.
 * \param pDst \ref destination_planar_image_pointer_array (host memory array containing device memory image plane pointers).
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Region of interest in the destination image.
 * \param nXFactor Factor by which x dimension is changed. 
 * \param nYFactor Factor by which y dimension is changed. 
 * \param nXShift Source pixel shift in x-direction.
 * \param nYShift Source pixel shift in y-direction.
 * \param eInterpolation The type of eInterpolation to perform resampling.
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref resize_error_codes
 */
NppStatus 
nppiResizeSqrPixel_32f_P4R(const Npp32f * const pSrc[4], NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                                 Npp32f * pDst[4], int nDstStep, NppiRect oDstROI,
                           double nXFactor, double nYFactor, double nXShift, double nYShift, int eInterpolation);

/**
 * 1 channel 64-bit floating point image resize.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Size in pixels of the source image.
 * \param oSrcROI Region of interest in the source image.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Region of interest in the destination image.
 * \param nXFactor Factor by which x dimension is changed. 
 * \param nYFactor Factor by which y dimension is changed. 
 * \param nXShift Source pixel shift in x-direction.
 * \param nYShift Source pixel shift in y-direction.
 * \param eInterpolation The type of eInterpolation to perform resampling.
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref resize_error_codes
 */
NppStatus 
nppiResizeSqrPixel_64f_C1R(const Npp64f * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                                 Npp64f * pDst, int nDstStep, NppiRect oDstROI,
                          double nXFactor, double nYFactor, double nXShift, double nYShift, int eInterpolation);

/**
 * 3 channel 64-bit floating point image resize.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Size in pixels of the source image.
 * \param oSrcROI Region of interest in the source image.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Region of interest in the destination image.
 * \param nXFactor Factor by which x dimension is changed. 
 * \param nYFactor Factor by which y dimension is changed. 
 * \param nXShift Source pixel shift in x-direction.
 * \param nYShift Source pixel shift in y-direction.
 * \param eInterpolation The type of eInterpolation to perform resampling.
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref resize_error_codes
 */
NppStatus 
nppiResizeSqrPixel_64f_C3R(const Npp64f * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                                 Npp64f * pDst, int nDstStep, NppiRect oDstROI,
                           double nXFactor, double nYFactor, double nXShift, double nYShift, int eInterpolation);

/**
 * 4 channel 64-bit floating point image resize.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Size in pixels of the source image.
 * \param oSrcROI Region of interest in the source image.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Region of interest in the destination image.
 * \param nXFactor Factor by which x dimension is changed. 
 * \param nYFactor Factor by which y dimension is changed. 
 * \param nXShift Source pixel shift in x-direction.
 * \param nYShift Source pixel shift in y-direction.
 * \param eInterpolation The type of eInterpolation to perform resampling.
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref resize_error_codes
 */
NppStatus 
nppiResizeSqrPixel_64f_C4R(const Npp64f * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                                 Npp64f * pDst, int nDstStep, NppiRect oDstROI,
                           double nXFactor, double nYFactor, double nXShift, double nYShift, int eInterpolation);

/**
 * 4 channel 64-bit floating point image resize not affecting alpha.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Size in pixels of the source image.
 * \param oSrcROI Region of interest in the source image.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Region of interest in the destination image.
 * \param nXFactor Factor by which x dimension is changed. 
 * \param nYFactor Factor by which y dimension is changed. 
 * \param nXShift Source pixel shift in x-direction.
 * \param nYShift Source pixel shift in y-direction.
 * \param eInterpolation The type of interpolation to perform resampling.
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref resize_error_codes
 */
NppStatus 
nppiResizeSqrPixel_64f_AC4R(const Npp64f * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                                  Npp64f * pDst, int nDstStep, NppiRect oDstROI,
                            double nXFactor, double nYFactor, double nXShift, double nYShift, int eInterpolation);

/**
 * 3 channel 64-bit floating point planar image resize.
 *
 * \param pSrc \ref source_planar_image_pointer_array (host memory array containing device memory image plane pointers).
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Size in pixels of the source image.
 * \param oSrcROI Region of interest in the source image.
 * \param pDst \ref destination_planar_image_pointer_array (host memory array containing device memory image plane pointers).
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Region of interest in the destination image.
 * \param nXFactor Factor by which x dimension is changed. 
 * \param nYFactor Factor by which y dimension is changed. 
 * \param nXShift Source pixel shift in x-direction.
 * \param nYShift Source pixel shift in y-direction.
 * \param eInterpolation The type of eInterpolation to perform resampling.
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref resize_error_codes
 */
NppStatus 
nppiResizeSqrPixel_64f_P3R(const Npp64f * const pSrc[3], NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                                 Npp64f * pDst[3], int nDstStep, NppiRect oDstROI,
                           double nXFactor, double nYFactor, double nXShift, double nYShift, int eInterpolation);

/**
 * 4 channel 64-bit floating point planar image resize.
 *
 * \param pSrc \ref source_planar_image_pointer_array (host memory array containing device memory image plane pointers).
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Size in pixels of the source image.
 * \param oSrcROI Region of interest in the source image.
 * \param pDst \ref destination_planar_image_pointer_array (host memory array containing device memory image plane pointers).
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Region of interest in the destination image.
 * \param nXFactor Factor by which x dimension is changed. 
 * \param nYFactor Factor by which y dimension is changed. 
 * \param nXShift Source pixel shift in x-direction.
 * \param nYShift Source pixel shift in y-direction.
 * \param eInterpolation The type of eInterpolation to perform resampling.
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref resize_error_codes
 */
NppStatus 
nppiResizeSqrPixel_64f_P4R(const Npp64f * const pSrc[4], NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                                 Npp64f * pDst[4], int nDstStep, NppiRect oDstROI,
                           double nXFactor, double nYFactor, double nXShift, double nYShift, int eInterpolation);

/** @} */

/** @} image_resize_square_pixel */

/** @defgroup image_resize Resize
 *
 * This function has been deprecated.  ResizeSqrPixel provides the same functionality and more.
 *
 * Resize supports the following interpolation modes:
 *
 * \code
 *   NPPI_INTER_NN
 *   NPPI_INTER_LINEAR
 *   NPPI_INTER_CUBIC
 *   NPPI_INTER_SUPER
 *   NPPI_INTER_LANCZOS
 * \endcode
 *
 * Resize uses the following scaling formula to select source pixels for interpolation:
 *
 * \code
 *   scaledSrcSize.width = nXFactor * srcRectROI.width;
 *   scaledSrcSize.height = nYFactor * srcRectROI.height;
 *   nAdjustedXFactor = (srcRectROI.width - 1) / (scaledSrcSize.width - 1);
 *   nAdjustedYFactor = (srcRectROI.height - 1) / (scaledSrcSize.height - 1);
 *   nSrcX = nAdjustedXFactor * nDstX;
 *   nSrcY = nAdjustedYFactor * nDstY;
 * \endcode
 *
 * In the Resize functions below source image clip checking is handled as follows:
 *
 * If the source pixel fractional x and y coordinates are greater than or equal to oSizeROI.x and less than oSizeROI.x + oSizeROI.width and
 * greater than or equal to oSizeROI.y and less than oSizeROI.y + oSizeROI.height then the source pixel is considered to be within
 * the source image clip rectangle and the source image is sampled.  Otherwise the source image is not sampled and a destination pixel is not
 * written to the destination image. 
 *
 * \section resize_error_codes Error Codes
 * The resize primitives return the following error codes:
 *
 *         - ::NPP_WRONG_INTERSECTION_ROI_ERROR indicates an error condition if
 *           srcROIRect has no intersection with the source image.
 *         - ::NPP_RESIZE_NO_OPERATION_ERROR if either destination ROI width or
 *           height is less than 1 pixel.
 *         - ::NPP_RESIZE_FACTOR_ERROR Indicates an error condition if either nXFactor or
 *           nYFactor is less than or equal to zero.
 *         - ::NPP_INTERPOLATION_ERROR if eInterpolation has an illegal value.
 *         - ::NPP_SIZE_ERROR if source size width or height is less than 2 pixels.
 *
 * @{
 *
 */

/** @name Resize
 * Resizes images.
 *                                    
 * @{
 *
 */

/**
 * 1 channel 8-bit unsigned image resize.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Size in pixels of the source image
 * \param oSrcROI Region of interest in the source image.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param dstROISize Size in pixels of the destination image.
 * \param nXFactor Factor by which x dimension is changed. 
 * \param nYFactor Factor by which y dimension is changed. 
 * \param eInterpolation The type of eInterpolation to perform resampling.
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref resize_error_codes
 */
NppStatus 
nppiResize_8u_C1R(const Npp8u * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                        Npp8u * pDst, int nDstStep, NppiSize dstROISize,
                  double nXFactor, double nYFactor, int eInterpolation);

/**
 * 3 channel 8-bit unsigned image resize.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Size in pixels of the source image
 * \param oSrcROI Region of interest in the source image.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param dstROISize Size in pixels of the destination image.
 * \param nXFactor Factor by which x dimension is changed. 
 * \param nYFactor Factor by which y dimension is changed. 
 * \param eInterpolation The type of eInterpolation to perform resampling.
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref resize_error_codes
 */
NppStatus 
nppiResize_8u_C3R(const Npp8u * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                        Npp8u * pDst, int nDstStep, NppiSize dstROISize,
                  double nXFactor, double nYFactor, int eInterpolation);

/**
 * 4 channel 8-bit unsigned image resize.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Size in pixels of the source image
 * \param oSrcROI Region of interest in the source image.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param dstROISize Size in pixels of the destination image.
 * \param nXFactor Factor by which x dimension is changed. 
 * \param nYFactor Factor by which y dimension is changed. 
 * \param eInterpolation The type of eInterpolation to perform resampling.
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref resize_error_codes
 */
NppStatus 
nppiResize_8u_C4R(const Npp8u * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                        Npp8u * pDst, int nDstStep, NppiSize dstROISize,
                  double nXFactor, double nYFactor, int eInterpolation);

/**
 * 4 channel 8-bit unsigned image resize not affecting alpha.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Size in pixels of the source image
 * \param oSrcROI Region of interest in the source image.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param dstROISize Size in pixels of the destination image.
 * \param nXFactor Factor by which x dimension is changed. 
 * \param nYFactor Factor by which y dimension is changed. 
 * \param eInterpolation The type of interpolation to perform resampling.
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref resize_error_codes
 */
NppStatus 
nppiResize_8u_AC4R(const Npp8u * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                         Npp8u * pDst, int nDstStep, NppiSize dstROISize,
                   double nXFactor, double nYFactor, int eInterpolation);

/**
 * 3 channel 8-bit unsigned planar image resize.
 *
 * \param pSrc \ref source_planar_image_pointer_array (host memory array containing device memory image plane pointers).
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Size in pixels of the source image.
 * \param oSrcROI Region of interest in the source image.
 * \param pDst \ref destination_planar_image_pointer_array (host memory array containing device memory image plane pointers).
 * \param nDstStep \ref destination_image_line_step.
 * \param dstROISize Size in pixels of the destination image
 * \param nXFactor Factor by which x dimension is changed. 
 * \param nYFactor Factor by which y dimension is changed. 
 * \param eInterpolation The type of eInterpolation to perform resampling.
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref resize_error_codes
 */
NppStatus 
nppiResize_8u_P3R(const Npp8u * const pSrc[3], NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                        Npp8u * pDst[3], int nDstStep, NppiSize dstROISize,
                  double nXFactor, double nYFactor, int eInterpolation);

/**
 * 4 channel 8-bit unsigned planar image resize.
 *
 * \param pSrc \ref source_planar_image_pointer_array (host memory array containing device memory image plane pointers).
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Size in pixels of the source image.
 * \param oSrcROI Region of interest in the source image.
 * \param pDst \ref destination_planar_image_pointer_array (host memory array containing device memory image plane pointers).
 * \param nDstStep \ref destination_image_line_step.
 * \param dstROISize Size in pixels of the destination image.
 * \param nXFactor Factor by which x dimension is changed. 
 * \param nYFactor Factor by which y dimension is changed. 
 * \param eInterpolation The type of eInterpolation to perform resampling.
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref resize_error_codes
 */
NppStatus 
nppiResize_8u_P4R(const Npp8u * const pSrc[4], NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                        Npp8u * pDst[4], int nDstStep, NppiSize dstROISize,
                  double nXFactor, double nYFactor, int eInterpolation);

/**
 * 1 channel 16-bit unsigned image resize.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Size in pixels of the source image
 * \param oSrcROI Region of interest in the source image.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param dstROISize Size in pixels of the destination image.
 * \param nXFactor Factor by which x dimension is changed. 
 * \param nYFactor Factor by which y dimension is changed. 
 * \param eInterpolation The type of eInterpolation to perform resampling.
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref resize_error_codes
 */
NppStatus 
nppiResize_16u_C1R(const Npp16u * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                         Npp16u * pDst, int nDstStep, NppiSize dstROISize,
                  double nXFactor, double nYFactor, int eInterpolation);

/**
 * 3 channel 16-bit unsigned image resize.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Size in pixels of the source image
 * \param oSrcROI Region of interest in the source image.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param dstROISize Size in pixels of the destination image.
 * \param nXFactor Factor by which x dimension is changed.
 * \param nYFactor Factor by which y dimension is changed. 
 * \param eInterpolation The type of eInterpolation to perform resampling.
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref resize_error_codes
 */
NppStatus 
nppiResize_16u_C3R(const Npp16u * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                         Npp16u * pDst, int nDstStep, NppiSize dstROISize,
                   double nXFactor, double nYFactor, int eInterpolation);

/**
 * 4 channel 16-bit unsigned image resize.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Size in pixels of the source image
 * \param oSrcROI Region of interest in the source image.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param dstROISize Size in pixels of the destination image.
 * \param nXFactor Factor by which x dimension is changed. 
 * \param nYFactor Factor by which y dimension is changed. 
 * \param eInterpolation The type of eInterpolation to perform resampling.
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref resize_error_codes
 */
NppStatus 
nppiResize_16u_C4R(const Npp16u * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                         Npp16u * pDst, int nDstStep, NppiSize dstROISize,
                   double nXFactor, double nYFactor, int eInterpolation);

/**
 * 4 channel 16-bit unsigned image resize not affecting alpha.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Size in pixels of the source image
 * \param oSrcROI Region of interest in the source image.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param dstROISize Size in pixels of the destination image.
 * \param nXFactor Factor by which x dimension is changed. 
 * \param nYFactor Factor by which y dimension is changed. 
 * \param eInterpolation The type of interpolation to perform resampling.
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref resize_error_codes
 */
NppStatus 
nppiResize_16u_AC4R(const Npp16u * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                          Npp16u * pDst, int nDstStep, NppiSize dstROISize,
                    double nXFactor, double nYFactor, int eInterpolation);

/**
 * 3 channel 16-bit unsigned planar image resize.
 *
 * \param pSrc \ref source_planar_image_pointer_array (host memory array containing device memory image plane pointers).
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Size in pixels of the source image.
 * \param oSrcROI Region of interest in the source image.
 * \param pDst \ref destination_planar_image_pointer_array (host memory array containing device memory image plane pointers).
 * \param nDstStep \ref destination_image_line_step.
 * \param dstROISize Size in pixels of the destination image.
 * \param nXFactor Factor by which x dimension is changed. 
 * \param nYFactor Factor by which y dimension is changed. 
 * \param eInterpolation The type of eInterpolation to perform resampling.
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref resize_error_codes
 */
NppStatus 
nppiResize_16u_P3R(const Npp16u * const pSrc[3], NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                         Npp16u * pDst[3], int nDstStep, NppiSize dstROISize,
                   double nXFactor, double nYFactor, int eInterpolation);

/**
 * 4 channel 16-bit unsigned planar image resize.
 *
 * \param pSrc \ref source_planar_image_pointer_array (host memory array containing device memory image plane pointers).
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Size in pixels of the source image.
 * \param oSrcROI Region of interest in the source image.
 * \param pDst \ref destination_planar_image_pointer_array (host memory array containing device memory image plane pointers).
 * \param nDstStep \ref destination_image_line_step.
 * \param dstROISize Size in pixels of the destination image.
 * \param nXFactor Factor by which x dimension is changed. 
 * \param nYFactor Factor by which y dimension is changed. 
 * \param eInterpolation The type of eInterpolation to perform resampling.
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref resize_error_codes
 */
NppStatus 
nppiResize_16u_P4R(const Npp16u * const pSrc[4], NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                         Npp16u * pDst[4], int nDstStep, NppiSize dstROISize,
                   double nXFactor, double nYFactor, int eInterpolation);

/**
 * 1 channel 32-bit floating point image resize.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Size in pixels of the source image
 * \param oSrcROI Region of interest in the source image.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param dstROISize Size in pixels of the destination image.
 * \param nXFactor Factor by which x dimension is changed. 
 * \param nYFactor Factor by which y dimension is changed. 
 * \param eInterpolation The type of eInterpolation to perform resampling.
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref resize_error_codes
 */
NppStatus 
nppiResize_32f_C1R(const Npp32f * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                         Npp32f * pDst, int nDstStep, NppiSize dstROISize,
                  double nXFactor, double nYFactor, int eInterpolation);

/**
 * 3 channel 32-bit floating point image resize.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Size in pixels of the source image
 * \param oSrcROI Region of interest in the source image.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param dstROISize Size in pixels of the destination image.
 * \param nXFactor Factor by which x dimension is changed. 
 * \param nYFactor Factor by which y dimension is changed. 
 * \param eInterpolation The type of eInterpolation to perform resampling.
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref resize_error_codes
 */
NppStatus 
nppiResize_32f_C3R(const Npp32f * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                         Npp32f * pDst, int nDstStep, NppiSize dstROISize,
                   double nXFactor, double nYFactor, int eInterpolation);

/**
 * 4 channel 32-bit floating point image resize.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Size in pixels of the source image
 * \param oSrcROI Region of interest in the source image.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param dstROISize Size in pixels of the destination image.
 * \param nXFactor Factor by which x dimension is changed. 
 * \param nYFactor Factor by which y dimension is changed. 
 * \param eInterpolation The type of eInterpolation to perform resampling.
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref resize_error_codes
 */
NppStatus 
nppiResize_32f_C4R(const Npp32f * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                         Npp32f * pDst, int nDstStep, NppiSize dstROISize,
                   double nXFactor, double nYFactor, int eInterpolation);

/**
 * 4 channel 32-bit floating point image resize not affecting alpha.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Size in pixels of the source image
 * \param oSrcROI Region of interest in the source image.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param dstROISize Size in pixels of the destination image.
 * \param nXFactor Factor by which x dimension is changed. 
 * \param nYFactor Factor by which y dimension is changed. 
 * \param eInterpolation The type of interpolation to perform resampling.
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref resize_error_codes
 */
NppStatus 
nppiResize_32f_AC4R(const Npp32f * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                          Npp32f * pDst, int nDstStep, NppiSize dstROISize,
                    double nXFactor, double nYFactor, int eInterpolation);

/**
 * 3 channel 32-bit floating point planar image resize.
 *
 * \param pSrc \ref source_planar_image_pointer_array (host memory array containing device memory image plane pointers).
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Size in pixels of the source image.
 * \param oSrcROI Region of interest in the source image.
 * \param pDst \ref destination_planar_image_pointer_array (host memory array containing device memory image plane pointers).
 * \param nDstStep \ref destination_image_line_step.
 * \param dstROISize Size in pixels of the destination image.
 * \param nXFactor Factor by which x dimension is changed. 
 * \param nYFactor Factor by which y dimension is changed. 
 * \param eInterpolation The type of eInterpolation to perform resampling.
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref resize_error_codes
 */
NppStatus 
nppiResize_32f_P3R(const Npp32f * const pSrc[3], NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                         Npp32f * pDst[3], int nDstStep, NppiSize dstROISize,
                   double nXFactor, double nYFactor, int eInterpolation);

/**
 * 4 channel 32-bit floating point planar image resize.
 *
 * \param pSrc \ref source_planar_image_pointer_array (host memory array containing device memory image plane pointers).
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Size in pixels of the source image.
 * \param oSrcROI Region of interest in the source image.
 * \param pDst \ref destination_planar_image_pointer_array (host memory array containing device memory image plane pointers).
 * \param nDstStep \ref destination_image_line_step.
 * \param dstROISize Size in pixels of the destination image.
 * \param nXFactor Factor by which x dimension is changed. 
 * \param nYFactor Factor by which y dimension is changed. 
 * \param eInterpolation The type of eInterpolation to perform resampling.
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref resize_error_codes
 */
NppStatus 
nppiResize_32f_P4R(const Npp32f * const pSrc[4], NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                         Npp32f * pDst[4], int nDstStep, NppiSize dstROISize,
                   double nXFactor, double nYFactor, int eInterpolation);

/** @} */

/** @} image_resize */

/** @defgroup image_remap Remap
 *
 * Remap supports the following interpolation modes:
 *
 *   NPPI_INTER_NN
 *   NPPI_INTER_LINEAR
 *   NPPI_INTER_CUBIC
 *   NPPI_INTER_CUBIC2P_BSPLINE
 *   NPPI_INTER_CUBIC2P_CATMULLROM
 *   NPPI_INTER_CUBIC2P_B05C03
 *   NPPI_INTER_LANCZOS
 *
 * Remap chooses source pixels using pixel coordinates explicitely supplied in two 2D device memory image arrays pointed to by the pXMap and pYMap pointers.
 * The pXMap array contains the X coordinated and the pYMap array contains the Y coordinate of the corresponding source image pixel to
 * use as input.   These coordinates are in floating point format so fraction pixel positions can be used. The coordinates of the source
 * pixel to sample are determined as follows:
 *
 *   nSrcX = pxMap[nDstX, nDstY]
 *   nSrcY = pyMap[nDstX, nDstY]
 *
 * In the Remap functions below source image clip checking is handled as follows:
 *
 * If the source pixel fractional x and y coordinates are greater than or equal to oSizeROI.x and less than oSizeROI.x + oSizeROI.width and
 * greater than or equal to oSizeROI.y and less than oSizeROI.y + oSizeROI.height then the source pixel is considered to be within
 * the source image clip rectangle and the source image is sampled.  Otherwise the source image is not sampled and a destination pixel is not
 * written to the destination image. 
 *
 * \section remap_error_codes Error Codes
 * The remap primitives return the following error codes:
 *
 *         - ::NPP_WRONG_INTERSECTION_ROI_ERROR indicates an error condition if
 *           srcROIRect has no intersection with the source image.
 *         - ::NPP_INTERPOLATION_ERROR if eInterpolation has an illegal value.
 *
 * @{
 *
 */

/** @name Remap
 * Remaps images.
 *                                    
 * @{
 *
 */

/**
 * 1 channel 8-bit unsigned image remap.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Size in pixels of the source image.
 * \param oSrcROI Region of interest in the source image.
 * \param pXMap Device memory pointer to 2D image array of X coordinate values to be used when sampling source image. 
 * \param nXMapStep pXMap image array line step in bytes.
 * \param pYMap Device memory pointer to 2D image array of Y coordinate values to be used when sampling source image. 
 * \param nYMapStep pYMap image array line step in bytes.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstSizeROI Region of interest size in the destination image.
 * \param eInterpolation The type of eInterpolation to perform resampling.
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref remap_error_codes
 */
NppStatus 
nppiRemap_8u_C1R(const Npp8u  * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                  const Npp32f * pXMap, int nXMapStep, const Npp32f * pYMap, int nYMapStep,
                        Npp8u  * pDst, int nDstStep, NppiSize oDstSizeROI, int eInterpolation);

/**
 * 3 channel 8-bit unsigned image remap.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Size in pixels of the source image.
 * \param oSrcROI Region of interest in the source image.
 * \param pXMap Device memory pointer to 2D image array of X coordinate values to be used when sampling source image. 
 * \param nXMapStep pXMap image array line step in bytes.
 * \param pYMap Device memory pointer to 2D image array of Y coordinate values to be used when sampling source image. 
 * \param nYMapStep pYMap image array line step in bytes.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstSizeROI Region of interest size in the destination image.
 * \param eInterpolation The type of eInterpolation to perform resampling.
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref remap_error_codes
 */
NppStatus 
nppiRemap_8u_C3R(const Npp8u  * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                  const Npp32f * pXMap, int nXMapStep, const Npp32f * pYMap, int nYMapStep,
                        Npp8u  * pDst, int nDstStep, NppiSize oDstSizeROI, int eInterpolation);

/**
 * 4 channel 8-bit unsigned image remap.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Size in pixels of the source image.
 * \param oSrcROI Region of interest in the source image.
 * \param pXMap Device memory pointer to 2D image array of X coordinate values to be used when sampling source image. 
 * \param nXMapStep pXMap image array line step in bytes.
 * \param pYMap Device memory pointer to 2D image array of Y coordinate values to be used when sampling source image. 
 * \param nYMapStep pYMap image array line step in bytes.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstSizeROI Region of interest size in the destination image.
 * \param eInterpolation The type of eInterpolation to perform resampling.
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref remap_error_codes
 */
NppStatus 
nppiRemap_8u_C4R(const Npp8u  * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                  const Npp32f * pXMap, int nXMapStep, const Npp32f * pYMap, int nYMapStep,
                        Npp8u  * pDst, int nDstStep, NppiSize oDstSizeROI, int eInterpolation);

/**
 * 4 channel 8-bit unsigned image remap not affecting alpha.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Size in pixels of the source image.
 * \param oSrcROI Region of interest in the source image.
 * \param pXMap Device memory pointer to 2D image array of X coordinate values to be used when sampling source image. 
 * \param nXMapStep pXMap image array line step in bytes.
 * \param pYMap Device memory pointer to 2D image array of Y coordinate values to be used when sampling source image. 
 * \param nYMapStep pYMap image array line step in bytes.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstSizeROI Region of interest size in the destination image.
 * \param eInterpolation The type of interpolation to perform resampling.
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref remap_error_codes
 */
NppStatus 
nppiRemap_8u_AC4R(const Npp8u  * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                   const Npp32f * pXMap, int nXMapStep, const Npp32f * pYMap, int nYMapStep,
                         Npp8u  * pDst, int nDstStep, NppiSize oDstSizeROI, int eInterpolation);

/**
 * 3 channel 8-bit unsigned planar image remap.
 *
 * \param pSrc \ref source_planar_image_pointer_array.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Size in pixels of the source image.
 * \param oSrcROI Region of interest in the source image.
 * \param pXMap Device memory pointer to 2D image array of X coordinate values to be used when sampling source image. 
 * \param nXMapStep pXMap image array line step in bytes.
 * \param pYMap Device memory pointer to 2D image array of Y coordinate values to be used when sampling source image. 
 * \param nYMapStep pYMap image array line step in bytes.
 * \param pDst \ref destination_planar_image_pointer_array.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstSizeROI Region of interest size in the destination image.
 * \param eInterpolation The type of eInterpolation to perform resampling.
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref remap_error_codes
 */
NppStatus 
nppiRemap_8u_P3R(const Npp8u  * const pSrc[3], NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                  const Npp32f * pXMap, int nXMapStep, const Npp32f * pYMap, int nYMapStep,
                        Npp8u  * pDst[3], int nDstStep, NppiSize oDstSizeROI, int eInterpolation);

/**
 * 4 channel 8-bit unsigned planar image remap.
 *
 * \param pSrc \ref source_planar_image_pointer_array.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Size in pixels of the source image.
 * \param oSrcROI Region of interest in the source image.
 * \param pXMap Device memory pointer to 2D image array of X coordinate values to be used when sampling source image. 
 * \param nXMapStep pXMap image array line step in bytes.
 * \param pYMap Device memory pointer to 2D image array of Y coordinate values to be used when sampling source image. 
 * \param nYMapStep pYMap image array line step in bytes.
 * \param pDst \ref destination_planar_image_pointer_array.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstSizeROI Region of interest size in the destination image.
 * \param eInterpolation The type of eInterpolation to perform resampling.
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref remap_error_codes
 */
NppStatus 
nppiRemap_8u_P4R(const Npp8u  * const pSrc[4], NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                  const Npp32f * pXMap, int nXMapStep, const Npp32f * pYMap, int nYMapStep,
                        Npp8u  * pDst[4], int nDstStep, NppiSize oDstSizeROI, int eInterpolation);

/**
 * 1 channel 16-bit unsigned image remap.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Size in pixels of the source image.
 * \param oSrcROI Region of interest in the source image.
 * \param pXMap Device memory pointer to 2D image array of X coordinate values to be used when sampling source image. 
 * \param nXMapStep pXMap image array line step in bytes.
 * \param pYMap Device memory pointer to 2D image array of Y coordinate values to be used when sampling source image. 
 * \param nYMapStep pYMap image array line step in bytes.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstSizeROI Region of interest size in the destination image.
 * \param eInterpolation The type of eInterpolation to perform resampling.
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref remap_error_codes
 */
NppStatus 
nppiRemap_16u_C1R(const Npp16u * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                   const Npp32f * pXMap, int nXMapStep, const Npp32f * pYMap, int nYMapStep,
                         Npp16u * pDst, int nDstStep, NppiSize oDstSizeROI, int eInterpolation);

/**
 * 3 channel 16-bit unsigned image remap.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Size in pixels of the source image.
 * \param oSrcROI Region of interest in the source image.
 * \param pXMap Device memory pointer to 2D image array of X coordinate values to be used when sampling source image. 
 * \param nXMapStep pXMap image array line step in bytes.
 * \param pYMap Device memory pointer to 2D image array of Y coordinate values to be used when sampling source image. 
 * \param nYMapStep pYMap image array line step in bytes.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstSizeROI Region of interest size in the destination image.
 * \param eInterpolation The type of eInterpolation to perform resampling.
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref remap_error_codes
 */
NppStatus 
nppiRemap_16u_C3R(const Npp16u * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                   const Npp32f * pXMap, int nXMapStep, const Npp32f * pYMap, int nYMapStep,
                         Npp16u * pDst, int nDstStep, NppiSize oDstSizeROI, int eInterpolation);

/**
 * 4 channel 16-bit unsigned image remap.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Size in pixels of the source image.
 * \param oSrcROI Region of interest in the source image.
 * \param pXMap Device memory pointer to 2D image array of X coordinate values to be used when sampling source image. 
 * \param nXMapStep pXMap image array line step in bytes.
 * \param pYMap Device memory pointer to 2D image array of Y coordinate values to be used when sampling source image. 
 * \param nYMapStep pYMap image array line step in bytes.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstSizeROI Region of interest size in the destination image.
 * \param eInterpolation The type of eInterpolation to perform resampling.
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref remap_error_codes
 */
NppStatus 
nppiRemap_16u_C4R(const Npp16u * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                   const Npp32f * pXMap, int nXMapStep, const Npp32f * pYMap, int nYMapStep,
                         Npp16u * pDst, int nDstStep, NppiSize oDstSizeROI, int eInterpolation);

/**
 * 4 channel 16-bit unsigned image remap not affecting alpha.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Size in pixels of the source image.
 * \param oSrcROI Region of interest in the source image.
 * \param pXMap Device memory pointer to 2D image array of X coordinate values to be used when sampling source image. 
 * \param nXMapStep pXMap image array line step in bytes.
 * \param pYMap Device memory pointer to 2D image array of Y coordinate values to be used when sampling source image. 
 * \param nYMapStep pYMap image array line step in bytes.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstSizeROI Region of interest size in the destination image.
 * \param eInterpolation The type of interpolation to perform resampling.
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref remap_error_codes
 */
NppStatus 
nppiRemap_16u_AC4R(const Npp16u * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                    const Npp32f * pXMap, int nXMapStep, const Npp32f * pYMap, int nYMapStep,
                          Npp16u * pDst, int nDstStep, NppiSize oDstSizeROI, int eInterpolation);

/**
 * 3 channel 16-bit unsigned planar image remap.
 *
 * \param pSrc \ref source_planar_image_pointer_array.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Size in pixels of the source image.
 * \param oSrcROI Region of interest in the source image.
 * \param pXMap Device memory pointer to 2D image array of X coordinate values to be used when sampling source image. 
 * \param nXMapStep pXMap image array line step in bytes.
 * \param pYMap Device memory pointer to 2D image array of Y coordinate values to be used when sampling source image. 
 * \param nYMapStep pYMap image array line step in bytes.
 * \param pDst \ref destination_planar_image_pointer_array.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstSizeROI Region of interest size in the destination image.
 * \param eInterpolation The type of eInterpolation to perform resampling.
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref remap_error_codes
 */
NppStatus 
nppiRemap_16u_P3R(const Npp16u * const pSrc[3], NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                   const Npp32f * pXMap, int nXMapStep, const Npp32f * pYMap, int nYMapStep,
                         Npp16u * pDst[3], int nDstStep, NppiSize oDstSizeROI, int eInterpolation);

/**
 * 4 channel 16-bit unsigned planar image remap.
 *
 * \param pSrc \ref source_planar_image_pointer_array.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Size in pixels of the source image.
 * \param oSrcROI Region of interest in the source image.
 * \param pXMap Device memory pointer to 2D image array of X coordinate values to be used when sampling source image. 
 * \param nXMapStep pXMap image array line step in bytes.
 * \param pYMap Device memory pointer to 2D image array of Y coordinate values to be used when sampling source image. 
 * \param nYMapStep pYMap image array line step in bytes.
 * \param pDst \ref destination_planar_image_pointer_array.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstSizeROI Region of interest size in the destination image.
 * \param eInterpolation The type of eInterpolation to perform resampling.
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref remap_error_codes
 */
NppStatus 
nppiRemap_16u_P4R(const Npp16u * const pSrc[4], NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                   const Npp32f * pXMap, int nXMapStep, const Npp32f * pYMap, int nYMapStep,
                         Npp16u * pDst[4], int nDstStep, NppiSize oDstSizeROI, int eInterpolation);

/**
 * 1 channel 16-bit signed image remap.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Size in pixels of the source image.
 * \param oSrcROI Region of interest in the source image.
 * \param pXMap Device memory pointer to 2D image array of X coordinate values to be used when sampling source image. 
 * \param nXMapStep pXMap image array line step in bytes.
 * \param pYMap Device memory pointer to 2D image array of Y coordinate values to be used when sampling source image. 
 * \param nYMapStep pYMap image array line step in bytes.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstSizeROI Region of interest size in the destination image.
 * \param eInterpolation The type of eInterpolation to perform resampling.
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref remap_error_codes
 */
NppStatus 
nppiRemap_16s_C1R(const Npp16s * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                   const Npp32f * pXMap, int nXMapStep, const Npp32f * pYMap, int nYMapStep,
                         Npp16s * pDst, int nDstStep, NppiSize oDstSizeROI, int eInterpolation);

/**
 * 3 channel 16-bit signed image remap.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Size in pixels of the source image.
 * \param oSrcROI Region of interest in the source image.
 * \param pXMap Device memory pointer to 2D image array of X coordinate values to be used when sampling source image. 
 * \param nXMapStep pXMap image array line step in bytes.
 * \param pYMap Device memory pointer to 2D image array of Y coordinate values to be used when sampling source image. 
 * \param nYMapStep pYMap image array line step in bytes.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstSizeROI Region of interest size in the destination image.
 * \param eInterpolation The type of eInterpolation to perform resampling.
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref remap_error_codes
 */
NppStatus 
nppiRemap_16s_C3R(const Npp16s * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                   const Npp32f * pXMap, int nXMapStep, const Npp32f * pYMap, int nYMapStep,
                         Npp16s * pDst, int nDstStep, NppiSize oDstSizeROI, int eInterpolation);

/**
 * 4 channel 16-bit signed image remap.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Size in pixels of the source image.
 * \param oSrcROI Region of interest in the source image.
 * \param pXMap Device memory pointer to 2D image array of X coordinate values to be used when sampling source image. 
 * \param nXMapStep pXMap image array line step in bytes.
 * \param pYMap Device memory pointer to 2D image array of Y coordinate values to be used when sampling source image. 
 * \param nYMapStep pYMap image array line step in bytes.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstSizeROI Region of interest size in the destination image.
 * \param eInterpolation The type of eInterpolation to perform resampling.
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref remap_error_codes
 */
NppStatus 
nppiRemap_16s_C4R(const Npp16s * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                   const Npp32f * pXMap, int nXMapStep, const Npp32f * pYMap, int nYMapStep,
                         Npp16s * pDst, int nDstStep, NppiSize oDstSizeROI, int eInterpolation);

/**
 * 4 channel 16-bit signed image remap not affecting alpha.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Size in pixels of the source image.
 * \param oSrcROI Region of interest in the source image.
 * \param pXMap Device memory pointer to 2D image array of X coordinate values to be used when sampling source image. 
 * \param nXMapStep pXMap image array line step in bytes.
 * \param pYMap Device memory pointer to 2D image array of Y coordinate values to be used when sampling source image. 
 * \param nYMapStep pYMap image array line step in bytes.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstSizeROI Region of interest size in the destination image.
 * \param eInterpolation The type of interpolation to perform resampling
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref remap_error_codes
 */
NppStatus 
nppiRemap_16s_AC4R(const Npp16s * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                    const Npp32f * pXMap, int nXMapStep, const Npp32f * pYMap, int nYMapStep,
                          Npp16s * pDst, int nDstStep, NppiSize oDstSizeROI, int eInterpolation);

/**
 * 3 channel 16-bit signed planar image remap.
 *
 * \param pSrc \ref source_planar_image_pointer_array.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Size in pixels of the source image.
 * \param oSrcROI Region of interest in the source image.
 * \param pXMap Device memory pointer to 2D image array of X coordinate values to be used when sampling source image. 
 * \param nXMapStep pXMap image array line step in bytes.
 * \param pYMap Device memory pointer to 2D image array of Y coordinate values to be used when sampling source image. 
 * \param nYMapStep pYMap image array line step in bytes.
 * \param pDst \ref destination_planar_image_pointer_array.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstSizeROI Region of interest size in the destination image.
 * \param eInterpolation The type of eInterpolation to perform resampling.
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref remap_error_codes
 */
NppStatus 
nppiRemap_16s_P3R(const Npp16s * const pSrc[3], NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                   const Npp32f * pXMap, int nXMapStep, const Npp32f * pYMap, int nYMapStep,
                         Npp16s * pDst[3], int nDstStep, NppiSize oDstSizeROI, int eInterpolation);

/**
 * 4 channel 16-bit signed planar image remap.
 *
 * \param pSrc \ref source_planar_image_pointer_array.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Size in pixels of the source image.
 * \param oSrcROI Region of interest in the source image.
 * \param pXMap Device memory pointer to 2D image array of X coordinate values to be used when sampling source image. 
 * \param nXMapStep pXMap image array line step in bytes.
 * \param pYMap Device memory pointer to 2D image array of Y coordinate values to be used when sampling source image. 
 * \param nYMapStep pYMap image array line step in bytes.
 * \param pDst \ref destination_planar_image_pointer_array.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstSizeROI Region of interest size in the destination image.
 * \param eInterpolation The type of eInterpolation to perform resampling.
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref remap_error_codes
 */
NppStatus 
nppiRemap_16s_P4R(const Npp16s * const pSrc[4], NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                   const Npp32f * pXMap, int nXMapStep, const Npp32f * pYMap, int nYMapStep,
                         Npp16s * pDst[4], int nDstStep, NppiSize oDstSizeROI, int eInterpolation);

/**
 * 1 channel 32-bit floating point image remap.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Size in pixels of the source image.
 * \param oSrcROI Region of interest in the source image.
 * \param pXMap Device memory pointer to 2D image array of X coordinate values to be used when sampling source image. 
 * \param nXMapStep pXMap image array line step in bytes.
 * \param pYMap Device memory pointer to 2D image array of Y coordinate values to be used when sampling source image. 
 * \param nYMapStep pYMap image array line step in bytes.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstSizeROI Region of interest size in the destination image.
 * \param eInterpolation The type of eInterpolation to perform resampling
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref remap_error_codes
 */
NppStatus 
nppiRemap_32f_C1R(const Npp32f * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                   const Npp32f * pXMap, int nXMapStep, const Npp32f * pYMap, int nYMapStep,
                         Npp32f * pDst, int nDstStep, NppiSize oDstSizeROI, int eInterpolation);

/**
 * 3 channel 32-bit floating point image remap.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Size in pixels of the source image.
 * \param oSrcROI Region of interest in the source image.
 * \param pXMap Device memory pointer to 2D image array of X coordinate values to be used when sampling source image. 
 * \param nXMapStep pXMap image array line step in bytes.
 * \param pYMap Device memory pointer to 2D image array of Y coordinate values to be used when sampling source image. 
 * \param nYMapStep pYMap image array line step in bytes.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstSizeROI Region of interest size in the destination image.
 * \param eInterpolation The type of eInterpolation to perform resampling
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref remap_error_codes
 */
NppStatus 
nppiRemap_32f_C3R(const Npp32f * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                   const Npp32f * pXMap, int nXMapStep, const Npp32f * pYMap, int nYMapStep,
                         Npp32f * pDst, int nDstStep, NppiSize oDstSizeROI, int eInterpolation);

/**
 * 4 channel 32-bit floating point image remap.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Size in pixels of the source image.
 * \param oSrcROI Region of interest in the source image.
 * \param pXMap Device memory pointer to 2D image array of X coordinate values to be used when sampling source image. 
 * \param nXMapStep pXMap image array line step in bytes.
 * \param pYMap Device memory pointer to 2D image array of Y coordinate values to be used when sampling source image. 
 * \param nYMapStep pYMap image array line step in bytes.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstSizeROI Region of interest size in the destination image.
 * \param eInterpolation The type of eInterpolation to perform resampling
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref remap_error_codes
 */
NppStatus 
nppiRemap_32f_C4R(const Npp32f * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                   const Npp32f * pXMap, int nXMapStep, const Npp32f * pYMap, int nYMapStep,
                         Npp32f * pDst, int nDstStep, NppiSize oDstSizeROI, int eInterpolation);

/**
 * 4 channel 32-bit floating point image remap not affecting alpha.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Size in pixels of the source image.
 * \param oSrcROI Region of interest in the source image.
 * \param pXMap Device memory pointer to 2D image array of X coordinate values to be used when sampling source image. 
 * \param nXMapStep pXMap image array line step in bytes.
 * \param pYMap Device memory pointer to 2D image array of Y coordinate values to be used when sampling source image. 
 * \param nYMapStep pYMap image array line step in bytes.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstSizeROI Region of interest size in the destination image.
 * \param eInterpolation The type of interpolation to perform resampling
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref remap_error_codes
 */
NppStatus 
nppiRemap_32f_AC4R(const Npp32f * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                    const Npp32f * pXMap, int nXMapStep, const Npp32f * pYMap, int nYMapStep,
                          Npp32f * pDst, int nDstStep, NppiSize oDstSizeROI, int eInterpolation);

/**
 * 3 channel 32-bit floating point planar image remap.
 *
 * \param pSrc \ref source_planar_image_pointer_array.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Size in pixels of the source image.
 * \param oSrcROI Region of interest in the source image.
 * \param pXMap Device memory pointer to 2D image array of X coordinate values to be used when sampling source image. 
 * \param nXMapStep pXMap image array line step in bytes.
 * \param pYMap Device memory pointer to 2D image array of Y coordinate values to be used when sampling source image. 
 * \param nYMapStep pYMap image array line step in bytes.
 * \param pDst \ref destination_planar_image_pointer_array.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstSizeROI Region of interest size in the destination image.
 * \param eInterpolation The type of eInterpolation to perform resampling.
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref remap_error_codes
 */
NppStatus 
nppiRemap_32f_P3R(const Npp32f * const pSrc[3], NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                   const Npp32f * pXMap, int nXMapStep, const Npp32f * pYMap, int nYMapStep,
                         Npp32f * pDst[3], int nDstStep, NppiSize oDstSizeROI, int eInterpolation);

/**
 * 4 channel 32-bit floating point planar image remap.
 *
 * \param pSrc \ref source_planar_image_pointer_array.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Size in pixels of the source image.
 * \param oSrcROI Region of interest in the source image.
 * \param pXMap Device memory pointer to 2D image array of X coordinate values to be used when sampling source image. 
 * \param nXMapStep pXMap image array line step in bytes.
 * \param pYMap Device memory pointer to 2D image array of Y coordinate values to be used when sampling source image. 
 * \param nYMapStep pYMap image array line step in bytes.
 * \param pDst \ref destination_planar_image_pointer_array.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstSizeROI Region of interest size in the destination image.
 * \param eInterpolation The type of eInterpolation to perform resampling.
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref remap_error_codes
 */
NppStatus 
nppiRemap_32f_P4R(const Npp32f * const pSrc[4], NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                   const Npp32f * pXMap, int nXMapStep, const Npp32f * pYMap, int nYMapStep,
                         Npp32f * pDst[4], int nDstStep, NppiSize oDstSizeROI, int eInterpolation);

/**
 * 1 channel 64-bit floating point image remap.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Size in pixels of the source image.
 * \param oSrcROI Region of interest in the source image.
 * \param pXMap Device memory pointer to 2D image array of X coordinate values to be used when sampling source image. 
 * \param nXMapStep pXMap image array line step in bytes.
 * \param pYMap Device memory pointer to 2D image array of Y coordinate values to be used when sampling source image. 
 * \param nYMapStep pYMap image array line step in bytes.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstSizeROI Region of interest size in the destination image.
 * \param eInterpolation The type of eInterpolation to perform resampling
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref remap_error_codes
 */
NppStatus 
nppiRemap_64f_C1R(const Npp64f * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                   const Npp64f * pXMap, int nXMapStep, const Npp64f * pYMap, int nYMapStep,
                         Npp64f * pDst, int nDstStep, NppiSize oDstSizeROI, int eInterpolation);

/**
 * 3 channel 64-bit floating point image remap.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Size in pixels of the source image.
 * \param oSrcROI Region of interest in the source image.
 * \param pXMap Device memory pointer to 2D image array of X coordinate values to be used when sampling source image. 
 * \param nXMapStep pXMap image array line step in bytes.
 * \param pYMap Device memory pointer to 2D image array of Y coordinate values to be used when sampling source image. 
 * \param nYMapStep pYMap image array line step in bytes.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstSizeROI Region of interest size in the destination image.
 * \param eInterpolation The type of eInterpolation to perform resampling
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref remap_error_codes
 */
NppStatus 
nppiRemap_64f_C3R(const Npp64f * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                   const Npp64f * pXMap, int nXMapStep, const Npp64f * pYMap, int nYMapStep,
                         Npp64f * pDst, int nDstStep, NppiSize oDstSizeROI, int eInterpolation);

/**
 * 4 channel 64-bit floating point image remap.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Size in pixels of the source image.
 * \param oSrcROI Region of interest in the source image.
 * \param pXMap Device memory pointer to 2D image array of X coordinate values to be used when sampling source image. 
 * \param nXMapStep pXMap image array line step in bytes.
 * \param pYMap Device memory pointer to 2D image array of Y coordinate values to be used when sampling source image. 
 * \param nYMapStep pYMap image array line step in bytes.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstSizeROI Region of interest size in the destination image.
 * \param eInterpolation The type of eInterpolation to perform resampling
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref remap_error_codes
 */
NppStatus 
nppiRemap_64f_C4R(const Npp64f * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                   const Npp64f * pXMap, int nXMapStep, const Npp64f * pYMap, int nYMapStep,
                         Npp64f * pDst, int nDstStep, NppiSize oDstSizeROI, int eInterpolation);

/**
 * 4 channel 64-bit floating point image remap not affecting alpha.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Size in pixels of the source image.
 * \param oSrcROI Region of interest in the source image.
 * \param pXMap Device memory pointer to 2D image array of X coordinate values to be used when sampling source image. 
 * \param nXMapStep pXMap image array line step in bytes.
 * \param pYMap Device memory pointer to 2D image array of Y coordinate values to be used when sampling source image. 
 * \param nYMapStep pYMap image array line step in bytes.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstSizeROI Region of interest size in the destination image.
 * \param eInterpolation The type of interpolation to perform resampling
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref remap_error_codes
 */
NppStatus 
nppiRemap_64f_AC4R(const Npp64f * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                    const Npp64f * pXMap, int nXMapStep, const Npp64f * pYMap, int nYMapStep,
                          Npp64f * pDst, int nDstStep, NppiSize oDstSizeROI, int eInterpolation);

/**
 * 3 channel 64-bit floating point planar image remap.
 *
 * \param pSrc \ref source_planar_image_pointer_array.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Size in pixels of the source image.
 * \param oSrcROI Region of interest in the source image.
 * \param pXMap Device memory pointer to 2D image array of X coordinate values to be used when sampling source image. 
 * \param nXMapStep pXMap image array line step in bytes.
 * \param pYMap Device memory pointer to 2D image array of Y coordinate values to be used when sampling source image. 
 * \param nYMapStep pYMap image array line step in bytes.
 * \param pDst \ref destination_planar_image_pointer_array.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstSizeROI Region of interest size in the destination image.
 * \param eInterpolation The type of eInterpolation to perform resampling.
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref remap_error_codes
 */
NppStatus 
nppiRemap_64f_P3R(const Npp64f * const pSrc[3], NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                   const Npp64f * pXMap, int nXMapStep, const Npp64f * pYMap, int nYMapStep,
                         Npp64f * pDst[3], int nDstStep, NppiSize oDstSizeROI, int eInterpolation);

/**
 * 4 channel 64-bit floating point planar image remap.
 *
 * \param pSrc \ref source_planar_image_pointer_array.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Size in pixels of the source image.
 * \param oSrcROI Region of interest in the source image.
 * \param pXMap Device memory pointer to 2D image array of X coordinate values to be used when sampling source image. 
 * \param nXMapStep pXMap image array line step in bytes.
 * \param pYMap Device memory pointer to 2D image array of Y coordinate values to be used when sampling source image. 
 * \param nYMapStep pYMap image array line step in bytes.
 * \param pDst \ref destination_planar_image_pointer_array.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstSizeROI Region of interest size in the destination image.
 * \param eInterpolation The type of eInterpolation to perform resampling.
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref remap_error_codes
 */
NppStatus 
nppiRemap_64f_P4R(const Npp64f * const pSrc[4], NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                   const Npp64f * pXMap, int nXMapStep, const Npp64f * pYMap, int nYMapStep,
                         Npp64f * pDst[4], int nDstStep, NppiSize oDstSizeROI, int eInterpolation);

/** @} */

/** @} image_remap */

/** @defgroup image_rotate Rotate
 *
 *  Rotates an image around the origin (0,0) and then shifts it.
 *
 * \section rotate_error_codes Rotate Error Codes
 * - ::NPP_INTERPOLATION_ERROR if eInterpolation has an illegal value.
 * - ::NPP_RECTANGLE_ERROR Indicates an error condition if width or height of
 *   the intersection of the oSrcROI and source image is less than or
 *   equal to 1.
 * - ::NPP_WRONG_INTERSECTION_ROI_ERROR indicates an error condition if
 *   srcROIRect has no intersection with the source image.
 * - ::NPP_WRONG_INTERSECTION_QUAD_WARNING indicates a warning that no
 *   operation is performed if the transformed source ROI does not
 *   intersect the destination ROI.
 *
 * @{
 *
 */

/** @name Utility Functions
 *
 * @{
 *
 */

/**
 * Compute shape of rotated image.
 * 
 * \param oSrcROI Region-of-interest of the source image.
 * \param aQuad Array of 2D points. These points are the locations of the corners
 *      of the rotated ROI. 
 * \param nAngle The rotation nAngle.
 * \param nShiftX Post-rotation shift in x-direction
 * \param nShiftY Post-rotation shift in y-direction
 * \return \ref roi_error_codes.
 */
NppStatus
nppiGetRotateQuad(NppiRect oSrcROI, double aQuad[4][2], double nAngle, double nShiftX, double nShiftY);

/**
 * Compute bounding-box of rotated image.
 * \param oSrcROI Region-of-interest of the source image.
 * \param aBoundingBox Two 2D points representing the bounding-box of the rotated image. All four points
 *      from nppiGetRotateQuad are contained inside the axis-aligned rectangle spanned by the the two
 *      points of this bounding box.
 * \param nAngle The rotation angle.
 * \param nShiftX Post-rotation shift in x-direction.
 * \param nShiftY Post-rotation shift in y-direction.
 *
 * \return \ref roi_error_codes.
 */
NppStatus 
nppiGetRotateBound(NppiRect oSrcROI, double aBoundingBox[2][2], double nAngle, double nShiftX, double nShiftY);

/** @} Utility Functions */

/** @name Rotate
 *
 * @{
 *
 */

/**
 * 8-bit unsigned image rotate.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Size in pixels of the source image
 * \param oSrcROI Region of interest in the source image.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Region of interest in the destination image.
 * \param nAngle The angle of rotation in degrees.
 * \param nShiftX Shift along horizontal axis 
 * \param nShiftY Shift along vertical axis 
 * \param eInterpolation The type of interpolation to perform resampling
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref rotate_error_codes
 */
NppStatus 
nppiRotate_8u_C1R(const Npp8u * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                        Npp8u * pDst, int nDstStep, NppiRect oDstROI,
                  double nAngle, double nShiftX, double nShiftY, int eInterpolation);

/**
 * 3 channel 8-bit unsigned image rotate.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Size in pixels of the source image
 * \param oSrcROI Region of interest in the source image.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Region of interest in the destination image.
 * \param nAngle The angle of rotation in degrees.
 * \param nShiftX Shift along horizontal axis 
 * \param nShiftY Shift along vertical axis 
 * \param eInterpolation The type of interpolation to perform resampling
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref rotate_error_codes
 */
NppStatus 
nppiRotate_8u_C3R(const Npp8u * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                        Npp8u * pDst, int nDstStep, NppiRect oDstROI,
                  double nAngle, double nShiftX, double nShiftY, int eInterpolation);

/**
 * 4 channel 8-bit unsigned image rotate.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Size in pixels of the source image
 * \param oSrcROI Region of interest in the source image.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Region of interest in the destination image.
 * \param nAngle The angle of rotation in degrees.
 * \param nShiftX Shift along horizontal axis 
 * \param nShiftY Shift along vertical axis 
 * \param eInterpolation The type of interpolation to perform resampling
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref rotate_error_codes
 */
NppStatus 
nppiRotate_8u_C4R(const Npp8u * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                        Npp8u * pDst, int nDstStep, NppiRect oDstROI,
                  double nAngle, double nShiftX, double nShiftY, int eInterpolation);

/**
 * 4 channel 8-bit unsigned image rotate ignoring alpha channel.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Size in pixels of the source image
 * \param oSrcROI Region of interest in the source image.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Region of interest in the destination image.
 * \param nAngle The angle of rotation in degrees.
 * \param nShiftX Shift along horizontal axis 
 * \param nShiftY Shift along vertical axis 
 * \param eInterpolation The type of interpolation to perform resampling
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref rotate_error_codes
 */
NppStatus 
nppiRotate_8u_AC4R(const Npp8u * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                         Npp8u * pDst, int nDstStep, NppiRect oDstROI,
                   double nAngle, double nShiftX, double nShiftY, int eInterpolation);

/**
 * 16-bit unsigned image rotate.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Size in pixels of the source image
 * \param oSrcROI Region of interest in the source image.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Region of interest in the destination image.
 * \param nAngle The angle of rotation in degrees.
 * \param nShiftX Shift along horizontal axis 
 * \param nShiftY Shift along vertical axis 
 * \param eInterpolation The type of interpolation to perform resampling
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref rotate_error_codes
 */
NppStatus 
nppiRotate_16u_C1R(const Npp16u * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                         Npp16u * pDst, int nDstStep, NppiRect oDstROI,
                   double nAngle, double nShiftX, double nShiftY, int eInterpolation);

/**
 * 3 channel 16-bit unsigned image rotate.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Size in pixels of the source image
 * \param oSrcROI Region of interest in the source image.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Region of interest in the destination image.
 * \param nAngle The angle of rotation in degrees.
 * \param nShiftX Shift along horizontal axis 
 * \param nShiftY Shift along vertical axis 
 * \param eInterpolation The type of interpolation to perform resampling
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref rotate_error_codes
 */
NppStatus 
nppiRotate_16u_C3R(const Npp16u * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                         Npp16u * pDst, int nDstStep, NppiRect oDstROI,
                   double nAngle, double nShiftX, double nShiftY, int eInterpolation);

/**
 * 4 channel 16-bit unsigned image rotate.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Size in pixels of the source image
 * \param oSrcROI Region of interest in the source image.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Region of interest in the destination image.
 * \param nAngle The angle of rotation in degrees.
 * \param nShiftX Shift along horizontal axis 
 * \param nShiftY Shift along vertical axis 
 * \param eInterpolation The type of interpolation to perform resampling
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref rotate_error_codes
 */
NppStatus 
nppiRotate_16u_C4R(const Npp16u * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                         Npp16u * pDst, int nDstStep, NppiRect oDstROI,
                   double nAngle, double nShiftX, double nShiftY, int eInterpolation);

/**
 * 4 channel 16-bit unsigned image rotate ignoring alpha channel.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Size in pixels of the source image
 * \param oSrcROI Region of interest in the source image.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Region of interest in the destination image.
 * \param nAngle The angle of rotation in degrees.
 * \param nShiftX Shift along horizontal axis 
 * \param nShiftY Shift along vertical axis 
 * \param eInterpolation The type of interpolation to perform resampling
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref rotate_error_codes
 */
NppStatus 
nppiRotate_16u_AC4R(const Npp16u * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                          Npp16u * pDst, int nDstStep, NppiRect oDstROI,
                    double nAngle, double nShiftX, double nShiftY, int eInterpolation);

/**
 * 32-bit float image rotate.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Size in pixels of the source image
 * \param oSrcROI Region of interest in the source image.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Region of interest in the destination image.
 * \param nAngle The angle of rotation in degrees.
 * \param nShiftX Shift along horizontal axis 
 * \param nShiftY Shift along vertical axis 
 * \param eInterpolation The type of interpolation to perform resampling
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref rotate_error_codes
 */
NppStatus 
nppiRotate_32f_C1R(const Npp32f * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                         Npp32f * pDst, int nDstStep, NppiRect oDstROI,
                   double nAngle, double nShiftX, double nShiftY, int eInterpolation);

/**
 * 3 channel 32-bit float image rotate.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Size in pixels of the source image
 * \param oSrcROI Region of interest in the source image.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Region of interest in the destination image.
 * \param nAngle The angle of rotation in degrees.
 * \param nShiftX Shift along horizontal axis 
 * \param nShiftY Shift along vertical axis 
 * \param eInterpolation The type of interpolation to perform resampling
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref rotate_error_codes
 */
NppStatus 
nppiRotate_32f_C3R(const Npp32f * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                         Npp32f * pDst, int nDstStep, NppiRect oDstROI,
                   double nAngle, double nShiftX, double nShiftY, int eInterpolation);

/**
 * 4 channel 32-bit float image rotate.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Size in pixels of the source image
 * \param oSrcROI Region of interest in the source image.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Region of interest in the destination image.
 * \param nAngle The angle of rotation in degrees.
 * \param nShiftX Shift along horizontal axis 
 * \param nShiftY Shift along vertical axis 
 * \param eInterpolation The type of interpolation to perform resampling
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref rotate_error_codes
 */
NppStatus 
nppiRotate_32f_C4R(const Npp32f * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                         Npp32f * pDst, int nDstStep, NppiRect oDstROI,
                   double nAngle, double nShiftX, double nShiftY, int eInterpolation);

/**
 * 4 channel 32-bit float image rotate ignoring alpha channel.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcSize Size in pixels of the source image
 * \param oSrcROI Region of interest in the source image.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Region of interest in the destination image.
 * \param nAngle The angle of rotation in degrees.
 * \param nShiftX Shift along horizontal axis 
 * \param nShiftY Shift along vertical axis 
 * \param eInterpolation The type of interpolation to perform resampling
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref rotate_error_codes
 */
NppStatus 
nppiRotate_32f_AC4R(const Npp32f * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                          Npp32f * pDst, int nDstStep, NppiRect oDstROI,
                    double nAngle, double nShiftX, double nShiftY, int eInterpolation);
/** @} */

/** @} image_rotate */

/** @defgroup image_mirror Mirror
 * \section mirror_error_codes Mirror Error Codes
 *         - ::NPP_MIRROR_FLIP_ERR if flip has an illegal value.
 *
 * @{
 *
 */

/** @name Mirror
 *  Mirrors images horizontally, vertically and diagonally.
 *
 * @{
 *
 */

/**
 * 1 channel 8-bit unsigned image mirror.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oROI \ref roi_specification.
 * \param flip Specifies the axis about which the image is to be mirrored.
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref mirror_error_codes
 */
NppStatus 
nppiMirror_8u_C1R(const Npp8u * pSrc, int nSrcStep, 
                        Npp8u * pDst, int nDstStep, 
                  NppiSize oROI, NppiAxis flip);

/**
 * 1 channel 8-bit unsigned in place image mirror.
 *
 * \param pSrcDst \ref in_place_image_pointer.
 * \param nSrcDstStep \ref in_place_image_line_step.
 * \param oROI \ref roi_specification.
 * \param flip Specifies the axis about which the image is to be mirrored.
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref mirror_error_codes
 */
NppStatus 
nppiMirror_8u_C1IR(Npp8u * pSrcDst, int nSrcDstStep, 
                   NppiSize oROI, NppiAxis flip);
                              
/**
 * 3 channel 8-bit unsigned image mirror.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oROI \ref roi_specification.
 * \param flip Specifies the axis about which the image is to be mirrored.
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref mirror_error_codes
 */
NppStatus 
nppiMirror_8u_C3R(const Npp8u * pSrc, int nSrcStep, 
                        Npp8u * pDst, int nDstStep, 
                  NppiSize oROI, NppiAxis flip);

/**
 * 3 channel 8-bit unsigned in place image mirror.
 *
 * \param pSrcDst \ref in_place_image_pointer.
 * \param nSrcDstStep \ref in_place_image_line_step.
 * \param oROI \ref roi_specification.
 * \param flip Specifies the axis about which the image is to be mirrored.
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref mirror_error_codes
 */
NppStatus 
nppiMirror_8u_C3IR(Npp8u * pSrcDst, int nSrcDstStep, 
                   NppiSize oROI, NppiAxis flip);

/**
 * 4 channel 8-bit unsigned image mirror.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep Distance in bytes between starts of consecutive lines of the
 *        destination image.
 * \param oROI \ref roi_specification.
 * \param flip Specifies the axis about which the image is to be mirrored.
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref mirror_error_codes
 */
NppStatus 
nppiMirror_8u_C4R(const Npp8u * pSrc, int nSrcStep, 
                        Npp8u * pDst, int nDstStep, 
                  NppiSize oROI, NppiAxis flip);

/**
 * 4 channel 8-bit unsigned in place image mirror.
 *
 * \param pSrcDst \ref in_place_image_pointer.
 * \param nSrcDstStep \ref in_place_image_line_step.
 * \param oROI \ref roi_specification.
 * \param flip Specifies the axis about which the image is to be mirrored.
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref mirror_error_codes
 */
NppStatus 
nppiMirror_8u_C4IR(Npp8u * pSrcDst, int nSrcDstStep, 
                   NppiSize oROI, NppiAxis flip);

/**
 * 4 channel 8-bit unsigned image mirror not affecting alpha.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep Distance in bytes between starts of consecutive lines of the
 *        destination image.
 * \param oROI \ref roi_specification.
 * \param flip Specifies the axis about which the image is to be mirrored.
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref mirror_error_codes
 */
NppStatus 
nppiMirror_8u_AC4R(const Npp8u * pSrc, int nSrcStep, 
                         Npp8u * pDst, int nDstStep, 
                   NppiSize oROI, NppiAxis flip);

/**
 * 4 channel 8-bit unsigned in place image mirror not affecting alpha.
 *
 * \param pSrcDst \ref in_place_image_pointer.
 * \param nSrcDstStep \ref in_place_image_line_step.
 * \param oROI \ref roi_specification.
 * \param flip Specifies the axis about which the image is to be mirrored.
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref mirror_error_codes
 */
NppStatus 
nppiMirror_8u_AC4IR(Npp8u * pSrcDst, int nSrcDstStep, 
                    NppiSize oROI, NppiAxis flip);
/**
 * 1 channel 16-bit unsigned image mirror.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oROI \ref roi_specification.
 * \param flip Specifies the axis about which the image is to be mirrored.
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref mirror_error_codes
 */
NppStatus 
nppiMirror_16u_C1R(const Npp16u * pSrc, int nSrcStep, 
                         Npp16u * pDst, int nDstStep, 
                   NppiSize oROI, NppiAxis flip);
                              
/**
 * 1 channel 16-bit unsigned in place image mirror.
 *
 * \param pSrcDst \ref in_place_image_pointer.
 * \param nSrcDstStep \ref in_place_image_line_step.
 * \param oROI \ref roi_specification.
 * \param flip Specifies the axis about which the image is to be mirrored.
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref mirror_error_codes
 */
NppStatus 
nppiMirror_16u_C1IR(Npp16u * pSrcDst, int nSrcDstStep, 
                    NppiSize oROI, NppiAxis flip);

/**
 * 3 channel 16-bit unsigned image mirror.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oROI \ref roi_specification.
 * \param flip Specifies the axis about which the image is to be mirrored.
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref mirror_error_codes
 */
NppStatus 
nppiMirror_16u_C3R(const Npp16u * pSrc, int nSrcStep, 
                         Npp16u * pDst, int nDstStep, 
                   NppiSize oROI, NppiAxis flip);

/**
 * 3 channel 16-bit unsigned in place image mirror.
 *
 * \param pSrcDst \ref in_place_image_pointer.
 * \param nSrcDstStep \ref in_place_image_line_step.
 * \param oROI \ref roi_specification.
 * \param flip Specifies the axis about which the image is to be mirrored.
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref mirror_error_codes
 */
NppStatus 
nppiMirror_16u_C3IR(Npp16u * pSrcDst, int nSrcDstStep, 
                    NppiSize oROI, NppiAxis flip);

/**
 * 4 channel 16-bit unsigned image mirror.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep Distance in bytes between starts of consecutive lines of the
 *        destination image.
 * \param oROI \ref roi_specification.
 * \param flip Specifies the axis about which the image is to be mirrored.
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref mirror_error_codes
 */
NppStatus 
nppiMirror_16u_C4R(const Npp16u * pSrc, int nSrcStep, 
                         Npp16u * pDst, int nDstStep, 
                   NppiSize oROI, NppiAxis flip);

/**
 * 4 channel 16-bit unsigned in place image mirror.
 *
 * \param pSrcDst \ref in_place_image_pointer.
 * \param nSrcDstStep \ref in_place_image_line_step.
 * \param oROI \ref roi_specification.
 * \param flip Specifies the axis about which the image is to be mirrored.
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref mirror_error_codes
 */
NppStatus 
nppiMirror_16u_C4IR(Npp16u * pSrcDst, int nSrcDstStep, 
                    NppiSize oROI, NppiAxis flip);

/**
 * 4 channel 16-bit unsigned image mirror not affecting alpha.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep Distance in bytes between starts of consecutive lines of the
 *        destination image.
 * \param oROI \ref roi_specification.
 * \param flip Specifies the axis about which the image is to be mirrored.
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref mirror_error_codes
 */
NppStatus 
nppiMirror_16u_AC4R(const Npp16u * pSrc, int nSrcStep, 
                          Npp16u * pDst, int nDstStep, 
                    NppiSize oROI, NppiAxis flip);

/**
 * 4 channel 16-bit unsigned in place image mirror not affecting alpha.
 *
 * \param pSrcDst \ref in_place_image_pointer.
 * \param nSrcDstStep \ref in_place_image_line_step.
 * \param oROI \ref roi_specification.
 * \param flip Specifies the axis about which the image is to be mirrored.
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref mirror_error_codes
 */
NppStatus 
nppiMirror_16u_AC4IR(Npp16u * pSrcDst, int nSrcDstStep, 
                     NppiSize oROI, NppiAxis flip);

/**
 * 1 channel 16-bit signed image mirror.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oROI \ref roi_specification.
 * \param flip Specifies the axis about which the image is to be mirrored.
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref mirror_error_codes
 */
NppStatus 
nppiMirror_16s_C1R(const Npp16s * pSrc, int nSrcStep, 
                         Npp16s * pDst, int nDstStep, 
                   NppiSize oROI, NppiAxis flip);
                              
/**
 * 1 channel 16-bit signed in place image mirror.
 *
 * \param pSrcDst \ref in_place_image_pointer.
 * \param nSrcDstStep \ref in_place_image_line_step.
 * \param oROI \ref roi_specification.
 * \param flip Specifies the axis about which the image is to be mirrored.
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref mirror_error_codes
 */
NppStatus 
nppiMirror_16s_C1IR(Npp16s * pSrcDst, int nSrcDstStep, 
                    NppiSize oROI, NppiAxis flip);

/**
 * 3 channel 16-bit signed image mirror.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oROI \ref roi_specification.
 * \param flip Specifies the axis about which the image is to be mirrored.
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref mirror_error_codes
 */
NppStatus 
nppiMirror_16s_C3R(const Npp16s * pSrc, int nSrcStep, 
                         Npp16s * pDst, int nDstStep, 
                   NppiSize oROI, NppiAxis flip);

/**
 * 3 channel 16-bit signed in place image mirror.
 *
 * \param pSrcDst \ref in_place_image_pointer.
 * \param nSrcDstStep \ref in_place_image_line_step.
 * \param oROI \ref roi_specification.
 * \param flip Specifies the axis about which the image is to be mirrored.
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref mirror_error_codes
 */
NppStatus 
nppiMirror_16s_C3IR(Npp16s * pSrcDst, int nSrcDstStep, 
                    NppiSize oROI, NppiAxis flip);

/**
 * 4 channel 16-bit signed image mirror.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep Distance in bytes between starts of consecutive lines of the
 *        destination image.
 * \param oROI \ref roi_specification.
 * \param flip Specifies the axis about which the image is to be mirrored.
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref mirror_error_codes
 */
NppStatus 
nppiMirror_16s_C4R(const Npp16s * pSrc, int nSrcStep, 
                         Npp16s * pDst, int nDstStep, 
                   NppiSize oROI, NppiAxis flip);

/**
 * 4 channel 16-bit signed in place image mirror.
 *
 * \param pSrcDst \ref in_place_image_pointer.
 * \param nSrcDstStep \ref in_place_image_line_step.
 * \param oROI \ref roi_specification.
 * \param flip Specifies the axis about which the image is to be mirrored.
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref mirror_error_codes
 */
NppStatus 
nppiMirror_16s_C4IR(Npp16s * pSrcDst, int nSrcDstStep, 
                    NppiSize oROI, NppiAxis flip);

/**
 * 4 channel 16-bit signed image mirror not affecting alpha.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep Distance in bytes between starts of consecutive lines of the
 *        destination image.
 * \param oROI \ref roi_specification.
 * \param flip Specifies the axis about which the image is to be mirrored.
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref mirror_error_codes
 */
NppStatus 
nppiMirror_16s_AC4R(const Npp16s * pSrc, int nSrcStep, 
                          Npp16s * pDst, int nDstStep, 
                    NppiSize oROI, NppiAxis flip);

/**
 * 4 channel 16-bit signed in place image mirror not affecting alpha.
 *
 * \param pSrcDst \ref in_place_image_pointer.
 * \param nSrcDstStep \ref in_place_image_line_step.
 * \param oROI \ref roi_specification.
 * \param flip Specifies the axis about which the image is to be mirrored.
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref mirror_error_codes
 */
NppStatus 
nppiMirror_16s_AC4IR(Npp16s * pSrcDst, int nSrcDstStep, 
                     NppiSize oROI, NppiAxis flip);

/**
 * 1 channel 32-bit image mirror.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oROI \ref roi_specification.
 * \param flip Specifies the axis about which the image is to be mirrored.
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref mirror_error_codes
 */
NppStatus 
nppiMirror_32s_C1R(const Npp32s * pSrc, int nSrcStep, 
                         Npp32s * pDst, int nDstStep, 
                   NppiSize oROI, NppiAxis flip);
                              
/**
 * 1 channel 32-bit signed in place image mirror.
 *
 * \param pSrcDst \ref in_place_image_pointer.
 * \param nSrcDstStep \ref in_place_image_line_step.
 * \param oROI \ref roi_specification.
 * \param flip Specifies the axis about which the image is to be mirrored.
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref mirror_error_codes
 */
NppStatus 
nppiMirror_32s_C1IR(Npp32s * pSrcDst, int nSrcDstStep, 
                    NppiSize oROI, NppiAxis flip);

/**
 * 3 channel 32-bit image mirror.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oROI \ref roi_specification.
 * \param flip Specifies the axis about which the image is to be mirrored.
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref mirror_error_codes
 */
NppStatus 
nppiMirror_32s_C3R(const Npp32s * pSrc, int nSrcStep, 
                         Npp32s * pDst, int nDstStep, 
                   NppiSize oROI, NppiAxis flip);

/**
 * 3 channel 32-bit signed in place image mirror.
 *
 * \param pSrcDst \ref in_place_image_pointer.
 * \param nSrcDstStep \ref in_place_image_line_step.
 * \param oROI \ref roi_specification.
 * \param flip Specifies the axis about which the image is to be mirrored.
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref mirror_error_codes
 */
NppStatus 
nppiMirror_32s_C3IR(Npp32s * pSrcDst, int nSrcDstStep, 
                    NppiSize oROI, NppiAxis flip);

/**
 * 4 channel 32-bit image mirror.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep Distance in bytes between starts of consecutive lines of the
 *        destination image.
 * \param oROI \ref roi_specification.
 * \param flip Specifies the axis about which the image is to be mirrored.
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref mirror_error_codes
 */
NppStatus 
nppiMirror_32s_C4R(const Npp32s * pSrc, int nSrcStep, 
                         Npp32s * pDst, int nDstStep, 
                   NppiSize oROI, NppiAxis flip);

/**
 * 4 channel 32-bit signed in place image mirror.
 *
 * \param pSrcDst \ref in_place_image_pointer.
 * \param nSrcDstStep \ref in_place_image_line_step.
 * \param oROI \ref roi_specification.
 * \param flip Specifies the axis about which the image is to be mirrored.
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref mirror_error_codes
 */
NppStatus 
nppiMirror_32s_C4IR(Npp32s * pSrcDst, int nSrcDstStep, 
                    NppiSize oROI, NppiAxis flip);


/**
 * 4 channel 32-bit image mirror not affecting alpha.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep Distance in bytes between starts of consecutive lines of the
 *        destination image.
 * \param oROI \ref roi_specification.
 * \param flip Specifies the axis about which the image is to be mirrored.
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref mirror_error_codes
 */
NppStatus 
nppiMirror_32s_AC4R(const Npp32s * pSrc, int nSrcStep, 
                          Npp32s * pDst, int nDstStep, 
                    NppiSize oROI, NppiAxis flip);

/**
 * 4 channel 32-bit signed in place image mirror not affecting alpha.
 *
 * \param pSrcDst \ref in_place_image_pointer.
 * \param nSrcDstStep \ref in_place_image_line_step.
 * \param oROI \ref roi_specification.
 * \param flip Specifies the axis about which the image is to be mirrored.
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref mirror_error_codes
 */
NppStatus 
nppiMirror_32s_AC4IR(Npp32s * pSrcDst, int nSrcDstStep, 
                     NppiSize oROI, NppiAxis flip);


/**
 * 1 channel 32-bit float image mirror.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oROI \ref roi_specification.
 * \param flip Specifies the axis about which the image is to be mirrored.
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref mirror_error_codes
 */
NppStatus 
nppiMirror_32f_C1R(const Npp32f * pSrc, int nSrcStep, 
                         Npp32f * pDst, int nDstStep, 
                   NppiSize oROI, NppiAxis flip);
                              
/**
 * 1 channel 32-bit float in place image mirror.
 *
 * \param pSrcDst \ref in_place_image_pointer.
 * \param nSrcDstStep \ref in_place_image_line_step.
 * \param oROI \ref roi_specification.
 * \param flip Specifies the axis about which the image is to be mirrored.
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref mirror_error_codes
 */
NppStatus 
nppiMirror_32f_C1IR(Npp32f * pSrcDst, int nSrcDstStep, 
                    NppiSize oROI, NppiAxis flip);


/**
 * 3 channel 32-bit float image mirror.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oROI \ref roi_specification.
 * \param flip Specifies the axis about which the image is to be mirrored.
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref mirror_error_codes
 */
NppStatus 
nppiMirror_32f_C3R(const Npp32f * pSrc, int nSrcStep, 
                         Npp32f * pDst, int nDstStep, 
                   NppiSize oROI, NppiAxis flip);

/**
 * 3 channel 32-bit float in place image mirror.
 *
 * \param pSrcDst \ref in_place_image_pointer.
 * \param nSrcDstStep \ref in_place_image_line_step.
 * \param oROI \ref roi_specification.
 * \param flip Specifies the axis about which the image is to be mirrored.
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref mirror_error_codes
 */
NppStatus 
nppiMirror_32f_C3IR(Npp32f * pSrcDst, int nSrcDstStep, 
                    NppiSize oROI, NppiAxis flip);

/**
 * 4 channel 32-bit float image mirror.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep Distance in bytes between starts of consecutive lines of the
 *        destination image.
 * \param oROI \ref roi_specification.
 * \param flip Specifies the axis about which the image is to be mirrored.
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref mirror_error_codes
 */
NppStatus 
nppiMirror_32f_C4R(const Npp32f * pSrc, int nSrcStep, 
                         Npp32f * pDst, int nDstStep, 
                   NppiSize oROI, NppiAxis flip);

/**
 * 4 channel 32-bit float in place image mirror.
 *
 * \param pSrcDst \ref in_place_image_pointer.
 * \param nSrcDstStep \ref in_place_image_line_step.
 * \param oROI \ref roi_specification.
 * \param flip Specifies the axis about which the image is to be mirrored.
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref mirror_error_codes
 */
NppStatus 
nppiMirror_32f_C4IR(Npp32f * pSrcDst, int nSrcDstStep, 
                    NppiSize oROI, NppiAxis flip);

/**
 * 4 channel 32-bit float image mirror not affecting alpha.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep Distance in bytes between starts of consecutive lines of the
 *        destination image.
 * \param oROI \ref roi_specification.
 * \param flip Specifies the axis about which the image is to be mirrored.
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref mirror_error_codes
 */
NppStatus 
nppiMirror_32f_AC4R(const Npp32f * pSrc, int nSrcStep, 
                          Npp32f * pDst, int nDstStep, 
                    NppiSize oROI, NppiAxis flip);

/**
 * 4 channel 32-bit float in place image mirror not affecting alpha.
 *
 * \param pSrcDst \ref in_place_image_pointer.
 * \param nSrcDstStep \ref in_place_image_line_step.
 * \param oROI \ref roi_specification.
 * \param flip Specifies the axis about which the image is to be mirrored.
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref mirror_error_codes
 */
NppStatus 
nppiMirror_32f_AC4IR(Npp32f * pSrcDst, int nSrcDstStep, 
                     NppiSize oROI, NppiAxis flip);

/** @} */

/** @} image_mirror */

/** @defgroup image_affine_transform Affine Transforms
 *
 * \section affine_transform_error_codes Affine Transform Error Codes
 *
 *         - ::NPP_RECT_ERROR Indicates an error condition if width or height of
 *           the intersection of the oSrcROI and source image is less than or
 *           equal to 1
 *         - ::NPP_WRONG_INTERSECTION_ROI_ERROR Indicates an error condition if
 *           oSrcROI has no intersection with the source image
 *         - ::NPP_INTERPOLATION_ERROR Indicates an error condition if
 *           interpolation has an illegal value
 *         - ::NPP_COEFF_ERROR Indicates an error condition if coefficient values
 *           are invalid
 *         - ::NPP_WRONG_INTERSECTION_QUAD_WARNING Indicates a warning that no
 *           operation is performed if the transformed source ROI has no
 *           intersection with the destination ROI
 *
 * @{
 *
 */

/** @name Utility Functions
 *
 * @{
 *
 */

/**
 * Computes affine transform coefficients based on source ROI and destination quadrilateral.
 *
 * The function computes the coefficients of an affine transformation that maps the
 * given source ROI (axis aligned rectangle with integer coordinates) to a quadrilateral
 * in the destination image.
 *
 * An affine transform in 2D is fully determined by the mapping of just three vertices.
 * This function's API allows for passing a complete quadrilateral effectively making the 
 * prolem overdetermined. What this means in practice is, that for certain quadrilaterals it is
 * not possible to find an affine transform that would map all four corners of the source
 * ROI to the four vertices of that quadrilateral.
 *
 * The function circumvents this problem by only looking at the first three vertices of
 * the destination image quadrilateral to determine the affine transformation's coefficients.
 * If the destination quadrilateral is indeed one that cannot be mapped using an affine
 * transformation the functions informs the user of this situation by returning a 
 * ::NPP_AFFINE_QUAD_INCORRECT_WARNING.
 *
 * \param oSrcROI The source ROI. This rectangle needs to be at least one pixel wide and
 *          high. If either width or hight are less than one an ::NPP_RECT_ERROR is returned.
 * \param aQuad The destination quadrilateral.
 * \param aCoeffs The resulting affine transform coefficients.
 * \return Error codes:
 *         - ::NPP_SIZE_ERROR Indicates an error condition if any image dimension
 *           has zero or negative value
 *         - ::NPP_RECT_ERROR Indicates an error condition if width or height of
 *           the intersection of the oSrcROI and source image is less than or
 *           equal to 1
 *         - ::NPP_COEFF_ERROR Indicates an error condition if coefficient values
 *           are invalid
 *         - ::NPP_AFFINE_QUAD_INCORRECT_WARNING Indicates a warning when quad
 *           does not conform to the transform properties. Fourth vertex is
 *           ignored, internally computed coordinates are used instead
 */
NppStatus 
nppiGetAffineTransform(NppiRect oSrcROI, const double aQuad[4][2], double aCoeffs[2][3]);


/**
 * Compute shape of transformed image.
 *
 * This method computes the quadrilateral in the destination image that 
 * the source ROI is transformed into by the affine transformation expressed
 * by the coefficients array (aCoeffs).
 *
 * \param oSrcROI The source ROI.
 * \param aQuad The resulting destination quadrangle.
 * \param aCoeffs The afine transform coefficients.
 * \return Error codes:
 *         - ::NPP_SIZE_ERROR Indicates an error condition if any image dimension
 *           has zero or negative value
 *         - ::NPP_RECT_ERROR Indicates an error condition if width or height of
 *           the intersection of the oSrcROI and source image is less than or
 *           equal to 1
 *         - ::NPP_COEFF_ERROR Indicates an error condition if coefficient values
 *           are invalid
 */
NppStatus 
nppiGetAffineQuad(NppiRect oSrcROI, double aQuad[4][2], const double aCoeffs[2][3]);


/**
 * Compute bounding-box of transformed image.
 *
 * The method effectively computes the bounding box (axis aligned rectangle) of
 * the transformed source ROI (see nppiGetAffineQuad()). 
 *
 * \param oSrcROI The source ROI.
 * \param aBound The resulting bounding box.
 * \param aCoeffs The afine transform coefficients.
 * \return Error codes:
 *         - ::NPP_SIZE_ERROR Indicates an error condition if any image dimension
 *           has zero or negative value
 *         - ::NPP_RECT_ERROR Indicates an error condition if width or height of
 *           the intersection of the oSrcROI and source image is less than or
 *           equal to 1
 *         - ::NPP_COEFF_ERROR Indicates an error condition if coefficient values
 *           are invalid
 */
NppStatus 
nppiGetAffineBound(NppiRect oSrcROI, double aBound[2][2], const double aCoeffs[2][3]);

/** @} Utility Functions Section */

/** @name Affine Transform
 * Transforms (warps) an image based on an affine transform. The affine
 * transform is given as a \f$2\times 3\f$ matrix C. A pixel location \f$(x, y)\f$ in the
 * source image is mapped to the location \f$(x', y')\f$ in the destination image.
 * The destination image coorodinates are computed as follows:
 * \f[
 * x' = c_{00} * x + c_{01} * y + c_{02} \qquad
 * y' = c_{10} * x + c_{11} * y + c_{12} \qquad
 * C = \left[ \matrix{c_{00} & c_{01} & c_{02} \cr 
                      c_{10} & c_{11} & c_{12} } \right]
 * \f]
 * Affine transforms can be understood as a linear transformation (traditional
 * matrix multiplication) and a shift operation. The \f$2\times 2\f$ matrix 
 * \f[
 *    L = \left[ \matrix{c_{00} & c_{01} \cr 
 *                       c_{10} & c_{11} } \right]
 * \f]
 * represents the linear transform portion of the affine transformation. The
 * vector
 * \f[
 *      v = \left( \matrix{c_{02} \cr
                           c_{12} } \right)
 * \f]
 * represents the post-transform shift, i.e. after the pixel location is transformed
 * by \f$L\f$ it is translated by \f$v\f$.
 * 
 * @{
 *
 */


/**
 * Single-channel 8-bit unsigned affine warp.
 * 
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aCoeffs Affine transform coefficients
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref affine_transform_error_codes
 */
NppStatus 
nppiWarpAffine_8u_C1R(const Npp8u * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI,
                            Npp8u * pDst, int nDstStep, NppiRect oDstROI, 
                      const double aCoeffs[2][3], int eInterpolation);

/**
 * Three-channel 8-bit unsigned affine warp.
 * 
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aCoeffs Affine transform coefficients
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref affine_transform_error_codes
 */
NppStatus 
nppiWarpAffine_8u_C3R(const Npp8u * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                            Npp8u * pDst, int nDstStep, NppiRect oDstROI, 
                      const double aCoeffs[2][3], int eInterpolation);


/**
 * Four-channel 8-bit unsigned affine warp.
 * 
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aCoeffs Affine transform coefficients
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref affine_transform_error_codes
 */
NppStatus 
nppiWarpAffine_8u_C4R(const Npp8u * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                            Npp8u * pDst, int nDstStep, NppiRect oDstROI, 
                      const double aCoeffs[2][3], int eInterpolation);


/**
 * Four-channel 8-bit unsigned affine warp, ignoring alpha channel.
 * 
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aCoeffs Affine transform coefficients
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref affine_transform_error_codes
 */
NppStatus 
nppiWarpAffine_8u_AC4R(const Npp8u * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                             Npp8u * pDst, int nDstStep, NppiRect oDstROI, 
                       const double aCoeffs[2][3], int eInterpolation);


/**
 * Three-channel planar 8-bit unsigned affine warp.
 * 
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aCoeffs Affine transform coefficients
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref affine_transform_error_codes
 */
NppStatus 
nppiWarpAffine_8u_P3R(const Npp8u * pSrc[3], NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                            Npp8u * pDst[3], int nDstStep, NppiRect oDstROI, 
                      const double aCoeffs[2][3], int eInterpolation);

/**
 * Four-channel planar 8-bit unsigned affine warp.
 * 
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aCoeffs Affine transform coefficients
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref affine_transform_error_codes
 */
NppStatus 
nppiWarpAffine_8u_P4R(const Npp8u * pSrc[4], NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                            Npp8u * pDst[4], int nDstStep, NppiRect oDstROI, 
                      const double aCoeffs[2][3], int eInterpolation);

/**
 * Single-channel 16-bit unsigned affine warp.
 * 
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aCoeffs Affine transform coefficients
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref affine_transform_error_codes
 */
NppStatus 
nppiWarpAffine_16u_C1R(const Npp16u * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI,
                             Npp16u * pDst, int nDstStep, NppiRect oDstROI, 
                       const double aCoeffs[2][3], int eInterpolation);

/**
 * Three-channel 16-bit unsigned affine warp.
 * 
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aCoeffs Affine transform coefficients
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref affine_transform_error_codes
 */
NppStatus 
nppiWarpAffine_16u_C3R(const Npp16u * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                             Npp16u * pDst, int nDstStep, NppiRect oDstROI, 
                       const double aCoeffs[2][3], int eInterpolation);

/**
 * Four-channel 16-bit unsigned affine warp.
 * 
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aCoeffs Affine transform coefficients
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref affine_transform_error_codes
 */
NppStatus 
nppiWarpAffine_16u_C4R(const Npp16u * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                             Npp16u * pDst, int nDstStep, NppiRect oDstROI, 
                       const double aCoeffs[2][3], int eInterpolation);

/**
 * Four-channel 16-bit unsigned affine warp, ignoring alpha channel.
 * 
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aCoeffs Affine transform coefficients
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref affine_transform_error_codes
 */
NppStatus 
nppiWarpAffine_16u_AC4R(const Npp16u * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                              Npp16u * pDst, int nDstStep, NppiRect oDstROI, 
                        const double aCoeffs[2][3], int eInterpolation);

/**
 * Three-channel planar 16-bit unsigned affine warp.
 * 
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aCoeffs Affine transform coefficients
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref affine_transform_error_codes
 */
NppStatus 
nppiWarpAffine_16u_P3R(const Npp16u * pSrc[3], NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                             Npp16u * pDst[3], int nDstStep, NppiRect oDstROI, 
                       const double aCoeffs[2][3], int eInterpolation);

/**
 * Four-channel planar 16-bit unsigned affine warp.
 * 
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aCoeffs Affine transform coefficients
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref affine_transform_error_codes
 */
NppStatus 
nppiWarpAffine_16u_P4R(const Npp16u * pSrc[4], NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                             Npp16u * pDst[4], int nDstStep, NppiRect oDstROI, 
                       const double aCoeffs[2][3], int eInterpolation);

/**
 * Single-channel 32-bit signed affine warp.
 * 
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aCoeffs Affine transform coefficients
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref affine_transform_error_codes
 */
NppStatus 
nppiWarpAffine_32s_C1R(const Npp32s * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                             Npp32s * pDst, int nDstStep, NppiRect oDstROI, 
                       const double aCoeffs[2][3], int eInterpolation);

/**
 * Three-channel 32-bit signed affine warp.
 * 
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aCoeffs Affine transform coefficients
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref affine_transform_error_codes
 */
NppStatus 
nppiWarpAffine_32s_C3R(const Npp32s * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                             Npp32s * pDst, int nDstStep, NppiRect oDstROI, 
                       const double aCoeffs[2][3], int eInterpolation);

/**
 * Four-channel 32-bit signed affine warp.
 * 
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aCoeffs Affine transform coefficients
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref affine_transform_error_codes
 */
NppStatus 
nppiWarpAffine_32s_C4R(const Npp32s * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                             Npp32s * pDst, int nDstStep, NppiRect oDstROI, 
                       const double aCoeffs[2][3], int eInterpolation);

/**
 * Four-channel 32-bit signed affine warp, ignoring alpha channel.
 * 
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aCoeffs Affine transform coefficients
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref affine_transform_error_codes
 */
NppStatus 
nppiWarpAffine_32s_AC4R(const Npp32s * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                              Npp32s * pDst, int nDstStep, NppiRect oDstROI, 
                        const double aCoeffs[2][3], int eInterpolation);

/**
 * Three-channel planar 32-bit signed affine warp.
 * 
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aCoeffs Affine transform coefficients
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref affine_transform_error_codes
 */
NppStatus 
nppiWarpAffine_32s_P3R(const Npp32s * pSrc[3], NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                             Npp32s * pDst[3], int nDstStep, NppiRect oDstROI, 
                       const double aCoeffs[2][3], int eInterpolation);

/**
 * Four-channel planar 32-bit signed affine warp.
 * 
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aCoeffs Affine transform coefficients
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref affine_transform_error_codes
 */
NppStatus 
nppiWarpAffine_32s_P4R(const Npp32s * pSrc[4], NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                             Npp32s * pDst[4], int nDstStep, NppiRect oDstROI, 
                       const double aCoeffs[2][3], int eInterpolation);

/**
 * Single-channel 32-bit floating-point affine warp.
 * 
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aCoeffs Affine transform coefficients
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref affine_transform_error_codes
 */
NppStatus 
nppiWarpAffine_32f_C1R(const Npp32f * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                             Npp32f * pDst, int nDstStep, NppiRect oDstROI, 
                       const double aCoeffs[2][3], int eInterpolation);

/**
 * Three-channel 32-bit floating-point affine warp.
 * 
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aCoeffs Affine transform coefficients
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref affine_transform_error_codes
 */
NppStatus 
nppiWarpAffine_32f_C3R(const Npp32f * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                             Npp32f * pDst, int nDstStep, NppiRect oDstROI, 
                       const double aCoeffs[2][3], int eInterpolation);

/**
 * Four-channel 32-bit floating-point affine warp.
 * 
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aCoeffs Affine transform coefficients
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref affine_transform_error_codes
 */
NppStatus 
nppiWarpAffine_32f_C4R(const Npp32f * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                             Npp32f * pDst, int nDstStep, NppiRect oDstROI, 
                       const double aCoeffs[2][3], int eInterpolation);

/**
 * Four-channel 32-bit floating-point affine warp, ignoring alpha channel.
 * 
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aCoeffs Affine transform coefficients
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref affine_transform_error_codes
 */
NppStatus 
nppiWarpAffine_32f_AC4R(const Npp32f * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                              Npp32f * pDst, int nDstStep, NppiRect oDstROI, 
                        const double aCoeffs[2][3], int eInterpolation);

/**
 * Three-channel planar 32-bit floating-point affine warp.
 * 
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aCoeffs Affine transform coefficients
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref affine_transform_error_codes
 */
NppStatus 
nppiWarpAffine_32f_P3R(const Npp32f * pSrc[3], NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                             Npp32f * pDst[3], int nDstStep, NppiRect oDstROI, 
                       const double aCoeffs[2][3], int eInterpolation);

/**
 * Four-channel planar 32-bit floating-point affine warp.
 * 
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aCoeffs Affine transform coefficients
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref affine_transform_error_codes
 */
NppStatus 
nppiWarpAffine_32f_P4R(const Npp32f * pSrc[4], NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI,
                             Npp32f * pDst[4], int nDstStep, NppiRect oDstROI, 
                       const double aCoeffs[2][3], int eInterpolation);


/**
 * Single-channel 64-bit floating-point affine warp.
 * 
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aCoeffs Affine transform coefficients
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref affine_transform_error_codes
 */
NppStatus 
nppiWarpAffine_64f_C1R(const Npp64f * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                             Npp64f * pDst, int nDstStep, NppiRect oDstROI, 
                       const double aCoeffs[2][3], int eInterpolation);

/**
 * Three-channel 64-bit floating-point affine warp.
 * 
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aCoeffs Affine transform coefficients
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref affine_transform_error_codes
 */
NppStatus 
nppiWarpAffine_64f_C3R(const Npp64f * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                             Npp64f * pDst, int nDstStep, NppiRect oDstROI, 
                       const double aCoeffs[2][3], int eInterpolation);

/**
 * Four-channel 64-bit floating-point affine warp.
 * 
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aCoeffs Affine transform coefficients
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref affine_transform_error_codes
 */
NppStatus 
nppiWarpAffine_64f_C4R(const Npp64f * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                             Npp64f * pDst, int nDstStep, NppiRect oDstROI, 
                       const double aCoeffs[2][3], int eInterpolation);

/**
 * Four-channel 64-bit floating-point affine warp, ignoring alpha channel.
 * 
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aCoeffs Affine transform coefficients
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref affine_transform_error_codes
 */
NppStatus 
nppiWarpAffine_64f_AC4R(const Npp64f * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                              Npp64f * pDst, int nDstStep, NppiRect oDstROI, 
                        const double aCoeffs[2][3], int eInterpolation);

/**
 * Three-channel planar 64-bit floating-point affine warp.
 * 
 * \param aSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param aDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aCoeffs Affine transform coefficients
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref affine_transform_error_codes
 */
NppStatus 
nppiWarpAffine_64f_P3R(const Npp64f * aSrc[3], NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI,
                             Npp64f * aDst[3], int nDstStep, NppiRect oDstROI, 
                       const double aCoeffs[2][3], int eInterpolation);

/**
 * Four-channel planar 64-bit floating-point affine warp.
 * 
 * \param aSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param aDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aCoeffs Affine transform coefficients
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref affine_transform_error_codes
 */
NppStatus 
nppiWarpAffine_64f_P4R(const Npp64f * aSrc[4], NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI,
                             Npp64f * aDst[4], int nDstStep, NppiRect oDstROI, 
                       const double aCoeffs[2][3], int eInterpolation);


/** @} Affine Transform Section */

/** @name Backwards Affine Transform
 * Transforms (warps) an image based on an affine transform. The affine
 * transform is given as a \f$2\times 3\f$ matrix C. A pixel location \f$(x, y)\f$ in the
 * source image is mapped to the location \f$(x', y')\f$ in the destination image.
 * The destination image coorodinates fullfil the following properties:
 * \f[
 * x = c_{00} * x' + c_{01} * y' + c_{02} \qquad
 * y = c_{10} * x' + c_{11} * y' + c_{12} \qquad
 * C = \left[ \matrix{c_{00} & c_{01} & c_{02} \cr 
                      c_{10} & c_{11} & c_{12} } \right]
 * \f]
 * In other words, given matrix \f$C\f$ the source image's shape is transfored to the destination image
 * using the inverse matrix \f$C^{-1}\f$:
 * \f[
 * M = C^{-1} = \left[ \matrix{m_{00} & m_{01} & m_{02} \cr 
                               m_{10} & m_{11} & m_{12} } \right]
 * x' = m_{00} * x + m_{01} * y + m_{02} \qquad
 * y' = m_{10} * x + m_{11} * y + m_{12} \qquad
 * \f]
 *
 * @{
 *
 */

/**
 * Single-channel 8-bit unsigned integer backwards affine warp.
 * 
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aCoeffs Affine transform coefficients
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref affine_transform_error_codes
 */
NppStatus 
nppiWarpAffineBack_8u_C1R(const Npp8u * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                                Npp8u * pDst, int nDstStep, NppiRect oDstROI, 
                          const double aCoeffs[2][3], int eInterpolation);

/**
 * Three-channel 8-bit unsigned integer backwards affine warp.
 * 
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aCoeffs Affine transform coefficients
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref affine_transform_error_codes
 */
NppStatus 
nppiWarpAffineBack_8u_C3R(const Npp8u * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                                Npp8u * pDst, int nDstStep, NppiRect oDstROI, 
                          const double aCoeffs[2][3], int eInterpolation);

/**
 * Four-channel 8-bit unsigned integer backwards affine warp.
 * 
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aCoeffs Affine transform coefficients
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref affine_transform_error_codes
 */
NppStatus 
nppiWarpAffineBack_8u_C4R(const Npp8u * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                                Npp8u * pDst, int nDstStep, NppiRect oDstROI, 
                          const double aCoeffs[2][3], int eInterpolation);

/**
 * Four-channel 8-bit unsigned integer backwards affine warp, ignoring alpha channel.
 * 
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aCoeffs Affine transform coefficients
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref affine_transform_error_codes
 */
NppStatus 
nppiWarpAffineBack_8u_AC4R(const Npp8u * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                                 Npp8u * pDst, int nDstStep, NppiRect oDstROI, 
                           const double aCoeffs[2][3], int eInterpolation);

/**
 * Three-channel planar 8-bit unsigned integer backwards affine warp.
 * 
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aCoeffs Affine transform coefficients
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref affine_transform_error_codes
 */
NppStatus 
nppiWarpAffineBack_8u_P3R(const Npp8u * pSrc[3], NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                                Npp8u * pDst[3], int nDstStep, NppiRect oDstROI, 
                          const double aCoeffs[2][3], int eInterpolation);

/**
 * Four-channel planar 8-bit unsigned integer backwards affine warp.
 * 
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aCoeffs Affine transform coefficients
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref affine_transform_error_codes
 */
NppStatus 
nppiWarpAffineBack_8u_P4R(const Npp8u * pSrc[4], NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                                Npp8u * pDst[4], int nDstStep, NppiRect oDstROI, 
                          const double aCoeffs[2][3], int eInterpolation);

/**
 * Single-channel 16-bit unsigned integer backwards affine warp.
 * 
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aCoeffs Affine transform coefficients
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref affine_transform_error_codes
 */
NppStatus 
nppiWarpAffineBack_16u_C1R(const Npp16u * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                                 Npp16u * pDst, int nDstStep, NppiRect oDstROI, 
                           const double aCoeffs[2][3], int eInterpolation);

/**
 * Three-channel 16-bit unsigned integer backwards affine warp.
 * 
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aCoeffs Affine transform coefficients
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref affine_transform_error_codes
 */
NppStatus 
nppiWarpAffineBack_16u_C3R(const Npp16u * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                                 Npp16u * pDst, int nDstStep, NppiRect oDstROI, 
                           const double aCoeffs[2][3], int eInterpolation);

/**
 * Four-channel 16-bit unsigned integer backwards affine warp.
 * 
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aCoeffs Affine transform coefficients
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref affine_transform_error_codes
 */
NppStatus 
nppiWarpAffineBack_16u_C4R(const Npp16u * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                                 Npp16u * pDst, int nDstStep, NppiRect oDstROI, 
                           const double aCoeffs[2][3], int eInterpolation);

/**
 * Four-channel 16-bit unsigned integer backwards affine warp, ignoring alpha channel.
 * 
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aCoeffs Affine transform coefficients
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref affine_transform_error_codes
 */
NppStatus 
nppiWarpAffineBack_16u_AC4R(const Npp16u * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                                  Npp16u * pDst, int nDstStep, NppiRect oDstROI, 
                            const double aCoeffs[2][3], int eInterpolation);

/**
 * Three-channel planar 16-bit unsigned integer backwards affine warp.
 * 
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aCoeffs Affine transform coefficients
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref affine_transform_error_codes
 */
NppStatus 
nppiWarpAffineBack_16u_P3R(const Npp16u * pSrc[3], NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                                 Npp16u * pDst[3], int nDstStep, NppiRect oDstROI, 
                           const double aCoeffs[2][3], int eInterpolation);

/**
 * Four-channel planar 16-bit unsigned integer backwards affine warp.
 * 
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aCoeffs Affine transform coefficients
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref affine_transform_error_codes
 */
NppStatus 
nppiWarpAffineBack_16u_P4R(const Npp16u * pSrc[4], NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                                 Npp16u * pDst[4], int nDstStep, NppiRect oDstROI, 
                           const double aCoeffs[2][3], int eInterpolation);

/**
 * Single-channel 32-bit signed integer backwards affine warp.
 * 
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aCoeffs Affine transform coefficients
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref affine_transform_error_codes
 */
NppStatus 
nppiWarpAffineBack_32s_C1R(const Npp32s * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                                 Npp32s * pDst, int nDstStep, NppiRect oDstROI, 
                           const double aCoeffs[2][3], int eInterpolation);

/**
 * Three-channel 32-bit signed integer backwards affine warp.
 * 
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aCoeffs Affine transform coefficients
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref affine_transform_error_codes
 */
NppStatus 
nppiWarpAffineBack_32s_C3R(const Npp32s * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                                 Npp32s * pDst, int nDstStep, NppiRect oDstROI, 
                           const double aCoeffs[2][3], int eInterpolation);

/**
 * Four-channel 32-bit signed integer backwards affine warp.
 * 
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aCoeffs Affine transform coefficients
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref affine_transform_error_codes
 */
NppStatus 
nppiWarpAffineBack_32s_C4R(const Npp32s * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                                 Npp32s * pDst, int nDstStep, NppiRect oDstROI, 
                           const double aCoeffs[2][3], int eInterpolation);

/**
 * Four-channel 32-bit signed integer backwards affine warp, ignoring alpha channel.
 * 
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aCoeffs Affine transform coefficients
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref affine_transform_error_codes
 */
NppStatus 
nppiWarpAffineBack_32s_AC4R(const Npp32s * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                                  Npp32s * pDst, int nDstStep, NppiRect oDstROI, 
                            const double aCoeffs[2][3], int eInterpolation);

/**
 * Three-channel planar 32-bit signed integer backwards affine warp.
 * 
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aCoeffs Affine transform coefficients
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref affine_transform_error_codes
 */
NppStatus 
nppiWarpAffineBack_32s_P3R(const Npp32s * pSrc[3], NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                                 Npp32s * pDst[3], int nDstStep, NppiRect oDstROI, 
                           const double aCoeffs[2][3], int eInterpolation);

/**
 * Four-channel planar 32-bit signed integer backwards affine warp.
 * 
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aCoeffs Affine transform coefficients
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref affine_transform_error_codes
 */
NppStatus 
nppiWarpAffineBack_32s_P4R(const Npp32s * pSrc[4], NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                                 Npp32s * pDst[4], int nDstStep, NppiRect oDstROI, 
                           const double aCoeffs[2][3], int eInterpolation);

/**
 * Single-channel 32-bit floating-point backwards affine warp.
 * 
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aCoeffs Affine transform coefficients
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref affine_transform_error_codes
 */
NppStatus 
nppiWarpAffineBack_32f_C1R(const Npp32f * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                                 Npp32f * pDst, int nDstStep, NppiRect oDstROI, 
                           const double aCoeffs[2][3], int eInterpolation);

/**
 * Three-channel 32-bit floating-point backwards affine warp.
 * 
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aCoeffs Affine transform coefficients
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref affine_transform_error_codes
 */
NppStatus 
nppiWarpAffineBack_32f_C3R(const Npp32f * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                                 Npp32f * pDst, int nDstStep, NppiRect oDstROI, 
                           const double aCoeffs[2][3], int eInterpolation);

/**
 * Four-channel 32-bit floating-point backwards affine warp.
 * 
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aCoeffs Affine transform coefficients
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref affine_transform_error_codes
 */
NppStatus 
nppiWarpAffineBack_32f_C4R(const Npp32f * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                                 Npp32f * pDst, int nDstStep, NppiRect oDstROI, 
                           const double aCoeffs[2][3], int eInterpolation);

/**
 * Four-channel 32-bit floating-point backwards affine warp, ignoring alpha channel.
 * 
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aCoeffs Affine transform coefficients
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref affine_transform_error_codes
 */
NppStatus 
nppiWarpAffineBack_32f_AC4R(const Npp32f * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                                  Npp32f * pDst, int nDstStep, NppiRect oDstROI, 
                            const double aCoeffs[2][3], int eInterpolation);

/**
 * Three-channel planar 32-bit floating-point backwards affine warp.
 * 
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aCoeffs Affine transform coefficients
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref affine_transform_error_codes
 */
NppStatus 
nppiWarpAffineBack_32f_P3R(const Npp32f * pSrc[3], NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                                 Npp32f * pDst[3], int nDstStep, NppiRect oDstROI, 
                           const double aCoeffs[2][3], int eInterpolation);

/**
 * Four-channel planar 32-bit floating-point backwards affine warp.
 * 
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aCoeffs Affine transform coefficients
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref affine_transform_error_codes
 */
NppStatus 
nppiWarpAffineBack_32f_P4R(const Npp32f * pSrc[4], NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                                 Npp32f * pDst[4], int nDstStep, NppiRect oDstROI, 
                           const double aCoeffs[2][3], int eInterpolation);


/** @} Backwards Affine Transform Section */

/** @name Quad-Based Affine Transform
 * Transforms (warps) an image based on an affine transform. The affine
 * transform is computed such that it maps a quadrilateral in source image space to a 
 * quadrilateral in destination image space. 
 *
 * An affine transform is fully determined by the mapping of 3 discrete points.
 * The following primitives compute an affine transformation matrix that maps 
 * the first three corners of the source quad are mapped to the first three 
 * vertices of the destination image quad. If the fourth vertices do not match
 * the transform, an ::NPP_AFFINE_QUAD_INCORRECT_WARNING is returned by the primitive.
 *
 *
 * @{
 *
 */

/**
 * Single-channel 32-bit floating-point quad-based affine warp.
 * 
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param aSrcQuad Source quad.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aDstQuad Destination quad.
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref affine_transform_error_codes
 */
NppStatus 
nppiWarpAffineQuad_8u_C1R(const Npp8u * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, const double aSrcQuad[4][2], 
                                Npp8u * pDst,                    int nDstStep, NppiRect oDstROI, const double aDstQuad[4][2], 
                          int eInterpolation);


/**
 * Three-channel 8-bit unsigned integer quad-based affine warp.
 * 
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param aSrcQuad Source quad.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aDstQuad Destination quad.
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref affine_transform_error_codes
 */
NppStatus 
nppiWarpAffineQuad_8u_C3R(const Npp8u * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, const double aSrcQuad[4][2], 
                                Npp8u * pDst,                    int nDstStep, NppiRect oDstROI, const double aDstQuad[4][2], 
                          int eInterpolation);

/**
 * Four-channel 8-bit unsigned integer quad-based affine warp.
 * 
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param aSrcQuad Source quad.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aDstQuad Destination quad.
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref affine_transform_error_codes
 */
NppStatus 
nppiWarpAffineQuad_8u_C4R(const Npp8u * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, const double aSrcQuad[4][2], 
                                Npp8u * pDst,                    int nDstStep, NppiRect oDstROI, const double aDstQuad[4][2], 
                          int eInterpolation);

/**
 * Four-channel 8-bit unsigned integer quad-based affine warp, ignoring alpha channel.
 * 
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param aSrcQuad Source quad.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aDstQuad Destination quad.
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref affine_transform_error_codes
 */
NppStatus 
nppiWarpAffineQuad_8u_AC4R(const Npp8u * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, const double aSrcQuad[4][2], 
                                 Npp8u * pDst,                    int nDstStep, NppiRect oDstROI, const double aDstQuad[4][2], 
                           int eInterpolation);

/**
 * Three-channel planar 8-bit unsigned integer quad-based affine warp.
 * 
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param aSrcQuad Source quad.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aDstQuad Destination quad.
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref affine_transform_error_codes
 */
NppStatus 
nppiWarpAffineQuad_8u_P3R(const Npp8u * pSrc[3], NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, const double aSrcQuad[4][2], 
                                Npp8u * pDst[3],                    int nDstStep, NppiRect oDstROI, const double aDstQuad[4][2], 
                          int eInterpolation);

/**
 * Four-channel planar 8-bit unsigned integer quad-based affine warp.
 * 
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param aSrcQuad Source quad.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aDstQuad Destination quad.
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref affine_transform_error_codes
 */
NppStatus 
nppiWarpAffineQuad_8u_P4R(const Npp8u * pSrc[4], NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, const double aSrcQuad[4][2], 
                                Npp8u * pDst[4],                    int nDstStep, NppiRect oDstROI, const double aDstQuad[4][2], 
                          int eInterpolation);

/**
 * Single-channel 16-bit unsigned integer quad-based affine warp.
 * 
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param aSrcQuad Source quad.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aDstQuad Destination quad.
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref affine_transform_error_codes
 */
NppStatus 
nppiWarpAffineQuad_16u_C1R(const Npp16u * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, const double aSrcQuad[4][2], 
                                 Npp16u * pDst, int nDstStep, NppiRect oDstROI,                    const double aDstQuad[4][2], 
                           int eInterpolation);

/**
 * Three-channel 16-bit unsigned integer quad-based affine warp.
 * 
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param aSrcQuad Source quad.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aDstQuad Destination quad.
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref affine_transform_error_codes
 */
NppStatus 
nppiWarpAffineQuad_16u_C3R(const Npp16u * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, const double aSrcQuad[4][2], 
                                 Npp16u * pDst, int nDstStep, NppiRect oDstROI, const double aDstQuad[4][2], 
                           int eInterpolation);

/**
 * Four-channel 16-bit unsigned integer quad-based affine warp.
 * 
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param aSrcQuad Source quad.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aDstQuad Destination quad.
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref affine_transform_error_codes
 */
NppStatus 
nppiWarpAffineQuad_16u_C4R(const Npp16u * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, const double aSrcQuad[4][2], 
                                 Npp16u * pDst, int nDstStep, NppiRect oDstROI, const double aDstQuad[4][2], 
                           int eInterpolation);

/**
 * Four-channel 16-bit unsigned integer quad-based affine warp, ignoring alpha channel.
 * 
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param aSrcQuad Source quad.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aDstQuad Destination quad.
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref affine_transform_error_codes
 */
NppStatus 
nppiWarpAffineQuad_16u_AC4R(const Npp16u * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, const double aSrcQuad[4][2], 
                                  Npp16u * pDst, int nDstStep, NppiRect oDstROI, const double aDstQuad[4][2], 
                            int eInterpolation);

/**
 * Three-channel planar 16-bit unsigned integer quad-based affine warp.
 * 
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param aSrcQuad Source quad.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aDstQuad Destination quad.
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref affine_transform_error_codes
 */
NppStatus 
nppiWarpAffineQuad_16u_P3R(const Npp16u * pSrc[3], NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, const double aSrcQuad[4][2], 
                                 Npp16u * pDst[3], int nDstStep, NppiRect oDstROI, const double aDstQuad[4][2], 
                           int eInterpolation);

/**
 * Four-channel planar 16-bit unsigned integer quad-based affine warp.
 * 
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param aSrcQuad Source quad.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aDstQuad Destination quad.
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref affine_transform_error_codes
 */
NppStatus 
nppiWarpAffineQuad_16u_P4R(const Npp16u * pSrc[4], NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, const double aSrcQuad[4][2], 
                                 Npp16u * pDst[4], int nDstStep, NppiRect oDstROI, const double aDstQuad[4][2], 
                           int eInterpolation);

/**
 * Single-channel 32-bit signed integer quad-based affine warp.
 * 
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param aSrcQuad Source quad.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aDstQuad Destination quad.
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref affine_transform_error_codes
 */
NppStatus 
nppiWarpAffineQuad_32s_C1R(const Npp32s * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, const double aSrcQuad[4][2], 
                                 Npp32s * pDst, int nDstStep, NppiRect oDstROI, const double aDstQuad[4][2], 
                           int eInterpolation);

/**
 * Three-channel 32-bit signed integer quad-based affine warp.
 * 
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param aSrcQuad Source quad.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aDstQuad Destination quad.
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref affine_transform_error_codes
 */
NppStatus 
nppiWarpAffineQuad_32s_C3R(const Npp32s * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, const double aSrcQuad[4][2], 
                                 Npp32s * pDst, int nDstStep, NppiRect oDstROI, const double aDstQuad[4][2], 
                           int eInterpolation);

/**
 * Four-channel 32-bit signed integer quad-based affine warp.
 * 
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param aSrcQuad Source quad.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aDstQuad Destination quad.
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref affine_transform_error_codes
 */
NppStatus 
nppiWarpAffineQuad_32s_C4R(const Npp32s * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, const double aSrcQuad[4][2], 
                                 Npp32s * pDst, int nDstStep, NppiRect oDstROI, const double aDstQuad[4][2], 
                           int eInterpolation);

/**
 * Four-channel 32-bit signed integer quad-based affine warp, ignoring alpha channel.
 * 
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param aSrcQuad Source quad.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aDstQuad Destination quad.
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref affine_transform_error_codes
 */
NppStatus 
nppiWarpAffineQuad_32s_AC4R(const Npp32s * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, const double aSrcQuad[4][2], 
                                  Npp32s * pDst, int nDstStep, NppiRect oDstROI, const double aDstQuad[4][2], 
                            int eInterpolation);

/**
 * Three-channel planar 32-bit signed integer quad-based affine warp.
 * 
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param aSrcQuad Source quad.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aDstQuad Destination quad.
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref affine_transform_error_codes
 */
NppStatus 
nppiWarpAffineQuad_32s_P3R(const Npp32s * pSrc[3], NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, const double aSrcQuad[4][2], 
                                 Npp32s * pDst[3], int nDstStep, NppiRect oDstROI, const double aDstQuad[4][2], 
                           int eInterpolation);

/**
 * Four-channel planar 32-bit signed integer quad-based affine warp.
 * 
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param aSrcQuad Source quad.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aDstQuad Destination quad.
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref affine_transform_error_codes
 */
NppStatus 
nppiWarpAffineQuad_32s_P4R(const Npp32s * pSrc[4], NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, const double aSrcQuad[4][2], 
                                 Npp32s * pDst[4], int nDstStep, NppiRect oDstROI, const double aDstQuad[4][2], 
                           int eInterpolation);

/**
 * Single-channel 32-bit floating-point quad-based affine warp.
 * 
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param aSrcQuad Source quad.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aDstQuad Destination quad.
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref affine_transform_error_codes
 */
NppStatus 
nppiWarpAffineQuad_32f_C1R(const Npp32f * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, const double aSrcQuad[4][2], 
                                 Npp32f * pDst, int nDstStep, NppiRect oDstROI, const double aDstQuad[4][2], 
                           int eInterpolation);

/**
 * Three-channel 32-bit floating-point quad-based affine warp.
 * 
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param aSrcQuad Source quad.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aDstQuad Destination quad.
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref affine_transform_error_codes
 */
NppStatus 
nppiWarpAffineQuad_32f_C3R(const Npp32f * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, const double aSrcQuad[4][2], 
                                 Npp32f * pDst, int nDstStep, NppiRect oDstROI, const double aDstQuad[4][2], 
                           int eInterpolation);

/**
 * Four-channel 32-bit floating-point quad-based affine warp.
 * 
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param aSrcQuad Source quad.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aDstQuad Destination quad.
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref affine_transform_error_codes
 */
NppStatus 
nppiWarpAffineQuad_32f_C4R(const Npp32f * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, const double aSrcQuad[4][2],
                                 Npp32f * pDst, int nDstStep, NppiRect oDstROI, const double aDstQuad[4][2], 
                           int eInterpolation);

/**
 * Four-channel 32-bit floating-point quad-based affine warp, ignoring alpha channel.
 * 
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param aSrcQuad Source quad.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aDstQuad Destination quad.
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref affine_transform_error_codes
 */
NppStatus 
nppiWarpAffineQuad_32f_AC4R(const Npp32f * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, const double aSrcQuad[4][2], 
                                  Npp32f * pDst, int nDstStep, NppiRect oDstROI, const double aDstQuad[4][2], 
                            int eInterpolation);

/**
 * Three-channel planar 32-bit floating-point quad-based affine warp.
 * 
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param aSrcQuad Source quad.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aDstQuad Destination quad.
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref affine_transform_error_codes
 */
NppStatus 
nppiWarpAffineQuad_32f_P3R(const Npp32f * pSrc[3], NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, const double aSrcQuad[4][2], 
                                 Npp32f * pDst[3], int nDstStep, NppiRect oDstROI, const double aDstQuad[4][2], 
                           int eInterpolation);

/**
 * Four-channel planar 32-bit floating-point quad-based affine warp.
 * 
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param aSrcQuad Source quad.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aDstQuad Destination quad.
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref affine_transform_error_codes
 */
NppStatus 
nppiWarpAffineQuad_32f_P4R(const Npp32f * pSrc[4], NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, const double aSrcQuad[4][2], 
                                 Npp32f * pDst[4], int nDstStep, NppiRect oDstROI, const double aDstQuad[4][2], 
                           int eInterpolation);


/** @} Quad-Based Affine Transform Section */

/** @} image_affine_transforms */

/** @defgroup image_perspective_transforms Perspective Transform
 *
 * \section perspective_transform_error_codes Perspective Transform Error Codes
 *
 *         - ::NPP_RECT_ERROR Indicates an error condition if width or height of
 *           the intersection of the oSrcROI and source image is less than or
 *           equal to 1
 *         - ::NPP_WRONG_INTERSECTION_ROI_ERROR Indicates an error condition if
 *           oSrcROI has no intersection with the source image
 *         - ::NPP_INTERPOLATION_ERROR Indicates an error condition if
 *           interpolation has an illegal value
 *         - ::NPP_COEFF_ERROR Indicates an error condition if coefficient values
 *           are invalid
 *         - ::NPP_WRONG_INTERSECTION_QUAD_WARNING Indicates a warning that no
 *           operation is performed if the transformed source ROI has no
 *           intersection with the destination ROI
 *
 * @{
 *
 */

/** @name Utility Functions
 *
 * @{
 *
 */

/**
 * Calculates perspective transform coefficients given source rectangular ROI
 * and its destination quadrangle projection
 *
 * \param oSrcROI Source ROI
 * \param quad Destination quadrangle
 * \param aCoeffs Perspective transform coefficients
 * \return Error codes:
 *         - ::NPP_SIZE_ERROR Indicates an error condition if any image dimension
 *           has zero or negative value
 *         - ::NPP_RECT_ERROR Indicates an error condition if width or height of
 *           the intersection of the oSrcROI and source image is less than or
 *           equal to 1
 *         - ::NPP_COEFF_ERROR Indicates an error condition if coefficient values
 *           are invalid
 */
NppStatus 
nppiGetPerspectiveTransform(NppiRect oSrcROI, const double quad[4][2], double aCoeffs[3][3]);


/**
 * Calculates perspective transform projection of given source rectangular
 * ROI
 *
 * \param oSrcROI Source ROI
 * \param quad Destination quadrangle
 * \param aCoeffs Perspective transform coefficients
 * \return Error codes:
 *         - ::NPP_SIZE_ERROR Indicates an error condition if any image dimension
 *           has zero or negative value
 *         - ::NPP_RECT_ERROR Indicates an error condition if width or height of
 *           the intersection of the oSrcROI and source image is less than or
 *           equal to 1
 *         - ::NPP_COEFF_ERROR Indicates an error condition if coefficient values
 *           are invalid
 */
NppStatus 
nppiGetPerspectiveQuad(NppiRect oSrcROI, double quad[4][2], const double aCoeffs[3][3]);


/**
 * Calculates bounding box of the perspective transform projection of the
 * given source rectangular ROI
 *
 * \param oSrcROI Source ROI
 * \param bound Bounding box of the transformed source ROI
 * \param aCoeffs Perspective transform coefficients
 * \return Error codes:
 *         - ::NPP_SIZE_ERROR Indicates an error condition if any image dimension
 *           has zero or negative value
 *         - ::NPP_RECT_ERROR Indicates an error condition if width or height of
 *           the intersection of the oSrcROI and source image is less than or
 *           equal to 1
 *         - ::NPP_COEFF_ERROR Indicates an error condition if coefficient values
 *           are invalid
 */
NppStatus 
nppiGetPerspectiveBound(NppiRect oSrcROI, double bound[2][2], const double aCoeffs[3][3]);

/** @} Utility Functions Section */

/** @name Perspective Transform
 * Transforms (warps) an image based on a perspective transform. The perspective
 * transform is given as a \f$3\times 3\f$ matrix C. A pixel location \f$(x, y)\f$ in the
 * source image is mapped to the location \f$(x', y')\f$ in the destination image.
 * The destination image coorodinates are computed as follows:
 * \f[
 * x' = \frac{c_{00} * x + c_{01} * y + c_{02}}{c_{20} * x + c_{21} * y + c_{22}} \qquad
 * y' = \frac{c_{10} * x + c_{11} * y + c_{12}}{c_{20} * x + c_{21} * y + c_{22}}
 * \f]
 * \f[
 * C = \left[ \matrix{c_{00} & c_{01} & c_{02}   \cr 
                      c_{10} & c_{11} & c_{12}   \cr 
                      c_{20} & c_{21} & c_{22} } \right]
 * \f]
 *
 * @{
 *
 */

/**
 * Single-channel 8-bit unsigned integer perspective warp.
 *
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aCoeffs Perspective transform coefficients
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref perspective_transform_error_codes
 */
NppStatus 
nppiWarpPerspective_8u_C1R(const Npp8u * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                                 Npp8u * pDst, int nDstStep, NppiRect oDstROI, 
                           const double aCoeffs[3][3], int eInterpolation);

/**
 * Three-channel 8-bit unsigned integer perspective warp.
 *
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aCoeffs Perspective transform coefficients
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref perspective_transform_error_codes
 */
NppStatus 
nppiWarpPerspective_8u_C3R(const Npp8u * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                                 Npp8u * pDst, int nDstStep, NppiRect oDstROI, 
                           const double aCoeffs[3][3], int eInterpolation);

/**
 * Four-channel 8-bit unsigned integer perspective warp.
 *
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aCoeffs Perspective transform coefficients
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref perspective_transform_error_codes
 */
NppStatus 
nppiWarpPerspective_8u_C4R(const Npp8u * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                                 Npp8u * pDst, int nDstStep, NppiRect oDstROI, 
                           const double aCoeffs[3][3], int eInterpolation);

/**
 * Four-channel 8-bit unsigned integer perspective warp, ignoring alpha channel.
 *
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aCoeffs Perspective transform coefficients
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref perspective_transform_error_codes
 */
NppStatus 
nppiWarpPerspective_8u_AC4R(const Npp8u * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                                  Npp8u * pDst, int nDstStep, NppiRect oDstROI, 
                            const double aCoeffs[3][3], int eInterpolation);

/**
 * Three-channel planar 8-bit unsigned integer perspective warp.
 *
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aCoeffs Perspective transform coefficients
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref perspective_transform_error_codes
 */
NppStatus 
nppiWarpPerspective_8u_P3R(const Npp8u * pSrc[3], NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                                 Npp8u * pDst[3], int nDstStep, NppiRect oDstROI, 
                           const double aCoeffs[3][3], int eInterpolation);

/**
 * Four-channel planar 8-bit unsigned integer perspective warp.
 *
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aCoeffs Perspective transform coefficients
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref perspective_transform_error_codes
 */
NppStatus 
nppiWarpPerspective_8u_P4R(const Npp8u * pSrc[4], NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                                 Npp8u * pDst[4], int nDstStep, NppiRect oDstROI, 
                           const double aCoeffs[3][3], int eInterpolation);

/**
 * Single-channel 16-bit unsigned integer perspective warp.
 *
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aCoeffs Perspective transform coefficients
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref perspective_transform_error_codes
 */
NppStatus 
nppiWarpPerspective_16u_C1R(const Npp16u * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                                  Npp16u * pDst, int nDstStep, NppiRect oDstROI, 
                            const double aCoeffs[3][3], int eInterpolation);

/**
 * Three-channel 16-bit unsigned integer perspective warp.
 *
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aCoeffs Perspective transform coefficients
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref perspective_transform_error_codes
 */
NppStatus 
nppiWarpPerspective_16u_C3R(const Npp16u * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                                  Npp16u * pDst, int nDstStep, NppiRect oDstROI, 
                            const double aCoeffs[3][3],int eInterpolation);

/**
 * Four-channel 16-bit unsigned integer perspective warp.
 *
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aCoeffs Perspective transform coefficients
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref perspective_transform_error_codes
 */
NppStatus 
nppiWarpPerspective_16u_C4R(const Npp16u * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                                  Npp16u * pDst, int nDstStep, NppiRect oDstROI, 
                            const double aCoeffs[3][3], int eInterpolation);

/**
 * Four-channel 16-bit unsigned integer perspective warp, igoring alpha channel.
 *
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aCoeffs Perspective transform coefficients
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref perspective_transform_error_codes
 */
NppStatus 
nppiWarpPerspective_16u_AC4R(const Npp16u * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                                   Npp16u * pDst, int nDstStep, NppiRect oDstROI, 
                             const double aCoeffs[3][3], int eInterpolation);

/**
 * Three-channel planar 16-bit unsigned integer perspective warp.
 *
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aCoeffs Perspective transform coefficients
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref perspective_transform_error_codes
 */
NppStatus 
nppiWarpPerspective_16u_P3R(const Npp16u * pSrc[3], NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                                  Npp16u * pDst[3], int nDstStep, NppiRect oDstROI, 
                            const double aCoeffs[3][3], int eInterpolation);

/**
 * Four-channel planar 16-bit unsigned integer perspective warp.
 *
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aCoeffs Perspective transform coefficients
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref perspective_transform_error_codes
 */
NppStatus 
nppiWarpPerspective_16u_P4R(const Npp16u * pSrc[4], NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                                  Npp16u * pDst[4], int nDstStep, NppiRect oDstROI, 
                            const double aCoeffs[3][3], int eInterpolation);

/**
 * Single-channel 32-bit signed integer perspective warp.
 *
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aCoeffs Perspective transform coefficients
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref perspective_transform_error_codes
 */
NppStatus 
nppiWarpPerspective_32s_C1R(const Npp32s * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                                  Npp32s * pDst, int nDstStep, NppiRect oDstROI, 
                            const double aCoeffs[3][3], int eInterpolation);

/**
 * Three-channel 32-bit signed integer perspective warp.
 *
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aCoeffs Perspective transform coefficients
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref perspective_transform_error_codes
 */
NppStatus 
nppiWarpPerspective_32s_C3R(const Npp32s * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                                  Npp32s * pDst, int nDstStep, NppiRect oDstROI, 
                            const double aCoeffs[3][3], int eInterpolation);

/**
 * Four-channel 32-bit signed integer perspective warp.
 *
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aCoeffs Perspective transform coefficients
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref perspective_transform_error_codes
 */
NppStatus 
nppiWarpPerspective_32s_C4R(const Npp32s * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                                  Npp32s * pDst, int nDstStep, NppiRect oDstROI, 
                            const double aCoeffs[3][3], int eInterpolation);

/**
 * Four-channel 32-bit signed integer perspective warp, igoring alpha channel.
 *
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aCoeffs Perspective transform coefficients
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref perspective_transform_error_codes
 */
NppStatus 
nppiWarpPerspective_32s_AC4R(const Npp32s * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                                   Npp32s * pDst, int nDstStep, NppiRect oDstROI, 
                             const double aCoeffs[3][3], int eInterpolation);

/**
 * Three-channel planar 32-bit signed integer perspective warp.
 *
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aCoeffs Perspective transform coefficients
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref perspective_transform_error_codes
 */
NppStatus 
nppiWarpPerspective_32s_P3R(const Npp32s * pSrc[3], NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                                  Npp32s * pDst[3], int nDstStep, NppiRect oDstROI, 
                            const double aCoeffs[3][3], int eInterpolation);

/**
 * Four-channel planar 32-bit signed integer perspective warp.
 *
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aCoeffs Perspective transform coefficients
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref perspective_transform_error_codes
 */
NppStatus 
nppiWarpPerspective_32s_P4R(const Npp32s * pSrc[4], NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                                  Npp32s * pDst[4], int nDstStep, NppiRect oDstROI, 
                            const double aCoeffs[3][3], int eInterpolation);

/**
 * Single-channel 32-bit floating-point perspective warp.
 *
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aCoeffs Perspective transform coefficients
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref perspective_transform_error_codes
 */
NppStatus 
nppiWarpPerspective_32f_C1R(const Npp32f * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                                  Npp32f * pDst, int nDstStep, NppiRect oDstROI, 
                            const double aCoeffs[3][3], int eInterpolation);

/**
 * Three-channel 32-bit floating-point perspective warp.
 *
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aCoeffs Perspective transform coefficients
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref perspective_transform_error_codes
 */
NppStatus 
nppiWarpPerspective_32f_C3R(const Npp32f * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                                  Npp32f * pDst, int nDstStep, NppiRect oDstROI, 
                            const double aCoeffs[3][3], int eInterpolation);

/**
 * Four-channel 32-bit floating-point perspective warp.
 *
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aCoeffs Perspective transform coefficients
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref perspective_transform_error_codes
 */
NppStatus 
nppiWarpPerspective_32f_C4R(const Npp32f * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                                  Npp32f * pDst, int nDstStep, NppiRect oDstROI, 
                            const double aCoeffs[3][3], int eInterpolation);

/**
 * Four-channel 32-bit floating-point perspective warp, ignoring alpha channel.
 *
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aCoeffs Perspective transform coefficients
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref perspective_transform_error_codes
 */
NppStatus 
nppiWarpPerspective_32f_AC4R(const Npp32f * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                                   Npp32f * pDst, int nDstStep, NppiRect oDstROI, 
                             const double aCoeffs[3][3], int eInterpolation);

/**
 * Three-channel planar 32-bit floating-point perspective warp.
 *
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aCoeffs Perspective transform coefficients
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref perspective_transform_error_codes
 */
NppStatus 
nppiWarpPerspective_32f_P3R(const Npp32f * pSrc[3], NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                                  Npp32f * pDst[3], int nDstStep, NppiRect oDstROI, 
                            const double aCoeffs[3][3], int eInterpolation);

/**
 * Four-channel planar 32-bit floating-point perspective warp.
 *
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aCoeffs Perspective transform coefficients
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref perspective_transform_error_codes
 */
NppStatus 
nppiWarpPerspective_32f_P4R(const Npp32f * pSrc[4], NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                                  Npp32f * pDst[4], int nDstStep, NppiRect oDstROI, 
                            const double aCoeffs[3][3], int eInterpolation);

/** @} Perspective Transform Section */


/** @name Backwards Perspective Transform
 * Transforms (warps) an image based on a perspective transform. The perspective
 * transform is given as a \f$3\times 3\f$ matrix C. A pixel location \f$(x, y)\f$ in the
 * source image is mapped to the location \f$(x', y')\f$ in the destination image.
 * The destination image coorodinates fullfil the following properties:
 * \f[
 * x = \frac{c_{00} * x' + c_{01} * y' + c_{02}}{c_{20} * x' + c_{21} * y' + c_{22}} \qquad
 * y = \frac{c_{10} * x' + c_{11} * y' + c_{12}}{c_{20} * x' + c_{21} * y' + c_{22}}
 * \f]
 * \f[
 * C = \left[ \matrix{c_{00} & c_{01} & c_{02}   \cr 
                      c_{10} & c_{11} & c_{12}   \cr 
                      c_{20} & c_{21} & c_{22} } \right]
 * \f]
 * In other words, given matrix \f$C\f$ the source image's shape is transfored to the destination image
 * using the inverse matrix \f$C^{-1}\f$:
 * \f[
 * M = C^{-1} = \left[ \matrix{m_{00} & m_{01} & m_{02} \cr 
                               m_{10} & m_{11} & m_{12} \cr 
                               m_{20} & m_{21} & m_{22} } \right]
 * x' = \frac{c_{00} * x + c_{01} * y + c_{02}}{c_{20} * x + c_{21} * y + c_{22}} \qquad
 * y' = \frac{c_{10} * x + c_{11} * y + c_{12}}{c_{20} * x + c_{21} * y + c_{22}}
 * \f]
 *
 * @{
 *
 */


/**
 * Single-channel 8-bit unsigned integer backwards perspective warp.
 *
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aCoeffs Perspective transform coefficients
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref perspective_transform_error_codes
 */
NppStatus 
nppiWarpPerspectiveBack_8u_C1R(const Npp8u * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                                     Npp8u * pDst, int nDstStep, NppiRect oDstROI, 
                               const double aCoeffs[3][3], int eInterpolation);

/**
 * Three-channel 8-bit unsigned integer backwards perspective warp.
 *
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aCoeffs Perspective transform coefficients
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref perspective_transform_error_codes
 */
NppStatus 
nppiWarpPerspectiveBack_8u_C3R(const Npp8u * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                                     Npp8u * pDst, int nDstStep, NppiRect oDstROI, 
                               const double aCoeffs[3][3], int eInterpolation);

/**
 * Four-channel 8-bit unsigned integer backwards perspective warp.
 *
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aCoeffs Perspective transform coefficients
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref perspective_transform_error_codes
 */
NppStatus 
nppiWarpPerspectiveBack_8u_C4R(const Npp8u * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI,
                                     Npp8u * pDst, int nDstStep, NppiRect oDstROI, 
                               const double aCoeffs[3][3], int eInterpolation);

/**
 * Four-channel 8-bit unsigned integer backwards perspective warp, igoring alpha channel.
 *
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aCoeffs Perspective transform coefficients
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref perspective_transform_error_codes
 */
NppStatus 
nppiWarpPerspectiveBack_8u_AC4R(const Npp8u * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                                      Npp8u * pDst, int nDstStep, NppiRect oDstROI, 
                                const double aCoeffs[3][3], int eInterpolation);

/**
 * Three-channel planar 8-bit unsigned integer backwards perspective warp.
 *
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aCoeffs Perspective transform coefficients
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref perspective_transform_error_codes
 */
NppStatus 
nppiWarpPerspectiveBack_8u_P3R(const Npp8u * pSrc[3], NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                                     Npp8u * pDst[3], int nDstStep, NppiRect oDstROI, 
                               const double aCoeffs[3][3], int eInterpolation);

/**
 * Four-channel planar 8-bit unsigned integer backwards perspective warp.
 *
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aCoeffs Perspective transform coefficients
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref perspective_transform_error_codes
 */
NppStatus 
nppiWarpPerspectiveBack_8u_P4R(const Npp8u * pSrc[4], NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                                     Npp8u * pDst[4], int nDstStep, NppiRect oDstROI, 
                               const double aCoeffs[3][3], int eInterpolation);


/**
 * Single-channel 16-bit unsigned integer backwards perspective warp.
 *
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aCoeffs Perspective transform coefficients
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref perspective_transform_error_codes
 */
NppStatus 
nppiWarpPerspectiveBack_16u_C1R(const Npp16u * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                                      Npp16u * pDst, int nDstStep, NppiRect oDstROI, 
                                const double aCoeffs[3][3], int eInterpolation);

/**
 * Three-channel 16-bit unsigned integer backwards perspective warp.
 *
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aCoeffs Perspective transform coefficients
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref perspective_transform_error_codes
 */
NppStatus 
nppiWarpPerspectiveBack_16u_C3R(const Npp16u * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                                      Npp16u * pDst, int nDstStep, NppiRect oDstROI, 
                                const double aCoeffs[3][3], int eInterpolation);
                                            
/**
 * Four-channel 16-bit unsigned integer backwards perspective warp.
 *
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aCoeffs Perspective transform coefficients
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref perspective_transform_error_codes
 */
NppStatus 
nppiWarpPerspectiveBack_16u_C4R(const Npp16u * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                                      Npp16u * pDst, int nDstStep, NppiRect oDstROI, 
                                const double aCoeffs[3][3], int eInterpolation);

/**
 * Four-channel 16-bit unsigned integer backwards perspective warp, ignoring alpha channel.
 *
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aCoeffs Perspective transform coefficients
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref perspective_transform_error_codes
 */
NppStatus 
nppiWarpPerspectiveBack_16u_AC4R(const Npp16u * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                                       Npp16u * pDst, int nDstStep, NppiRect oDstROI, 
                                 const double aCoeffs[3][3], int eInterpolation);

/**
 * Four-channel planar 16-bit unsigned integer backwards perspective warp.
 *
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aCoeffs Perspective transform coefficients
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref perspective_transform_error_codes
 */
NppStatus 
nppiWarpPerspectiveBack_16u_P3R(const Npp16u * pSrc[3], NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                                      Npp16u * pDst[3], int nDstStep, NppiRect oDstROI, 
                                const double aCoeffs[3][3], int eInterpolation);

/**
 * Four-channel planar 16-bit unsigned integer backwards perspective warp.
 *
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aCoeffs Perspective transform coefficients
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref perspective_transform_error_codes
 */
NppStatus 
nppiWarpPerspectiveBack_16u_P4R(const Npp16u * pSrc[4], NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                                      Npp16u * pDst[4], int nDstStep, NppiRect oDstROI, 
                                const double aCoeffs[3][3], int eInterpolation);

/**
 * Single-channel 32-bit signed integer backwards perspective warp.
 *
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aCoeffs Perspective transform coefficients
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref perspective_transform_error_codes
 */
NppStatus 
nppiWarpPerspectiveBack_32s_C1R(const Npp32s * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                                      Npp32s * pDst, int nDstStep, NppiRect oDstROI, 
                                const double aCoeffs[3][3], int eInterpolation);

/**
 * Three-channel 32-bit signed integer backwards perspective warp.
 *
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aCoeffs Perspective transform coefficients
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref perspective_transform_error_codes
 */
NppStatus 
nppiWarpPerspectiveBack_32s_C3R(const Npp32s * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                                      Npp32s * pDst, int nDstStep, NppiRect oDstROI, 
                                const double aCoeffs[3][3], int eInterpolation);

/**
 * Four-channel 32-bit signed integer backwards perspective warp.
 *
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aCoeffs Perspective transform coefficients
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref perspective_transform_error_codes
 */
NppStatus 
nppiWarpPerspectiveBack_32s_C4R(const Npp32s * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                                      Npp32s * pDst, int nDstStep, NppiRect oDstROI, 
                                const double aCoeffs[3][3], int eInterpolation);

/**
 * Four-channel 32-bit signed integer backwards perspective warp, ignoring alpha channel.
 *
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aCoeffs Perspective transform coefficients
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref perspective_transform_error_codes
 */
NppStatus 
nppiWarpPerspectiveBack_32s_AC4R(const Npp32s * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                                       Npp32s * pDst, int nDstStep, NppiRect oDstROI, 
                                 const double aCoeffs[3][3], int eInterpolation);

/**
 * Three-channel planar 32-bit signed integer backwards perspective warp.
 *
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aCoeffs Perspective transform coefficients
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref perspective_transform_error_codes
 */
NppStatus 
nppiWarpPerspectiveBack_32s_P3R(const Npp32s * pSrc[3], NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                                      Npp32s * pDst[3], int nDstStep, NppiRect oDstROI, 
                                const double aCoeffs[3][3], int eInterpolation);

/**
 * Four-channel planar 32-bit signed integer backwards perspective warp.
 *
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aCoeffs Perspective transform coefficients
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref perspective_transform_error_codes
 */
NppStatus 
nppiWarpPerspectiveBack_32s_P4R(const Npp32s * pSrc[4], NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                                      Npp32s * pDst[4], int nDstStep, NppiRect oDstROI, 
                                const double aCoeffs[3][3], int eInterpolation);

/**
 * Single-channel 32-bit floating-point backwards perspective warp.
 *
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aCoeffs Perspective transform coefficients
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref perspective_transform_error_codes
 */
NppStatus 
nppiWarpPerspectiveBack_32f_C1R(const Npp32f * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                                      Npp32f * pDst, int nDstStep, NppiRect oDstROI, 
                                const double aCoeffs[3][3], int eInterpolation);

/**
 * Three-channel 32-bit floating-point backwards perspective warp.
 *
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aCoeffs Perspective transform coefficients
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref perspective_transform_error_codes
 */
NppStatus 
nppiWarpPerspectiveBack_32f_C3R(const Npp32f * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                                      Npp32f * pDst, int nDstStep, NppiRect oDstROI, 
                                const double aCoeffs[3][3], int eInterpolation);

/**
 * Four-channel 32-bit floating-point backwards perspective warp.
 *
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aCoeffs Perspective transform coefficients
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref perspective_transform_error_codes
 */
NppStatus 
nppiWarpPerspectiveBack_32f_C4R(const Npp32f * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                                      Npp32f * pDst, int nDstStep, NppiRect oDstROI, 
                                const double aCoeffs[3][3], int eInterpolation);

/**
 * Four-channel 32-bit floating-point backwards perspective warp, ignorning alpha channel.
 *
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aCoeffs Perspective transform coefficients
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref perspective_transform_error_codes
 */
NppStatus 
nppiWarpPerspectiveBack_32f_AC4R(const Npp32f * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                                       Npp32f * pDst, int nDstStep, NppiRect oDstROI, 
                                 const double aCoeffs[3][3], int eInterpolation);

/**
 * Three-channel planar 32-bit floating-point backwards perspective warp.
 *
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aCoeffs Perspective transform coefficients
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref perspective_transform_error_codes
 */
NppStatus 
nppiWarpPerspectiveBack_32f_P3R(const Npp32f * pSrc[3], NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, 
                                      Npp32f * pDst[3], int nDstStep, NppiRect oDstROI, 
                                const double aCoeffs[3][3], int eInterpolation);

/**
 * Four-channel planar 32-bit floating-point backwards perspective warp.
 *
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aCoeffs Perspective transform coefficients
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref perspective_transform_error_codes
 */
NppStatus 
nppiWarpPerspectiveBack_32f_P4R(const Npp32f * pSrc[4], NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI,
                                      Npp32f * pDst[4], int nDstStep, NppiRect oDstROI, 
                                const double aCoeffs[3][3], int eInterpolation);

/** @} Backwards Perspective Transform Section */

/** @name Quad-Based Perspective Transform
 * Transforms (warps) an image based on an perspective transform. The perspective
 * transform is computed such that it maps a quadrilateral in source image space to a 
 * quadrilateral in destination image space. 
 *
 * @{
 *
 */

/**
 * Single-channel 8-bit unsigned integer quad-based perspective warp.
 * 
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param aSrcQuad Source quad.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aDstQuad Destination quad.
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref perspective_transform_error_codes
 */
 NppStatus 
nppiWarpPerspectiveQuad_8u_C1R(const Npp8u * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, const double aSrcQuad[4][2], 
                                     Npp8u * pDst, int nDstStep, NppiRect oDstROI, const double aDstQuad[4][2], int eInterpolation);

/**
 * Three-channel 8-bit unsigned integer quad-based perspective warp.
 * 
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param aSrcQuad Source quad.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aDstQuad Destination quad.
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref perspective_transform_error_codes
 */
NppStatus 
nppiWarpPerspectiveQuad_8u_C3R(const Npp8u * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, const double aSrcQuad[4][2], 
                                     Npp8u * pDst, int nDstStep, NppiRect oDstROI,  const double aDstQuad[4][2], int eInterpolation);

/**
 * Four-channel 8-bit unsigned integer quad-based perspective warp.
 * 
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param aSrcQuad Source quad.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aDstQuad Destination quad.
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref perspective_transform_error_codes
 */
NppStatus 
nppiWarpPerspectiveQuad_8u_C4R(const Npp8u * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, const double aSrcQuad[4][2], 
                                     Npp8u * pDst, int nDstStep, NppiRect oDstROI, const double aDstQuad[4][2], int eInterpolation);

/**
 * Four-channel 8-bit unsigned integer quad-based perspective warp, ignoring alpha channel.
 * 
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param aSrcQuad Source quad.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aDstQuad Destination quad.
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref perspective_transform_error_codes
 */
NppStatus 
nppiWarpPerspectiveQuad_8u_AC4R(const Npp8u * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, const double aSrcQuad[4][2], 
                                      Npp8u * pDst, int nDstStep, NppiRect oDstROI, const double aDstQuad[4][2], int eInterpolation);

/**
 * Three-channel planar 8-bit unsigned integer quad-based perspective warp.
 * 
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param aSrcQuad Source quad.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aDstQuad Destination quad.
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref perspective_transform_error_codes
 */
NppStatus 
nppiWarpPerspectiveQuad_8u_P3R(const Npp8u * pSrc[3], NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, const double aSrcQuad[4][2], 
                                     Npp8u * pDst[3], int nDstStep, NppiRect oDstROI, const double aDstQuad[4][2], int eInterpolation);

/**
 * Four-channel planar 8-bit unsigned integer quad-based perspective warp.
 * 
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param aSrcQuad Source quad.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aDstQuad Destination quad.
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref perspective_transform_error_codes
 */
NppStatus 
nppiWarpPerspectiveQuad_8u_P4R(const Npp8u * pSrc[4], NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, const double aSrcQuad[4][2], 
                                     Npp8u * pDst[4], int nDstStep, NppiRect oDstROI, const double aDstQuad[4][2], int eInterpolation);

/**
 * Single-channel 16-bit unsigned integer quad-based perspective warp.
 * 
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param aSrcQuad Source quad.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aDstQuad Destination quad.
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref perspective_transform_error_codes
 */
NppStatus 
nppiWarpPerspectiveQuad_16u_C1R(const Npp16u * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, const double aSrcQuad[4][2], 
                                      Npp16u * pDst, int nDstStep, NppiRect oDstROI, const double aDstQuad[4][2], int eInterpolation);

/**
 * Three-channel 16-bit unsigned integer quad-based perspective warp.
 * 
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param aSrcQuad Source quad.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aDstQuad Destination quad.
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref perspective_transform_error_codes
 */
NppStatus 
nppiWarpPerspectiveQuad_16u_C3R(const Npp16u * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, const double aSrcQuad[4][2], 
                                      Npp16u * pDst, int nDstStep, NppiRect oDstROI, const double aDstQuad[4][2], int eInterpolation);

/**
 * Four-channel 16-bit unsigned integer quad-based perspective warp.
 * 
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param aSrcQuad Source quad.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aDstQuad Destination quad.
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref perspective_transform_error_codes
 */
NppStatus 
nppiWarpPerspectiveQuad_16u_C4R(const Npp16u * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, const double aSrcQuad[4][2], 
                                      Npp16u * pDst, int nDstStep, NppiRect oDstROI, const double aDstQuad[4][2], int eInterpolation);

/**
 * Four-channel 16-bit unsigned integer quad-based perspective warp, ignoring alpha channel.
 * 
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param aSrcQuad Source quad.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aDstQuad Destination quad.
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref perspective_transform_error_codes
 */
NppStatus 
nppiWarpPerspectiveQuad_16u_AC4R(const Npp16u * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, const double aSrcQuad[4][2], 
                                       Npp16u * pDst, int nDstStep, NppiRect oDstROI, const double aDstQuad[4][2], int eInterpolation);

/**
 * Three-channel planar 16-bit unsigned integer quad-based perspective warp.
 * 
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param aSrcQuad Source quad.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aDstQuad Destination quad.
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref perspective_transform_error_codes
 */
NppStatus 
nppiWarpPerspectiveQuad_16u_P3R(const Npp16u * pSrc[3], NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, const double aSrcQuad[4][2], 
                                      Npp16u * pDst[3], int nDstStep, NppiRect oDstROI, const double aDstQuad[4][2], int eInterpolation);

/**
 * Four-channel planar 16-bit unsigned integer quad-based perspective warp.
 * 
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param aSrcQuad Source quad.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aDstQuad Destination quad.
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref perspective_transform_error_codes
 */
NppStatus 
nppiWarpPerspectiveQuad_16u_P4R(const Npp16u * pSrc[4], NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, const double aSrcQuad[4][2], 
                                      Npp16u * pDst[4], int nDstStep, NppiRect oDstROI, const double aDstQuad[4][2], int eInterpolation);

/**
 * Single-channel 32-bit signed integer quad-based perspective warp.
 * 
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param aSrcQuad Source quad.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aDstQuad Destination quad.
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref perspective_transform_error_codes
 */
NppStatus 
nppiWarpPerspectiveQuad_32s_C1R(const Npp32s * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, const double aSrcQuad[4][2], 
                                      Npp32s * pDst, int nDstStep, NppiRect oDstROI, const double aDstQuad[4][2], int eInterpolation);

/**
 * Three-channel 32-bit signed integer quad-based perspective warp.
 * 
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param aSrcQuad Source quad.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aDstQuad Destination quad.
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref perspective_transform_error_codes
 */
NppStatus 
nppiWarpPerspectiveQuad_32s_C3R(const Npp32s * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, const double aSrcQuad[4][2], 
                                      Npp32s * pDst, int nDstStep, NppiRect oDstROI, const double aDstQuad[4][2], int eInterpolation);

/**
 * Four-channel 32-bit signed integer quad-based perspective warp.
 * 
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param aSrcQuad Source quad.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aDstQuad Destination quad.
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref perspective_transform_error_codes
 */
NppStatus 
nppiWarpPerspectiveQuad_32s_C4R(const Npp32s * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, const double aSrcQuad[4][2], 
                                      Npp32s * pDst, int nDstStep, NppiRect oDstROI, const double aDstQuad[4][2], int eInterpolation);

/**
 * Four-channel 32-bit signed integer quad-based perspective warp, ignoring alpha channel.
 * 
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param aSrcQuad Source quad.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aDstQuad Destination quad.
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref perspective_transform_error_codes
 */
NppStatus 
nppiWarpPerspectiveQuad_32s_AC4R(const Npp32s * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, const double aSrcQuad[4][2], 
                                       Npp32s * pDst, int nDstStep, NppiRect oDstROI, const double aDstQuad[4][2], int eInterpolation);

/**
 * Three-channel planar 32-bit signed integer quad-based perspective warp.
 * 
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param aSrcQuad Source quad.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aDstQuad Destination quad.
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref perspective_transform_error_codes
 */
NppStatus 
nppiWarpPerspectiveQuad_32s_P3R(const Npp32s * pSrc[3], NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, const double aSrcQuad[4][2], 
                                      Npp32s * pDst[3], int nDstStep, NppiRect oDstROI, const double aDstQuad[4][2], int eInterpolation);

/**
 * Four-channel planar 32-bit signed integer quad-based perspective warp.
 * 
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param aSrcQuad Source quad.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aDstQuad Destination quad.
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref perspective_transform_error_codes
 */
NppStatus 
nppiWarpPerspectiveQuad_32s_P4R(const Npp32s * pSrc[4], NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, const double aSrcQuad[4][2], 
                                      Npp32s * pDst[4], int nDstStep, NppiRect oDstROI, const double aDstQuad[4][2], int eInterpolation);
                                            
/**
 * Single-channel 32-bit floating-point quad-based perspective warp.
 * 
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param aSrcQuad Source quad.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aDstQuad Destination quad.
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref perspective_transform_error_codes
 */
NppStatus 
nppiWarpPerspectiveQuad_32f_C1R(const Npp32f * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, const double aSrcQuad[4][2], 
                                      Npp32f * pDst, int nDstStep, NppiRect oDstROI, const double aDstQuad[4][2], int eInterpolation);

/**
 * Three-channel 32-bit floating-point quad-based perspective warp.
 * 
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param aSrcQuad Source quad.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aDstQuad Destination quad.
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref perspective_transform_error_codes
 */
NppStatus 
nppiWarpPerspectiveQuad_32f_C3R(const Npp32f * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, const double aSrcQuad[4][2], 
                                      Npp32f * pDst, int nDstStep, NppiRect oDstROI, const double aDstQuad[4][2], int eInterpolation);

/**
 * Four-channel 32-bit floating-point quad-based perspective warp.
 * 
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param aSrcQuad Source quad.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aDstQuad Destination quad.
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref perspective_transform_error_codes
 */
NppStatus 
nppiWarpPerspectiveQuad_32f_C4R(const Npp32f * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, const double aSrcQuad[4][2], 
                                      Npp32f * pDst, int nDstStep, NppiRect oDstROI, const double aDstQuad[4][2], int eInterpolation);

/**
 * Four-channel 32-bit floating-point quad-based perspective warp, ignoring alpha channel.
 * 
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param aSrcQuad Source quad.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aDstQuad Destination quad.
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref perspective_transform_error_codes
 */
NppStatus 
nppiWarpPerspectiveQuad_32f_AC4R(const Npp32f * pSrc, NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, const double aSrcQuad[4][2], 
                                       Npp32f * pDst, int nDstStep, NppiRect oDstROI, const double aDstQuad[4][2], int eInterpolation);

/**
 * Three-channel planar 32-bit floating-point quad-based perspective warp.
 * 
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param aSrcQuad Source quad.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aDstQuad Destination quad.
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref perspective_transform_error_codes
 */
NppStatus 
nppiWarpPerspectiveQuad_32f_P3R(const Npp32f * pSrc[3], NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, const double aSrcQuad[4][2], 
                                      Npp32f * pDst[3], int nDstStep, NppiRect oDstROI, const double aDstQuad[4][2], int eInterpolation);

/**
 * Four-channel planar 32-bit floating-point quad-based perspective warp.
 * 
 * \param pSrc \ref source_image_pointer.
 * \param oSrcSize Size of source image in pixels
 * \param nSrcStep \ref source_image_line_step.
 * \param oSrcROI Source ROI
 * \param aSrcQuad Source quad.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param oDstROI Destination ROI
 * \param aDstQuad Destination quad.
 * \param eInterpolation Interpolation mode: can be NPPI_INTER_NN,
 *        NPPI_INTER_LINEAR or NPPI_INTER_CUBIC
 * \return \ref image_data_error_codes, \ref roi_error_codes, \ref perspective_transform_error_codes
 */
NppStatus 
nppiWarpPerspectiveQuad_32f_P4R(const Npp32f * pSrc[4], NppiSize oSrcSize, int nSrcStep, NppiRect oSrcROI, const double aSrcQuad[4][2], 
                                      Npp32f * pDst[4], int nDstStep, NppiRect oDstROI, const double aDstQuad[4][2], int eInterpolation);



/** @} Quad-Based Perspective Transform Section */

/** @} image_perspective_transforms */

/** @} image_geometry_transforms */

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* NV_NPPI_GEOMETRY_TRANSFORMS_H */
