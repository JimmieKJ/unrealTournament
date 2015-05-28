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
#ifndef NV_NPPI_COMPUTER_VISION_H
#define NV_NPPI_COMPUTER_VISION_H
 
/**
 * \file nppi_computer_vision.h
 * NPP Image Processing Functionality.
 */
 
#include "nppdefs.h"


#ifdef __cplusplus
extern "C" {
#endif

/** @defgroup image_labeling_and_segmentation Labeling and Segmentation
 *  @ingroup nppi
 *
 * Pixel labeling and image segementation operations.
 *
 * @{
 *
 */

#if defined (__cplusplus)
struct NppiGraphcutState;
#else
typedef struct NppiGraphcutState NppiGraphcutState;
#endif

/** @defgroup image_graphcut GraphCut
 *
 * @{
 *
 */

/** @name Graphcut
 *
 * @{
 *
 */

/**
 * Calculates the size of the temporary buffer for graph-cut with 4 neighborhood labeling.
 *
 * \see nppiGraphcutInitAlloc(), nppiGraphcut_32s8u().
 * 
 * \param oSize Graph size.
 * \param pBufSize Pointer to variable that returns the size of the
 *        temporary buffer. 
 *
 * \return NPP_SUCCESS Indicates no error. Any other value indicates an error
 *         or a warning
 * \return NPP_SIZE_ERROR Indicates an error condition if any image dimension
 *         has zero or negative value
 * \return NPP_NULL_POINTER_ERROR Indicates an error condition if pBufSize
 *         pointer is NULL
 */
NppStatus nppiGraphcutGetSize(NppiSize oSize, int * pBufSize);

/**
 * Calculates the size of the temporary buffer for graph-cut with 8 neighborhood labeling.
 *
 * \see nppiGraphcut8InitAlloc(), nppiGraphcut8_32s8u().
 * 
 * \param oSize Graph size.
 * \param pBufSize Pointer to variable that returns the size of the
 *        temporary buffer. 
 *
 * \return NPP_SUCCESS Indicates no error. Any other value indicates an error
 *         or a warning
 * \return NPP_SIZE_ERROR Indicates an error condition if any image dimension
 *         has zero or negative value
 * \return NPP_NULL_POINTER_ERROR Indicates an error condition if pBufSize
 *         pointer is NULL
 */
NppStatus nppiGraphcut8GetSize(NppiSize oSize, int * pBufSize);

/**
 * Initializes graph-cut state structure and allocates additional resources for graph-cut with 8 neighborhood labeling.
 *
 * \see nppiGraphcut_32s8u(), nppiGraphcutGetSize().
 * 
 * \param oSize Graph size
 * \param ppState Pointer to pointer to graph-cut state structure. 
 * \param pDeviceMem pDeviceMem to the sufficient amount of device memory. The CUDA runtime or NPP memory allocators
 *      must be used to allocate this memory. The minimum amount of device memory required
 *      to run graph-cut on a for a specific image size is computed by nppiGraphcutGetSize().
 *
 * \return NPP_SUCCESS Indicates no error. Any other value indicates an error
 *         or a warning
 * \return NPP_SIZE_ERROR Indicates an error condition if any image dimension
 *         has zero or negative value
 * \return NPP_NULL_POINTER_ERROR Indicates an error condition if pBufSize
 *         pointer is NULL
 */
NppStatus nppiGraphcutInitAlloc(NppiSize oSize, NppiGraphcutState** ppState, Npp8u* pDeviceMem);

/**
 * Allocates and initializes the graph-cut state structure and additional resources for graph-cut with 8 neighborhood labeling.
 *
 * \see nppiGraphcut8_32s8u(), nppiGraphcut8GetSize().
 * 
 * \param oSize Graph size
 * \param ppState Pointer to pointer to graph-cut state structure. 
 * \param pDeviceMem to the sufficient amount of device memory. The CUDA runtime or NPP memory allocators
 *      must be used to allocate this memory. The minimum amount of device memory required
 *      to run graph-cut on a for a specific image size is computed by nppiGraphcut8GetSize().
 *
 * \return NPP_SUCCESS Indicates no error. Any other value indicates an error
 *         or a warning
 * \return NPP_SIZE_ERROR Indicates an error condition if any image dimension
 *         has zero or negative value
 * \return NPP_NULL_POINTER_ERROR Indicates an error condition if pBufSize
 *         pointer is NULL
 */
NppStatus nppiGraphcut8InitAlloc(NppiSize oSize, NppiGraphcutState ** ppState, Npp8u * pDeviceMem);

/**
 * Frees the additional resources of the graph-cut state structure.
 *
 * \see nppiGraphcutInitAlloc
 * \see nppiGraphcut8InitAlloc
 * 
 * \param pState Pointer to graph-cut state structure. 
 *
 * \return NPP_SUCCESS Indicates no error. Any other value indicates an error
 *         or a warning
 * \return NPP_SIZE_ERROR Indicates an error condition if any image dimension
 *         has zero or negative value
 * \return NPP_NULL_POINTER_ERROR Indicates an error condition if pState
 *         pointer is NULL
 */
NppStatus nppiGraphcutFree(NppiGraphcutState * pState);

/**
 * Graphcut of a flow network (32bit signed integer edge capacities). The
 * function computes the minimal cut (graphcut) of a 2D regular 4-connected
 * graph. 
 * The inputs are the capacities of the horizontal (in transposed form),
 * vertical and terminal (source and sink) edges. The capacities to source and
 * sink 
 * are stored as capacity differences in the terminals array 
 * ( terminals(x) = source(x) - sink(x) ). The implementation assumes that the
 * edge capacities 
 * for boundary edges that would connect to nodes outside the specified domain
 * are set to 0 (for example left(0,*) == 0). If this is not fulfilled the
 * computed labeling may be wrong!
 * The computed binary labeling is encoded as unsigned 8bit values (0 and >0).
 *
 * \see nppiGraphcutInitAlloc(), nppiGraphcutFree(), nppiGraphcutGetSize().
 *
 * \param pTerminals Pointer to differences of terminal edge capacities
 *        (terminal(x) = source(x) - sink(x))
 * \param pLeftTransposed Pointer to transposed left edge capacities
 *        (left(0,*) must be 0)
 * \param pRightTransposed Pointer to transposed right edge capacities
 *        (right(width-1,*) must be 0)
 * \param pTop Pointer to top edge capacities (top(*,0) must be 0)
 * \param pBottom Pointer to bottom edge capacities (bottom(*,height-1)
 *        must be 0)
 * \param nStep Step in bytes between any pair of sequential rows of edge
 *        capacities
 * \param nTransposedStep Step in bytes between any pair of sequential
 *        rows of tranposed edge capacities
 * \param size Graph size
 * \param pLabel Pointer to destination label image 
 * \param nLabelStep Step in bytes between any pair of sequential rows of
 *        label image
 * \param pState Pointer to graph-cut state structure. This structure must be
 *          initialized allocated and initialized using nppiGraphcutInitAlloc().
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus nppiGraphcut_32s8u(Npp32s * pTerminals, Npp32s * pLeftTransposed, Npp32s * pRightTransposed, Npp32s * pTop, Npp32s * pBottom, int nStep, int nTransposedStep, NppiSize size, Npp8u * pLabel, int nLabelStep, NppiGraphcutState *pState);


/**
 * Graphcut of a flow network (32bit signed integer edge capacities). The
 * function computes the minimal cut (graphcut) of a 2D regular 8-connected
 * graph. 
 * The inputs are the capacities of the horizontal (in transposed form),
 * vertical and terminal (source and sink) edges. The capacities to source and
 * sink 
 * are stored as capacity differences in the terminals array 
 * ( terminals(x) = source(x) - sink(x) ). The implementation assumes that the
 * edge capacities 
 * for boundary edges that would connect to nodes outside the specified domain
 * are set to 0 (for example left(0,*) == 0). If this is not fulfilled the
 * computed labeling may be wrong!
 * The computed binary labeling is encoded as unsigned 8bit values (0 and >0).
 *
 * \see nppiGraphcut8InitAlloc(), nppiGraphcutFree(), nppiGraphcut8GetSize().
 *
 * \param pTerminals Pointer to differences of terminal edge capacities
 *        (terminal(x) = source(x) - sink(x))
 * \param pLeftTransposed Pointer to transposed left edge capacities
 *        (left(0,*) must be 0)
 * \param pRightTransposed Pointer to transposed right edge capacities
 *        (right(width-1,*) must be 0)
 * \param pTop Pointer to top edge capacities (top(*,0) must be 0)
 * \param pTopLeft Pointer to top left edge capacities (topleft(*,0) 
 *        & topleft(0,*) must be 0)
 * \param pTopRight Pointer to top right edge capacities (topright(*,0)
 *        & topright(width-1,*) must be 0)
 * \param pBottom Pointer to bottom edge capacities (bottom(*,height-1)
 *        must be 0)
 * \param pBottomLeft Pointer to bottom left edge capacities 
 *        (bottomleft(*,height-1) && bottomleft(0,*) must be 0)
 * \param pBottomRight Pointer to bottom right edge capacities 
 *        (bottomright(*,height-1) && bottomright(width-1,*) must be 0)
 * \param nStep Step in bytes between any pair of sequential rows of edge
 *        capacities
 * \param nTransposedStep Step in bytes between any pair of sequential
 *        rows of tranposed edge capacities
 * \param size Graph size
 * \param pLabel Pointer to destination label image 
 * \param nLabelStep Step in bytes between any pair of sequential rows of
 *        label image
 * \param pState Pointer to graph-cut state structure. This structure must be
 *          initialized allocated and initialized using nppiGraphcut8InitAlloc().
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus nppiGraphcut8_32s8u(Npp32s * pTerminals, Npp32s * pLeftTransposed, Npp32s * pRightTransposed, Npp32s * pTop, Npp32s * pTopLeft, Npp32s * pTopRight, Npp32s * pBottom, Npp32s * pBottomLeft, Npp32s * pBottomRight, int nStep, int nTransposedStep, NppiSize size, Npp8u * pLabel, int nLabelStep, NppiGraphcutState *pState);

/**
 * Graphcut of a flow network (32bit float edge capacities). The
 * function computes the minimal cut (graphcut) of a 2D regular 4-connected
 * graph. 
 * The inputs are the capacities of the horizontal (in transposed form),
 * vertical and terminal (source and sink) edges. The capacities to source and
 * sink 
 * are stored as capacity differences in the terminals array 
 * ( terminals(x) = source(x) - sink(x) ). The implementation assumes that the
 * edge capacities 
 * for boundary edges that would connect to nodes outside the specified domain
 * are set to 0 (for example left(0,*) == 0). If this is not fulfilled the
 * computed labeling may be wrong!
 * The computed binary labeling is encoded as unsigned 8bit values (0 and >0).
 *
 * \see nppiGraphcutInitAlloc(), nppiGraphcutFree(), nppiGraphcutGetSize().
 *
 * \param pTerminals Pointer to differences of terminal edge capacities
 *        (terminal(x) = source(x) - sink(x))
 * \param pLeftTransposed Pointer to transposed left edge capacities
 *        (left(0,*) must be 0)
 * \param pRightTransposed Pointer to transposed right edge capacities
 *        (right(width-1,*) must be 0)
 * \param pTop Pointer to top edge capacities (top(*,0) must be 0)
 * \param pBottom Pointer to bottom edge capacities (bottom(*,height-1)
 *        must be 0)
 * \param nStep Step in bytes between any pair of sequential rows of edge
 *        capacities
 * \param nTransposedStep Step in bytes between any pair of sequential
 *        rows of tranposed edge capacities
 * \param size Graph size
 * \param pLabel Pointer to destination label image 
 * \param nLabelStep Step in bytes between any pair of sequential rows of
 *        label image
 * \param pState Pointer to graph-cut state structure. This structure must be
 *          initialized allocated and initialized using nppiGraphcutInitAlloc().
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus nppiGraphcut_32f8u(Npp32f * pTerminals, Npp32f * pLeftTransposed, Npp32f * pRightTransposed, Npp32f * pTop, Npp32f * pBottom, int nStep, int nTransposedStep, NppiSize size, Npp8u * pLabel, int nLabelStep, NppiGraphcutState *pState);


/**
 * Graphcut of a flow network (32bit float edge capacities). The
 * function computes the minimal cut (graphcut) of a 2D regular 8-connected
 * graph. 
 * The inputs are the capacities of the horizontal (in transposed form),
 * vertical and terminal (source and sink) edges. The capacities to source and
 * sink 
 * are stored as capacity differences in the terminals array 
 * ( terminals(x) = source(x) - sink(x) ). The implementation assumes that the
 * edge capacities 
 * for boundary edges that would connect to nodes outside the specified domain
 * are set to 0 (for example left(0,*) == 0). If this is not fulfilled the
 * computed labeling may be wrong!
 * The computed binary labeling is encoded as unsigned 8bit values (0 and >0).
 *
 * \see nppiGraphcut8InitAlloc(), nppiGraphcutFree(), nppiGraphcut8GetSize().
 *
 * \param pTerminals Pointer to differences of terminal edge capacities
 *        (terminal(x) = source(x) - sink(x))
 * \param pLeftTransposed Pointer to transposed left edge capacities
 *        (left(0,*) must be 0)
 * \param pRightTransposed Pointer to transposed right edge capacities
 *        (right(width-1,*) must be 0)
 * \param pTop Pointer to top edge capacities (top(*,0) must be 0)
 * \param pTopLeft Pointer to top left edge capacities (topleft(*,0) 
 *        & topleft(0,*) must be 0)
 * \param pTopRight Pointer to top right edge capacities (topright(*,0)
 *        & topright(width-1,*) must be 0)
 * \param pBottom Pointer to bottom edge capacities (bottom(*,height-1)
 *        must be 0)
 * \param pBottomLeft Pointer to bottom left edge capacities 
 *        (bottomleft(*,height-1) && bottomleft(0,*) must be 0)
 * \param pBottomRight Pointer to bottom right edge capacities 
 *        (bottomright(*,height-1) && bottomright(width-1,*) must be 0)
 * \param nStep Step in bytes between any pair of sequential rows of edge
 *        capacities
 * \param nTransposedStep Step in bytes between any pair of sequential
 *        rows of tranposed edge capacities
 * \param size Graph size
 * \param pLabel Pointer to destination label image 
 * \param nLabelStep Step in bytes between any pair of sequential rows of
 *        label image
 * \param pState Pointer to graph-cut state structure. This structure must be
 *          initialized allocated and initialized using nppiGraphcut8InitAlloc().
 * \return \ref image_data_error_codes, \ref roi_error_codes
 */
NppStatus nppiGraphcut8_32f8u(Npp32f * pTerminals, Npp32f * pLeftTransposed, Npp32f * pRightTransposed, Npp32f * pTop, Npp32f * pTopLeft, Npp32f * pTopRight, Npp32f * pBottom, Npp32f * pBottomLeft, Npp32f * pBottomRight, int nStep, int nTransposedStep, NppiSize size, Npp8u * pLabel, int nLabelStep, NppiGraphcutState *pState);


/** @} end of Graphcut */

/** @} image_graphcut */

/** @} image_labeling_and_segmentation */

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* NV_NPPI_COMPUTER_VISION_H */
