/*
 * Copyright 1993-2013 NVIDIA Corporation.  All rights reserved.
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

#ifndef NVVM_H
#define NVVM_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <stdlib.h>


/*****************************//**
 *
 * \defgroup error Error Handling
 *
 ********************************/


/**
 * \ingroup error
 * \brief   NVVM API call result code.
 */
typedef enum {
  NVVM_SUCCESS = 0,
  NVVM_ERROR_OUT_OF_MEMORY = 1,
  NVVM_ERROR_PROGRAM_CREATION_FAILURE = 2,
  NVVM_ERROR_IR_VERSION_MISMATCH = 3,
  NVVM_ERROR_INVALID_INPUT = 4,
  NVVM_ERROR_INVALID_PROGRAM = 5,
  NVVM_ERROR_INVALID_IR = 6,
  NVVM_ERROR_INVALID_OPTION = 7,
  NVVM_ERROR_NO_MODULE_IN_PROGRAM = 8,
  NVVM_ERROR_COMPILATION = 9
} nvvmResult;


/**
 * \ingroup error
 * \brief   Get the message string for the given #nvvmResult code.
 *
 * \param   [in] result NVVM API result code.
 * \return  Message string for the given #nvvmResult code.
 */
const char *nvvmGetErrorString(nvvmResult result);


/****************************************//**
 *
 * \defgroup query General Information Query
 *
 *******************************************/


/**
 * \ingroup query
 * \brief   Get the NVVM version.
 *
 * \param   [out] major NVVM major version number.
 * \param   [out] minor NVVM minor version number.
 * \return 
 *   - \link ::nvvmResult NVVM_SUCCESS \endlink
 *
 */
nvvmResult nvvmVersion(int *major, int *minor);


/********************************//**
 *
 * \defgroup compilation Compilation
 *
 ***********************************/

/**
 * \ingroup compilation
 * \brief   NVVM Program 
 *
 * An opaque handle for a program 
 */
typedef struct _nvvmProgram *nvvmProgram;

/**
 * \ingroup compilation
 * \brief   Create a program, and set the value of its handle to *prog.
 *
 * \param   [in] prog NVVM program. 
 * \return
 *   - \link ::nvvmResult NVVM_SUCCESS \endlink
 *   - \link ::nvvmResult NVVM_ERROR_OUT_OF_MEMORY \endlink
 *   - \link ::nvvmResult NVVM_ERROR_INVALID_PROGRAM \endlink
 *
 * \see     nvvmDestroyProgram()
 */
nvvmResult nvvmCreateProgram(nvvmProgram *prog);


/**
 * \ingroup compilation
 * \brief   Destroy a program.
 *
 * \param    [in] prog NVVM program. 
 * \return
 *   - \link ::nvvmResult NVVM_SUCCESS \endlink
 *   - \link ::nvvmResult NVVM_ERROR_INVALID_PROGRAM \endlink
 *
 * \see     nvvmCreateProgram()
 */
nvvmResult nvvmDestroyProgram(nvvmProgram *prog);


/**
 * \ingroup compilation
 * \brief   Add a module level NVVM IR to a program. 
 *
 * The buffer should contain an NVVM IR module either in the bitcode
 * representation or in the text representation.
 *
 * \param   [in] prog   NVVM program.
 * \param   [in] buffer NVVM IR module in the bitcode or text
 *                      representation.
 * \param   [in] size   Size of the NVVM IR module.
 * \param   [in] name   Name of the NVVM IR module.
 *                      If NULL, "<unnamed>" is used as the name.
 * \return
 *   - \link ::nvvmResult NVVM_SUCCESS \endlink
 *   - \link ::nvvmResult NVVM_ERROR_OUT_OF_MEMORY \endlink
 *   - \link ::nvvmResult NVVM_ERROR_INVALID_INPUT \endlink
 *   - \link ::nvvmResult NVVM_ERROR_INVALID_PROGRAM \endlink
 */
nvvmResult nvvmAddModuleToProgram(nvvmProgram prog, const char *buffer, size_t size, const char *name);


/**
 * \ingroup compilation
 * \brief   Compile the NVVM program.
 *
 * The NVVM IR modules in the program will be linked at the IR level.
 * The linked IR program is compiled to PTX.
 *
 * The target datalayout in the linked IR program is used to
 * determine the address size (32bit vs 64bit).
 *
 * The valid compiler options are:
 *
 *   - -g (enable generation of debugging information)
 *   - -opt=
 *     - 0 (disable optimizations)
 *     - 3 (default, enable optimizations)
 *   - -arch=
 *     - compute_20 (default)
 *     - compute_30
 *     - compute_35
 *   - -ftz=
 *     - 0 (default, preserve denormal values, when performing
 *          single-precision floating-point operations)
 *     - 1 (flush denormal values to zero, when performing
 *          single-precision floating-point operations)
 *   - -prec-sqrt=
 *     - 0 (use a faster approximation for single-precision
 *          floating-point square root)
 *     - 1 (default, use IEEE round-to-nearest mode for
 *          single-precision floating-point square root)
 *   - -prec-div=
 *     - 0 (use a faster approximation for single-precision
 *          floating-point division and reciprocals)
 *     - 1 (default, use IEEE round-to-nearest mode for
 *          single-precision floating-point division and reciprocals)
 *   - -fma=
 *     - 0 (disable FMA contraction)
 *     - 1 (default, enable FMA contraction)
 *
 * \param   [in] prog       NVVM program.
 * \param   [in] numOptions Number of compiler options passed.
 * \param   [in] options    Compiler options in the form of C string array.
 * \return
 *   - \link ::nvvmResult NVVM_SUCCESS \endlink
 *   - \link ::nvvmResult NVVM_ERROR_OUT_OF_MEMORY \endlink
 *   - \link ::nvvmResult NVVM_ERROR_IR_VERSION_MISMATCH \endlink
 *   - \link ::nvvmResult NVVM_ERROR_INVALID_PROGRAM \endlink
 *   - \link ::nvvmResult NVVM_ERROR_INVALID_OPTION \endlink
 *   - \link ::nvvmResult NVVM_ERROR_NO_MODULE_IN_PROGRAM \endlink
 *   - \link ::nvvmResult NVVM_ERROR_COMPILATION \endlink
 */
nvvmResult nvvmCompileProgram(nvvmProgram prog, int numOptions, const char **options);   

/**
 * \ingroup compilation
 * \brief   Verify the NVVM program.
 *
 * The valid compiler options are:
 *
 * NONE. The \p numOptions and \p options parameters are ignored, and are for future use.
 *
 * \param   [in] prog       NVVM program.
 * \param   [in] numOptions Number of compiler options passed.
 * \param   [in] options    Compiler options in the form of C string array.
 * \return
 *   - \link ::nvvmResult NVVM_SUCCESS \endlink
 *   - \link ::nvvmResult NVVM_ERROR_OUT_OF_MEMORY \endlink
 *   - \link ::nvvmResult NVVM_ERROR_IR_VERSION_MISMATCH \endlink
 *   - \link ::nvvmResult NVVM_ERROR_INVALID_PROGRAM \endlink
 *   - \link ::nvvmResult NVVM_ERROR_INVALID_IR \endlink
 *   - \link ::nvvmResult NVVM_ERROR_INVALID_OPTION \endlink
 *   - \link ::nvvmResult NVVM_ERROR_NO_MODULE_IN_PROGRAM \endlink
 */
nvvmResult nvvmVerifyProgram(nvvmProgram prog, int numOptions, const char **options);

/**
 * \ingroup compilation
 * \brief   Get the size of the compiled result.
 *
 * \param   [in]  prog          NVVM program.
 * \param   [out] bufferSizeRet Size of the compiled result (including the
 *                              trailing NULL).
 * \return
 *   - \link ::nvvmResult NVVM_SUCCESS \endlink
 *   - \link ::nvvmResult NVVM_ERROR_INVALID_PROGRAM \endlink
 */
nvvmResult nvvmGetCompiledResultSize(nvvmProgram prog, size_t *bufferSizeRet);


/**
 * \ingroup compilation
 * \brief   Get the compiled result.
 *
 * The result is stored in the memory pointed by 'buffer'.
 *
 * \param   [in]  prog   NVVM program.
 * \param   [out] buffer Compiled result.
 * \return
 *   - \link ::nvvmResult NVVM_SUCCESS \endlink
 *   - \link ::nvvmResult NVVM_ERROR_INVALID_PROGRAM \endlink
 */
nvvmResult nvvmGetCompiledResult(nvvmProgram prog, char *buffer);


/**
 * \ingroup compilation
 * \brief   Get the Size of Compiler/Verifier Message.
 *
 * The size of the message string (including the trailing NULL) is stored into
 * 'buffer_size_ret' when the return value is NVVM_SUCCESS.
 *   
 * \param   [in]  prog          NVVM program.
 * \param   [out] bufferSizeRet Size of the compilation/verification log
                                (including the trailing NULL).
 * \return
 *   - \link ::nvvmResult NVVM_SUCCESS \endlink
 *   - \link ::nvvmResult NVVM_ERROR_INVALID_PROGRAM \endlink
 */
nvvmResult nvvmGetProgramLogSize(nvvmProgram prog, size_t *bufferSizeRet);


/**
 * \ingroup compilation
 * \brief   Get the Compiler/Verifier Message
 *
 * The NULL terminated message string is stored in the memory pointed by
 * 'buffer' when the return value is NVVM_SUCCESS.
 *   
 * \param   [in]  prog   NVVM program program.
 * \param   [out] buffer Compilation/Verification log.
 * \return
 *   - \link ::nvvmResult NVVM_SUCCESS \endlink
 *   - \link ::nvvmResult NVVM_ERROR_INVALID_PROGRAM \endlink
 */
nvvmResult nvvmGetProgramLog(nvvmProgram prog, char *buffer);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* NVVM_H */
