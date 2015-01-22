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

	.section .text
	.align 16
	// unsigned long __TBB_machine_lg( unsigned long x );
	// r32 = x
	.proc  __TBB_machine_lg#
	.global __TBB_machine_lg#
__TBB_machine_lg:
        shr r16=r32,1	// .x
;;
        shr r17=r32,2	// ..x
	or r32=r32,r16	// xx
;;
	shr r16=r32,3	// ...xx
	or r32=r32,r17  // xxx
;;
	shr r17=r32,5	// .....xxx
	or r32=r32,r16  // xxxxx
;;
	shr r16=r32,8	// ........xxxxx
	or r32=r32,r17	// xxxxxxxx
;;
	shr r17=r32,13
	or r32=r32,r16	// 13x
;;
	shr r16=r32,21
	or r32=r32,r17	// 21x
;;
	shr r17=r32,34  
	or r32=r32,r16	// 34x
;;
	shr r16=r32,55
	or r32=r32,r17  // 55x
;;
	or r32=r32,r16  // 64x
;;
	popcnt r8=r32
;;
	add r8=-1,r8
   	br.ret.sptk.many b0	
	.endp __TBB_machine_lg#
