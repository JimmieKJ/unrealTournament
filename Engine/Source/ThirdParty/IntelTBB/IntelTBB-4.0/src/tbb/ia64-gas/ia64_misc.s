// Copyright 2005-2012 Intel Corporation.  All Rights Reserved.
//
// The source code contained or described herein and all documents related
// to the source code ("Material") are owned by Intel Corporation or its
// suppliers or licensors.  Title to the Material remains with Intel
// Corporation or its suppliers and licensors.  The Material is protected
// by worldwide copyright laws and treaty provisions.  No part of the
// Material may be used, copied, reproduced, modified, published, uploaded,
// posted, transmitted, distributed, or disclosed in any way without
// Intel's prior express written permission.
//
// No license under any patent, copyright, trade secret or other
// intellectual property right is granted to or conferred upon you by
// disclosure or delivery of the Materials, either expressly, by
// implication, inducement, estoppel or otherwise.  Any license under such
// intellectual property rights must be express and approved by Intel in
// writing.

	// RSE backing store pointer retrieval
    .section .text
    .align 16
    .proc __TBB_get_bsp#
    .global __TBB_get_bsp#
__TBB_get_bsp:
        mov r8=ar.bsp
        br.ret.sptk.many b0
    .endp __TBB_get_bsp#

    .section .text
    .align 16
    .proc __TBB_machine_load8_relaxed#
    .global __TBB_machine_load8_relaxed#
__TBB_machine_load8_relaxed:
        ld8 r8=[r32]
        br.ret.sptk.many b0
    .endp __TBB_machine_load8_relaxed#

    .section .text
    .align 16
    .proc __TBB_machine_store8_relaxed#
    .global __TBB_machine_store8_relaxed#
__TBB_machine_store8_relaxed:
        st8 [r32]=r33
        br.ret.sptk.many b0
    .endp __TBB_machine_store8_relaxed#

    .section .text
    .align 16
    .proc __TBB_machine_load4_relaxed#
    .global __TBB_machine_load4_relaxed#
__TBB_machine_load4_relaxed:
        ld4 r8=[r32]
        br.ret.sptk.many b0
    .endp __TBB_machine_load4_relaxed#

    .section .text
    .align 16
    .proc __TBB_machine_store4_relaxed#
    .global __TBB_machine_store4_relaxed#
__TBB_machine_store4_relaxed:
        st4 [r32]=r33
        br.ret.sptk.many b0
    .endp __TBB_machine_store4_relaxed#

    .section .text
    .align 16
    .proc __TBB_machine_load2_relaxed#
    .global __TBB_machine_load2_relaxed#
__TBB_machine_load2_relaxed:
        ld2 r8=[r32]
        br.ret.sptk.many b0
    .endp __TBB_machine_load2_relaxed#

    .section .text
    .align 16
    .proc __TBB_machine_store2_relaxed#
    .global __TBB_machine_store2_relaxed#
__TBB_machine_store2_relaxed:
        st2 [r32]=r33
        br.ret.sptk.many b0
    .endp __TBB_machine_store2_relaxed#

    .section .text
    .align 16
    .proc __TBB_machine_load1_relaxed#
    .global __TBB_machine_load1_relaxed#
__TBB_machine_load1_relaxed:
        ld1 r8=[r32]
        br.ret.sptk.many b0
    .endp __TBB_machine_load1_relaxed#

    .section .text
    .align 16
    .proc __TBB_machine_store1_relaxed#
    .global __TBB_machine_store1_relaxed#
__TBB_machine_store1_relaxed:
        st1 [r32]=r33
        br.ret.sptk.many b0
    .endp __TBB_machine_store1_relaxed#
