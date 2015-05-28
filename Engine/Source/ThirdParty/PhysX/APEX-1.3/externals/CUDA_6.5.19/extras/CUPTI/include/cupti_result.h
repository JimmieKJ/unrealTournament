/*
 * Copyright 2010-2014 NVIDIA Corporation.  All rights reserved.
 *
 * NOTICE TO LICENSEE:
 *
 * This source code and/or documentation ("Licensed Deliverables") are
 * subject to NVIDIA intellectual property rights under U.S. and
 * international Copyright laws.
 *
 * These Licensed Deliverables contained herein is PROPRIETARY and
 * CONFIDENTIAL to NVIDIA and is being provided under the terms and
 * conditions of a form of NVIDIA software license agreement by and
 * between NVIDIA and Licensee ("License Agreement") or electronically
 * accepted by Licensee.  Notwithstanding any terms or conditions to
 * the contrary in the License Agreement, reproduction or disclosure
 * of the Licensed Deliverables to any third party without the express
 * written consent of NVIDIA is prohibited.
 *
 * NOTWITHSTANDING ANY TERMS OR CONDITIONS TO THE CONTRARY IN THE
 * LICENSE AGREEMENT, NVIDIA MAKES NO REPRESENTATION ABOUT THE
 * SUITABILITY OF THESE LICENSED DELIVERABLES FOR ANY PURPOSE.  IT IS
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
 * C.F.R. 12.212 (SEPT 1995) and is provided to the U.S. Government
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

#if !defined(_CUPTI_RESULT_H_)
#define _CUPTI_RESULT_H_

#ifndef CUPTIAPI
#ifdef _WIN32
#define CUPTIAPI __stdcall
#else 
#define CUPTIAPI
#endif 
#endif 

#if defined(__cplusplus)
extern "C" {
#endif 

/**
 * \defgroup CUPTI_RESULT_API CUPTI Result Codes
 * Error and result codes returned by CUPTI functions.
 * @{
 */

/**
 * \brief CUPTI result codes.
 *
 * Error and result codes returned by CUPTI functions.
 */
typedef enum {
    /**
     * No error.
     */
    CUPTI_SUCCESS                                       = 0,
    /**
     * One or more of the parameters is invalid.
     */
    CUPTI_ERROR_INVALID_PARAMETER                       = 1,
    /**
     * The device does not correspond to a valid CUDA device.
     */
    CUPTI_ERROR_INVALID_DEVICE                          = 2,
    /**
     * The context is NULL or not valid.
     */
    CUPTI_ERROR_INVALID_CONTEXT                         = 3,
    /**
     * The event domain id is invalid.
     */
    CUPTI_ERROR_INVALID_EVENT_DOMAIN_ID                 = 4,   
    /**
     * The event id is invalid.
     */
    CUPTI_ERROR_INVALID_EVENT_ID                        = 5,
    /**
     * The event name is invalid.
     */
    CUPTI_ERROR_INVALID_EVENT_NAME                      = 6,
    /**
     * The current operation cannot be performed due to dependency on
     * other factors.
     */
    CUPTI_ERROR_INVALID_OPERATION                       = 7,
    /**
     * Unable to allocate enough memory to perform the requested
     * operation.
     */
    CUPTI_ERROR_OUT_OF_MEMORY                           = 8,
    /**
     * An error occurred on the performance monitoring hardware.
     */
    CUPTI_ERROR_HARDWARE                                = 9,
    /**
     * The output buffer size is not sufficient to return all
     * requested data.
     */
    CUPTI_ERROR_PARAMETER_SIZE_NOT_SUFFICIENT           = 10,
    /**
     * API is not implemented. 
     */
    CUPTI_ERROR_API_NOT_IMPLEMENTED                     = 11,
    /**
     * The maximum limit is reached.
     */
    CUPTI_ERROR_MAX_LIMIT_REACHED                       = 12,
    /**
     * The object is not yet ready to perform the requested operation.
     */
    CUPTI_ERROR_NOT_READY                               = 13,
    /**
     * The current operation is not compatible with the current state
     * of the object
     */
    CUPTI_ERROR_NOT_COMPATIBLE                          = 14,
    /**
     * CUPTI is unable to initialize its connection to the CUDA
     * driver.
     */
    CUPTI_ERROR_NOT_INITIALIZED                         = 15,
    /**
     * The metric id is invalid.
     */
    CUPTI_ERROR_INVALID_METRIC_ID                        = 16,
    /**
     * The metric name is invalid.
     */
    CUPTI_ERROR_INVALID_METRIC_NAME                      = 17,
    /**
     * The queue is empty.
     */
    CUPTI_ERROR_QUEUE_EMPTY                              = 18,
    /**
     * Invalid handle (internal?).
     */
    CUPTI_ERROR_INVALID_HANDLE                           = 19,
    /**
     * Invalid stream.
     */
    CUPTI_ERROR_INVALID_STREAM                           = 20,
    /**
     * Invalid kind.
     */
    CUPTI_ERROR_INVALID_KIND                             = 21,
    /**
     * Invalid event value.
     */
    CUPTI_ERROR_INVALID_EVENT_VALUE                      = 22,
    /**
     * CUPTI is disabled due to conflicts with other enabled profilers
     */
    CUPTI_ERROR_DISABLED                                 = 23,
    /**
     * Invalid module.
     */
    CUPTI_ERROR_INVALID_MODULE                           = 24,
    /**
     * Invalid metric value.
     */
    CUPTI_ERROR_INVALID_METRIC_VALUE                     = 25,
    /**
     * The performance monitoring hardware is in use by other client.
     */
    CUPTI_ERROR_HARDWARE_BUSY                            = 26,
    /**
     * The attempted operation is not supported on the current
     * system or device.
     */
    CUPTI_ERROR_NOT_SUPPORTED                            = 27,
    /**
     * An unknown internal error has occurred.
     */
    CUPTI_ERROR_UNKNOWN                                 = 999,
    CUPTI_ERROR_FORCE_INT                               = 0x7fffffff
} CUptiResult;

/**
 * \brief Get the descriptive string for a CUptiResult.
 * 
 * Return the descriptive string for a CUptiResult in \p *str.
 * \note \b Thread-safety: this function is thread safe.
 *
 * \param result The result to get the string for
 * \param str Returns the string
 *
 * \retval CUPTI_SUCCESS on success
 * \retval CUPTI_ERROR_INVALID_PARAMETER if \p str is NULL or \p
 * result is not a valid CUptiResult
 */
CUptiResult CUPTIAPI cuptiGetResultString(CUptiResult result, const char **str); 
 
/** @} */ /* END CUPTI_RESULT_API */

#if defined(__cplusplus)
}
#endif 

#endif /*_CUPTI_RESULT_H_*/


