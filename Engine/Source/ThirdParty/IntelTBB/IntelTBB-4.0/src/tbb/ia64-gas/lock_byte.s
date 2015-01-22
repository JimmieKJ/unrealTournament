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

	// Support for class TinyLock
	.section .text
	.align 16
	// unsigned int __TBB_machine_trylockbyte( byte& flag );
	// r32 = address of flag 
	.proc  __TBB_machine_trylockbyte#
	.global __TBB_machine_trylockbyte#
ADDRESS_OF_FLAG=r32
RETCODE=r8
FLAG=r9
BUSY=r10
SCRATCH=r11
__TBB_machine_trylockbyte:
        ld1.acq FLAG=[ADDRESS_OF_FLAG]
        mov BUSY=1
        mov RETCODE=0
;;
        cmp.ne p6,p0=0,FLAG
        mov ar.ccv=r0
(p6)    br.ret.sptk.many b0
;;
        cmpxchg1.acq SCRATCH=[ADDRESS_OF_FLAG],BUSY,ar.ccv  // Try to acquire lock
;;
        cmp.eq p6,p0=0,SCRATCH
;;
(p6)    mov RETCODE=1
   	br.ret.sptk.many b0	
	.endp __TBB_machine_trylockbyte#
