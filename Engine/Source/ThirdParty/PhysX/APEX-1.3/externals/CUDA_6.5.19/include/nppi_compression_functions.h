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
#ifndef NV_NPPI_COMPRESSION_FUNCTIONS_H
#define NV_NPPI_COMPRESSION_FUNCTIONS_H
 
/**
 * \file nppi_compression_functions.h
 * NPP Image Processing Functionality.
 */
 
#include "nppdefs.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @defgroup image_compression Compression
 *  @ingroup nppi
 *
 * Image compression primitives.
 *
 * The JPEG standard defines a flow of level shift, DCT and quantization for
 * forward JPEG transform and inverse level shift, IDCT and de-quantization
 * for inverse JPEG transform. This group has the functions for both forward
 * and inverse functions. 
 *
 * @{
 *
 */

/** @defgroup image_quantization Quantization Functions
 *
 * @{
 *
 */

/**
 * Apply quality factor to raw 8-bit quantization table.
 *
 * This is effectively and in-place method that modifies a given raw
 * quantization table based on a quality factor.
 * Note that this method is a host method and that the pointer to the
 * raw quantization table is a host pointer.
 *
 * \param hpQuantRawTable Raw quantization table.
 * \param nQualityFactor Quality factor for the table. Range is [1:100].
 * \return Error code:
 *      ::NPP_NULL_POINTER_ERROR is returned if hpQuantRawTable is 0.
 */
NppStatus 
nppiQuantFwdRawTableInit_JPEG_8u(Npp8u * hpQuantRawTable, int nQualityFactor);

/**
 * Initializes a quantization table for nppiDCTQuantFwd8x8LS_JPEG_8u16s_C1R().
 *    The method creates a 16-bit version of the raw table and converts the 
 * data order from zigzag layout to original row-order layout since raw
 * quantization tables are typically stored in zigzag format.
 *
 * This method is a host method. It consumes and produces host data. I.e. the pointers
 * passed to this function must be host pointers. The resulting table needs to be
 * transferred to device memory in order to be used with nppiDCTQuantFwd8x8LS_JPEG_8u16s_C1R()
 * function.
 *
 * \param hpQuantRawTable Host pointer to raw quantization table as returned by 
 *      nppiQuantFwdRawTableInit_JPEG_8u(). The raw quantization table is assumed to be in
 *      zigzag order.
 * \param hpQuantFwdRawTable Forward quantization table for use with nppiDCTQuantFwd8x8LS_JPEG_8u16s_C1R().
 * \return Error code:
 *      ::NPP_NULL_POINTER_ERROR pQuantRawTable is 0.
 */
NppStatus 
nppiQuantFwdTableInit_JPEG_8u16u(const Npp8u * hpQuantRawTable, Npp16u * hpQuantFwdRawTable);

/**
 * Initializes a quantization table for nppiDCTQuantInv8x8LS_JPEG_16s8u_C1R().
 *      The nppiDCTQuantFwd8x8LS_JPEG_8u16s_C1R() method uses a quantization table
 * in a 16-bit format allowing for faster processing. In addition it converts the 
 * data order from zigzag layout to original row-order layout. Typically raw
 * quantization tables are stored in zigzag format.
 *
 * This method is a host method and consumes and produces host data. I.e. the pointers
 * passed to this function must be host pointers. The resulting table needs to be
 * transferred to device memory in order to be used with nppiDCTQuantFwd8x8LS_JPEG_8u16s_C1R()
 * function.
 *
 * \param hpQuantRawTable Raw quantization table.
 * \param hpQuantFwdRawTable Inverse quantization table.
 * \return ::NPP_NULL_POINTER_ERROR pQuantRawTable or pQuantFwdRawTable is0.
 */
NppStatus 
nppiQuantInvTableInit_JPEG_8u16u(const Npp8u * hpQuantRawTable, Npp16u * hpQuantFwdRawTable);


/**
 * Forward DCT, quantization and level shift part of the JPEG encoding.
 * Input is expected in 8x8 macro blocks and output is expected to be in 64x1
 * macro blocks.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param pQuantFwdTable Forward quantization tables for JPEG encoding created
 *          using nppiQuantInvTableInit_JPEG_8u16u().
 * \param oSizeROI \ref roi_specification.
 * \return Error codes:
 *         - ::NPP_SIZE_ERROR For negative input height/width or not a multiple of
 *           8 width/height.
 *         - ::NPP_STEP_ERROR If input image width is not multiple of 8 or does not
 *           match ROI.
 *         - ::NPP_NULL_POINTER_ERROR If the destination pointer is 0.
 */
NppStatus 
nppiDCTQuantFwd8x8LS_JPEG_8u16s_C1R(const Npp8u  * pSrc, int nSrcStep, 
                                          Npp16s * pDst, int nDstStep, 
                                    const Npp16u * pQuantFwdTable, NppiSize oSizeROI);

/**
 * Inverse DCT, de-quantization and level shift part of the JPEG decoding.
 * Input is expected in 64x1 macro blocks and output is expected to be in 8x8
 * macro blocks.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep Image width in pixels x 8 x sizeof(Npp16s).
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep Image width in pixels x 8 x sizeof(Npp16s).
 * \param pQuantInvTable Inverse quantization tables for JPEG decoding created
 *           using nppiQuantInvTableInit_JPEG_8u16u().
 * \param oSizeROI \ref roi_specification.
 * \return Error codes:
 *         - ::NPP_SIZE_ERROR For negative input height/width or not a multiple of
 *           8 width/height.
 *         - ::NPP_STEP_ERROR If input image width is not multiple of 8 or does not
 *           match ROI.
 *         - ::NPP_NULL_POINTER_ERROR If the destination pointer is 0.
 */
NppStatus 
nppiDCTQuantInv8x8LS_JPEG_16s8u_C1R(const Npp16s * pSrc, int nSrcStep, 
                                          Npp8u  * pDst, int nDstStep, 
                                    const Npp16u * pQuantInvTable, NppiSize oSizeROI);
   


#if defined (__cplusplus)
struct NppiDCTState;
#else
typedef struct NppiDCTState NppiDCTState;
#endif


/**
 * Initializes DCT state structure and allocates additional resources.
 *
 * \see nppiDCTQuantFwd8x8LS_JPEG_8u16s_C1R_NEW(), nppiDCTQuantInv8x8LS_JPEG_16s8u_C1R_NEW.
 * 
 * \param ppState Pointer to pointer to DCT state structure. 
 *
 * \return NPP_SUCCESS Indicates no error. Any other value indicates an error
 *         or a warning
 * \return NPP_SIZE_ERROR Indicates an error condition if any image dimension
 *         has zero or negative value
 * \return NPP_NULL_POINTER_ERROR Indicates an error condition if pBufSize
 *         pointer is NULL
 */
NppStatus nppiDCTInitAlloc(NppiDCTState** ppState);

/**
 * Frees the additional resources of the DCT state structure.
 *
 * \see nppiDCTInitAlloc
 * 
 * \param pState Pointer to DCT state structure. 
 *
 * \return NPP_SUCCESS Indicates no error. Any other value indicates an error
 *         or a warning
 * \return NPP_SIZE_ERROR Indicates an error condition if any image dimension
 *         has zero or negative value
 * \return NPP_NULL_POINTER_ERROR Indicates an error condition if pState
 *         pointer is NULL
 */
NppStatus nppiDCTFree(NppiDCTState* pState);

/**
 * Forward DCT, quantization and level shift part of the JPEG encoding.
 * Input is expected in 8x8 macro blocks and output is expected to be in 64x1
 * macro blocks. The new version of the primitive takes the ROI in image pixel size and
 * works with DCT coefficients that are in zig-zag order.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep \ref source_image_line_step.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep Image width in pixels x 8 x sizeof(Npp16s).
 * \param pQuantizationTable Quantization Table in zig-zag order.
 * \param oSizeROI \ref roi_specification.
 * \param pState Pointer to DCT state structure. This structure must be
 *          initialized allocated and initialized using nppiDCTInitAlloc(). 
 * \return Error codes:
 *         - ::NPP_SIZE_ERROR For negative input height/width or not a multiple of
 *           8 width/height.
 *         - ::NPP_STEP_ERROR If input image width is not multiple of 8 or does not
 *           match ROI.
 *         - ::NPP_NULL_POINTER_ERROR If the destination pointer is 0.
 */
NppStatus 
nppiDCTQuantFwd8x8LS_JPEG_8u16s_C1R_NEW(const Npp8u  * pSrc, int nSrcStep, 
                                        Npp16s * pDst, int nDstStep, 
                                        const Npp8u * pQuantizationTable, NppiSize oSizeROI,
                                        NppiDCTState* pState);



/**
 * Inverse DCT, de-quantization and level shift part of the JPEG decoding.
 * Input is expected in 64x1 macro blocks and output is expected to be in 8x8
 * macro blocks. The new version of the primitive takes the ROI in image pixel size and
 * works with DCT coefficients that are in zig-zag order.
 *
 * \param pSrc \ref source_image_pointer.
 * \param nSrcStep Image width in pixels x 8 x sizeof(Npp16s).
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param pQuantizationTable Quantization Table in zig-zag order.
 * \param oSizeROI \ref roi_specification.
 * \param pState Pointer to DCT state structure. This structure must be
 *          initialized allocated and initialized using nppiDCTInitAlloc().  
 * \return Error codes:
 *         - ::NPP_SIZE_ERROR For negative input height/width or not a multiple of
 *           8 width/height.
 *         - ::NPP_STEP_ERROR If input image width is not multiple of 8 or does not
 *           match ROI.
 *         - ::NPP_NULL_POINTER_ERROR If the destination pointer is 0.
 */
NppStatus 
nppiDCTQuantInv8x8LS_JPEG_16s8u_C1R_NEW(const Npp16s * pSrc, int nSrcStep, 
                                        Npp8u  * pDst, int nDstStep, 
                                        const Npp8u * pQuantizationTable, NppiSize oSizeROI,
                                        NppiDCTState* pState);
                                    

/** @} image_quantization */

#if defined (__cplusplus)
struct NppiDecodeHuffmanSpec;
#else
typedef struct NppiDecodeHuffmanSpec NppiDecodeHuffmanSpec;
#endif

/**
 * Returns the length of the NppiDecodeHuffmanSpec structure.
 * \param pSize Pointer to a variable that will receive the length of the NppiDecodeHuffmanSpec structure.
 * \return Error codes:
 *         - ::NPP_NULL_POINTER_ERROR If one of the pointers is 0.
**/
NppStatus
nppiDecodeHuffmanSpecGetBufSize_JPEG(int* pSize);

/**
 * Creates a Huffman table in a format that is suitable for the decoder on the host.
 * \param pRawHuffmanTable Huffman table formated as specified in the JPEG standard.
 * \param eTableType Enum specifying type of table (nppiDCTable or nppiACTable).
 * \param pHuffmanSpec Pointer to the Huffman table for the decoder
 * \return Error codes:
 *         - ::NPP_NULL_POINTER_ERROR If one of the pointers is 0.
**/
NppStatus
nppiDecodeHuffmanSpecInitHost_JPEG(const Npp8u* pRawHuffmanTable, NppiHuffmanTableType eTableType, NppiDecodeHuffmanSpec  *pHuffmanSpec);

/**
 * Allocates memory and creates a Huffman table in a format that is suitable for the decoder on the host.
 * \param pRawHuffmanTable Huffman table formated as specified in the JPEG standard.
 * \param eTableType Enum specifying type of table (nppiDCTable or nppiACTable).
 * \param ppHuffmanSpec Pointer to returned pointer to the Huffman table for the decoder
 * \return Error codes:
 *         - ::NPP_NULL_POINTER_ERROR If one of the pointers is 0.
**/
NppStatus
nppiDecodeHuffmanSpecInitAllocHost_JPEG(const Npp8u* pRawHuffmanTable, NppiHuffmanTableType eTableType, NppiDecodeHuffmanSpec  **ppHuffmanSpec);

/**
 * Frees the host memory allocated by nppiDecodeHuffmanSpecInitAllocHost_JPEG.
 * \param pHuffmanSpec Pointer to the Huffman table for the decoder

**/
NppStatus
nppiDecodeHuffmanSpecFreeHost_JPEG(NppiDecodeHuffmanSpec  *pHuffmanSpec);

/**
 * Huffman Decoding of the JPEG decoding on the host.
 * Input is expected in byte stuffed huffman encoded JPEG scan and output is expected to be 64x1 macro blocks.
 *
 * \param pSrc Byte-stuffed huffman encoded JPEG scan.
 * \param nLength Byte length of the input.
 * \param restartInterval Restart Interval, see JPEG standard.
 * \param Ss Start Coefficient, see JPEG standard.
 * \param Se End Coefficient, see JPEG standard.
 * \param Ah Bit Approximation High, see JPEG standard.
 * \param Al Bit Approximation Low, see JPEG standard.
 * \param pDst \ref destination_image_pointer.
 * \param nDstStep \ref destination_image_line_step.
 * \param pHuffmanTableDC DC Huffman table.
 * \param pHuffmanTableAC AC Huffman table.
 * \param oSizeROI \ref roi_specification.
 * \return Error codes:
 *         - ::NPP_SIZE_ERROR For negative input height/width or not a multiple of
 *           8 width/height.
 *         - ::NPP_STEP_ERROR If input image width is not multiple of 8 or does not
 *           match ROI.
 *         - ::NPP_NULL_POINTER_ERROR If the destination pointer is 0.
 */
NppStatus
nppiDecodeHuffmanScanHost_JPEG_8u16s_P1R(const Npp8u  * pSrc, Npp32s nLength,
                  Npp32s restartInterval, Npp32s Ss, Npp32s Se, Npp32s Ah, Npp32s Al,
                    Npp16s * pDst, Npp32s nDstStep,
                    NppiDecodeHuffmanSpec  * pHuffmanTableDC, 
                    NppiDecodeHuffmanSpec  * pHuffmanTableAC, 
                    NppiSize oSizeROI); 


/**
 * Huffman Decoding of the JPEG decoding on the host.
 * Input is expected in byte stuffed huffman encoded JPEG scan and output is expected to be 64x1 macro blocks.
 *
 * \param pSrc Byte-stuffed huffman encoded JPEG scan.
 * \param nLength Byte length of the input.
 * \param nRestartInterval Restart Interval, see JPEG standard. 
 * \param nSs Start Coefficient, see JPEG standard.
 * \param nSe End Coefficient, see JPEG standard.
 * \param nAh Bit Approximation High, see JPEG standard.
 * \param nAl Bit Approximation Low, see JPEG standard.
 * \param apDst \ref destination_image_pointer.
 * \param aDstStep \ref destination_image_line_step.
 * \param apHuffmanDCTable DC Huffman tables.
 * \param apHuffmanACTable AC Huffman tables.
 * \param aSizeROI \ref roi_specification.
 * \return Error codes:
 *         - ::NPP_SIZE_ERROR For negative input height/width or not a multiple of
 *           8 width/height.
 *         - ::NPP_STEP_ERROR If input image width is not multiple of 8 or does not
 *           match ROI.
 *         - ::NPP_NULL_POINTER_ERROR If the destination pointer is 0.
 */
 NppStatus
 nppiDecodeHuffmanScanHost_JPEG_8u16s_P3R(const Npp8u * pSrc, Npp32s nLength,
                  Npp32s nRestartInterval, Npp32s nSs, Npp32s nSe, Npp32s nAh, Npp32s nAl,
                    Npp16s * apDst[3], Npp32s aDstStep[3],
                    NppiDecodeHuffmanSpec * apHuffmanDCTable[3], 
                    NppiDecodeHuffmanSpec * apHuffmanACTable[3], 
                    NppiSize aSizeROI[3]);

/** @} image_compression */

#if defined (__cplusplus)
struct NppiEncodeHuffmanSpec;
#else
typedef struct NppiEncodeHuffmanSpec NppiEncodeHuffmanSpec;
#endif


/**
 * Returns the length of the NppiEncodeHuffmanSpec structure.
 * \param pSize Pointer to a variable that will receive the length of the NppiEncodeHuffmanSpec structure.
 * \return Error codes:
 *         - ::NPP_NULL_POINTER_ERROR If one of the pointers is 0.
**/
NppStatus
nppiEncodeHuffmanSpecGetBufSize_JPEG(int* pSize);

/**
 * Creates a Huffman table in a format that is suitable for the encoder.
 * \param pRawHuffmanTable Huffman table formated as specified in the JPEG standard.
 * \param eTableType Enum specifying type of table (nppiDCTable or nppiACTable).
 * \param pHuffmanSpec Pointer to the Huffman table for the decoder
 * \return Error codes:
 *         - ::NPP_NULL_POINTER_ERROR If one of the pointers is 0.
**/
NppStatus
nppiEncodeHuffmanSpecInit_JPEG(const Npp8u* pRawHuffmanTable, NppiHuffmanTableType eTableType, NppiEncodeHuffmanSpec  *pHuffmanSpec);

/**
 * Allocates memory and creates a Huffman table in a format that is suitable for the encoder.
 * \param pRawHuffmanTable Huffman table formated as specified in the JPEG standard.
 * \param eTableType Enum specifying type of table (nppiDCTable or nppiACTable).
 * \param ppHuffmanSpec Pointer to returned pointer to the Huffman table for the encoder
 * \return Error codes:
 *         - ::NPP_NULL_POINTER_ERROR If one of the pointers is 0.
**/
NppStatus
nppiEncodeHuffmanSpecInitAlloc_JPEG(const Npp8u* pRawHuffmanTable, NppiHuffmanTableType eTableType, NppiEncodeHuffmanSpec  **ppHuffmanSpec);

/**
 * Frees the memory allocated by nppiEncodeHuffmanSpecInitAlloc_JPEG.
 * \param pHuffmanSpec Pointer to the Huffman table for the encoder

**/
NppStatus
nppiEncodeHuffmanSpecFree_JPEG(NppiEncodeHuffmanSpec  *pHuffmanSpec);

/**
 * Huffman Encoding of the JPEG Encoding.
 * Input is expected to be 64x1 macro blocks and output is expected as byte stuffed huffman encoded JPEG scan.
 *
 * \param pSrc \ref destination_image_pointer.
 * \param nSrcStep \ref destination_image_line_step.
 * \param nRestartInterval Restart Interval, see JPEG standard. Currently only values <=0 are supported.
 * \param nSs Start Coefficient, see JPEG standard.
 * \param nSe End Coefficient, see JPEG standard.
 * \param nAh Bit Approximation High, see JPEG standard.
 * \param nAl Bit Approximation Low, see JPEG standard.
 * \param pDst Byte-stuffed huffman encoded JPEG scan.
 * \param nLength Byte length of the huffman encoded JPEG scan.
 * \param pHuffmanTableDC DC Huffman table.
 * \param pHuffmanTableAC AC Huffman table.
 * \param oSizeROI \ref roi_specification.
 * \return Error codes:
 *         - ::NPP_SIZE_ERROR For negative input height/width or not a multiple of
 *           8 width/height.
 *         - ::NPP_STEP_ERROR If input image width is not multiple of 8 or does not
 *           match ROI.
 *         - ::NPP_NULL_POINTER_ERROR If the destination pointer is 0.
 *         - ::NPP_NOT_SUFFICIENT_COMPUTE_CAPABILITY If the device has compute capability < 2.0. 
 */
NppStatus
nppiEncodeHuffmanScan_JPEG_8u16s_P1R(const Npp16s * pSrc, Npp32s nSrcStep,
                    Npp32s restartInterval, Npp32s Ss, Npp32s Se, Npp32s Ah, Npp32s Al,
                    Npp8u  * pDst, Npp32s* nLength,
                    NppiEncodeHuffmanSpec  * pHuffmanTableDC, 
                    NppiEncodeHuffmanSpec  * pHuffmanTableAC, 
                    NppiSize oSizeROI,
                    Npp8u* pTempStorage); 


/**
 * Huffman Encoding of the JPEG Encoding.
 * Input is expected to be 64x1 macro blocks and output is expected as byte stuffed huffman encoded JPEG scan.
 *
 * \param apSrc \ref destination_image_pointer.
 * \param aSrcStep \ref destination_image_line_step.
 * \param nRestartInterval Restart Interval, see JPEG standard. Currently only values <=0 are supported.
 * \param nSs Start Coefficient, see JPEG standard.
 * \param nSe End Coefficient, see JPEG standard.
 * \param nAh Bit Approximation High, see JPEG standard.
 * \param nAl Bit Approximation Low, see JPEG standard.
 * \param pDst Byte-stuffed huffman encoded JPEG scan.
 * \param nLength Byte length of the huffman encoded JPEG scan.
 * \param apHuffmanTableDC DC Huffman tables.
 * \param apHuffmanTableAC AC Huffman tables.
 * \param aSizeROI \ref roi_specification.
 * \return Error codes:
 *         - ::NPP_SIZE_ERROR For negative input height/width or not a multiple of
 *           8 width/height.
 *         - ::NPP_STEP_ERROR If input image width is not multiple of 8 or does not
 *           match ROI.
 *         - ::NPP_NULL_POINTER_ERROR If the destination pointer is 0.
 *         - ::NPP_NOT_SUFFICIENT_COMPUTE_CAPABILITY If the device has compute capability < 2.0.
 */
 NppStatus
 nppiEncodeHuffmanScan_JPEG_8u16s_P3R(Npp16s * apSrc[3], Npp32s aSrcStep[3],
                    Npp32s nRestartInterval, Npp32s nSs, Npp32s nSe, Npp32s nAh, Npp32s nAl,
                    Npp8u  * pDst, Npp32s* nLength,
                    NppiEncodeHuffmanSpec * apHuffmanDCTable[3], 
                    NppiEncodeHuffmanSpec * apHuffmanACTable[3], 
                    NppiSize aSizeROI[3],
                    Npp8u* pTempStorage);

/**
 * Optimize Huffman Encoding of the JPEG Encoding.
 * Input is expected to be 64x1 macro blocks and output is expected as byte stuffed huffman encoded JPEG scan.
 *
 * \param pSrc \ref destination_image_pointer.
 * \param nSrcStep \ref destination_image_line_step.
 * \param nRestartInterval Restart Interval, see JPEG standard. Currently only values <=0 are supported.
 * \param nSs Start Coefficient, see JPEG standard.
 * \param nSe End Coefficient, see JPEG standard.
 * \param nAh Bit Approximation High, see JPEG standard.
 * \param nAl Bit Approximation Low, see JPEG standard.
 * \param pDst Byte-stuffed huffman encoded JPEG scan.
 * \param pLength Pointer to the byte length of the huffman encoded JPEG scan.
 * \param hpCodesDC Host pointer to the code of the huffman tree for DC component.
 * \param hpTableDC Host pointer to the table of the huffman tree for DC component.
 * \param hpCodesAC Host pointer to the code of the huffman tree for AC component.
 * \param hpTableAC Host pointer to the table of the huffman tree for AC component.
* \param pHuffmanTableDC DC Huffman table.
 * \param pHuffmanTableAC AC Huffman table.
 * \param oSizeROI \ref roi_specification.
 * \return Error codes:
 *         - ::NPP_SIZE_ERROR For negative input height/width or not a multiple of
 *           8 width/height.
 *         - ::NPP_STEP_ERROR If input image width is not multiple of 8 or does not
 *           match ROI.
 *         - ::NPP_NULL_POINTER_ERROR If the destination pointer is 0.
 *         - ::NPP_NOT_SUFFICIENT_COMPUTE_CAPABILITY If the device has compute capability < 2.0. 
 */
NppStatus
nppiEncodeOptimizeHuffmanScan_JPEG_8u16s_P1R(const Npp16s * pSrc, Npp32s nSrcStep,
                                             Npp32s nRestartInterval, Npp32s nSs, 
                                             Npp32s nSe, Npp32s nAh, Npp32s nAl,
                                             Npp8u * pDst, Npp32s * pLength,
                                             Npp8u * hpCodesDC, Npp8u * hpTableDC,
                                             Npp8u * hpCodesAC, Npp8u * hpTableAC,
                                             NppiEncodeHuffmanSpec * aHuffmanDCTable, 
                                             NppiEncodeHuffmanSpec * aHuffmanACTable, 
                                             NppiSize oSizeROI, Npp8u * pTempStorage);

/**
 * Optimize Huffman Encoding of the JPEG Encoding.
 * Input is expected to be 64x1 macro blocks and output is expected as byte stuffed huffman encoded JPEG scan.
 *
 * \param apSrc \ref destination_image_pointer.
 * \param aSrcStep \ref destination_image_line_step.
 * \param nRestartInterval Restart Interval, see JPEG standard. Currently only values <=0 are supported.
 * \param nSs Start Coefficient, see JPEG standard.
 * \param nSe End Coefficient, see JPEG standard.
 * \param nAh Bit Approximation High, see JPEG standard.
 * \param nAl Bit Approximation Low, see JPEG standard.
 * \param pDst Byte-stuffed huffman encoded JPEG scan.
 * \param pLength Pointer to the byte length of the huffman encoded JPEG scan.
 * \param hpCodesDC Host pointer to the code of the huffman tree for DC component.
 * \param hpTableDC Host pointer to the table of the huffman tree for DC component.
 * \param hpCodesAC Host pointer to the code of the huffman tree for AC component.
 * \param hpTableAC Host pointer to the table of the huffman tree for AC component.
 * \param apHuffmanTableDC DC Huffman tables.
 * \param apHuffmanTableAC AC Huffman tables.
 * \param aSizeROI \ref roi_specification.
 * \return Error codes:
 *         - ::NPP_SIZE_ERROR For negative input height/width or not a multiple of
 *           8 width/height.
 *         - ::NPP_STEP_ERROR If input image width is not multiple of 8 or does not
 *           match ROI.
 *         - ::NPP_NULL_POINTER_ERROR If the destination pointer is 0.
 *         - ::NPP_NOT_SUFFICIENT_COMPUTE_CAPABILITY If the device has compute capability < 2.0.
 */
NppStatus
nppiEncodeOptimizeHuffmanScan_JPEG_8u16s_P3R(Npp16s * pSrc[3], Npp32s aSrcStep[3],
                                             Npp32s nRestartInterval, Npp32s nSs, 
                                             Npp32s nSe, Npp32s nAh, Npp32s nAl,
                                             Npp8u * pDst, Npp32s * pLength,
                                             Npp8u * hpCodesDC[3], Npp8u * hpTableDC[3],
                                             Npp8u * hpCodesAC[3], Npp8u * hpTableAC[3],
                                             NppiEncodeHuffmanSpec * aHuffmanDCTable[3], 
                                             NppiEncodeHuffmanSpec * aHuffmanACTable[3], 
                                             NppiSize oSizeROI[3], Npp8u * pTempStorage);

/**
 * Calculates the size of the temporary buffer for baseline Huffman encoding.
 *
 * \see nppiEncodeHuffmanScan_JPEG_8u16s_P1R(), nppiEncodeHuffmanScan_JPEG_8u16s_P3R().
 * 
 * \param oSize Image Dimension.
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
NppStatus nppiEncodeHuffmanGetSize(NppiSize oSize, int nChannels, int * pBufSize);

/**
 * Calculates the size of the temporary buffer for optimize Huffman coding.
 *
 * See \ref nppiGenerateOptimizeHuffmanTable_JPEG.
 * 
 * \param oSize Image Dimension.
 * \param nChannels Number of channels in the image.
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
NppStatus nppiEncodeOptimizeHuffmanGetSize(NppiSize oSize, int nChannels, int * pBufSize);


/** @} image_compression */


#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* NV_NPPI_COMPRESSION_FUNCTIONS_H */
